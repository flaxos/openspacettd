#!/usr/bin/env python3
"""WP-05: three independent engine workers, native command packets over TCP loopback.

This exercises game command serialization, sanitizing receive, and the native
server and client execution queues. It does not exercise the join/map-transfer handshake.
"""

import argparse
import os
import socket
import struct
import subprocess
import sys

CASE = "WP-05 isolated loopback command worker"
LIMIT = 65536


def exact(sock, count):
    out = bytearray()
    while len(out) < count:
        part = sock.recv(count - len(out))
        if not part:
            raise RuntimeError("worker closed loopback socket")
        out.extend(part)
    return bytes(out)


def send(sock, op, payload=b""):
    sock.sendall(op.encode("ascii") + struct.pack("<I", len(payload)) + payload)


def receive(sock, expected):
    header = exact(sock, 5)
    op = chr(header[0])
    size = struct.unpack("<I", header[1:])[0]
    if size > LIMIT or op != expected:
        raise RuntimeError(f"expected {expected}, received {op} with {size} bytes")
    return exact(sock, size)


def assert_equal(states, label):
    if len(set(states)) != 1:
        raise AssertionError(f"{label}: server and clients diverged: {states!r}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("test_binary", help="built openttd_test executable")
    args = parser.parse_args()
    workers = []
    sockets = []
    try:
        for role in ("server", "client1", "client2"):
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            parent = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            parent.connect(listener.getsockname())
            child, _ = listener.accept()
            listener.close()
            parent.settimeout(15)
            env = os.environ.copy()
            env["OSTTD_AUTHORITY_LOOPBACK_FD"] = str(child.fileno())
            env["OSTTD_AUTHORITY_LOOPBACK_ROLE"] = role
            process = subprocess.Popen(
                [args.test_binary, CASE],
                env=env,
                pass_fds=(child.fileno(),),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            child.close()
            workers.append(process)
            sockets.append(parent)

        states = [receive(sock, "S") for sock in sockets]
        assert_equal(states, "initial fixture")
        baseline = states[0]
        changed = 0

        for origin, command, success in (
            (1, "D", False),  # company 0 attempts company 1 HQ
            (1, "I", False),  # invalid HQ tier
            (1, "U", True),   # one HQ tier
            (2, "C", False),  # insufficient company funds; money lowered below
            (2, "C", True),   # colony and canonical directory
            (1, "P", True),   # one world phase promotion
            (2, "U", False),  # duplicate HQ target
            (1, "C", False),  # already colonized world
            (2, "V", False),  # unknown world
        ):
            if command == "C" and not success and states[0] != baseline and changed == 1:
                # Set the same low cash balance in all independent fixtures.
                for sock in sockets:
                    send(sock, "L")
                states = [receive(sock, "S") for sock in sockets]
                assert_equal(states, "low funds setup")
            if command == "C" and success:
                for sock in sockets:
                    send(sock, "H")
                states = [receive(sock, "S") for sock in sockets]
                assert_equal(states, "funds restored")
            before = states[0]
            send(sockets[origin], command)
            packet = receive(sockets[origin], "K")
            if not packet or packet[0:2] != struct.pack("<H", len(packet)):
                raise AssertionError("origin did not serialize a native game packet")
            # The command has left its origin, but no worker may have applied it yet.
            for sock in sockets:
                send(sock, "S")
            states = [receive(sock, "S") for sock in sockets]
            assert_equal(states, f"pre-broadcast {command}")
            if states[0] != before:
                raise AssertionError(f"origin eagerly mutated on {command}")

            # The server accepts and sanitizes the originating packet, schedules
            # it through NetworkSendCommand/NetworkDistributeCommands, executes
            # its native server queue, then returns the canonical payload.
            send(sockets[0], "B", packet)
            canonical = receive(sockets[0], "W")
            server_state = receive(sockets[0], "S")
            for sock in sockets[1:]:
                send(sock, "B", canonical)
            states = [server_state] + [receive(sock, "S") for sock in sockets[1:]]
            assert_equal(states, f"post-broadcast {command}")
            if success and states[0] == before:
                raise AssertionError(f"successful {command} made no state change")
            if not success and states[0] != before:
                raise AssertionError(f"denied {command} changed state")
            state = states[0].decode("utf-8")
            if success:
                required = {
                    "U": ("hq:0:2:Authority HQ;", "company:0:10000000;"),
                    "C": ("world:0:3:10100:Relay Outpost:", "company:0:9995000;"),
                    "P": ("world:0:2:10350:Relay Outpost:", "company:0:9987000;"),
                }[command]
                if any(value not in state for value in required):
                    raise AssertionError(f"{command} did not apply exactly one expected change: {state}")
            changed += int(states[0] != before)
            print(f"{command}: {'accepted' if success else 'denied'}; frame state {states[0].decode('utf-8', 'replace')}")

        if changed != 3 or states[0] == baseline:
            raise AssertionError("expected exactly HQ upgrade, colonization, promotion")

        for sock in sockets:
            send(sock, "R")
        reloaded = [receive(sock, "S") for sock in sockets]
        assert_equal(reloaded, "save/reload")
        if reloaded[0] != states[0]:
            raise AssertionError("save/reload changed canonical authority state")

        for sock in sockets:
            send(sock, "Z")
        for role, process in zip(("server", "client1", "client2"), workers):
            output, _ = process.communicate(timeout=15)
            if process.returncode != 0:
                raise RuntimeError(f"{role} failed ({process.returncode}):\n{output}")
        print(f"save/reload: {reloaded[0].decode('utf-8', 'replace')}")
        print("WP-05 TCP loopback: native server queue and two independent client queues, denial, save/reload equality PASS")
    finally:
        for sock in sockets:
            sock.close()
        for process in workers:
            if process.poll() is None:
                process.terminate()
                try:
                    output, _ = process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    output, _ = process.communicate()
                if output:
                    print(f"interrupted worker output:\n{output[-10000:]}", file=sys.stderr)
            elif process.stdout is not None and not process.stdout.closed:
                remaining = process.stdout.read()
                if remaining and process.returncode != 0:
                    print(f"worker output:\n{remaining}", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"WP-05 loopback FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
