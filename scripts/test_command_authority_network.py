#!/usr/bin/env python3
"""WP-05: three independent engine workers, native command packets over TCP loopback.

This exercises game command serialization, sanitizing receive, and the native
server and client execution queues. It does not exercise the join/map-transfer handshake.
"""

import argparse
import json
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


def policy_state(raw, mode):
    state = json.loads(raw)
    required = {'mode', 'rating', 'money', 'materials', 'economy', 'map', 'rng',
                'crossing', 'foreign_tram', 'demolition', 'competitor_crossing', 'extra_dynamite'}
    if not isinstance(state, dict) or set(state) != required or state['mode'] != mode or state['rating'] != -200:
        raise AssertionError('construction-policy native state is incomplete or has a different mode/rating')
    if state['competitor_crossing'] is not False or state['extra_dynamite'] is not False:
        raise AssertionError('construction-policy fixture must retain both restrictive settings')
    return state


def assert_policy_transition(before_raw, after_raw, command, mode):
    before, after = policy_state(before_raw, mode), policy_state(after_raw, mode)
    for field in ('rating', 'materials', 'economy', 'rng', 'foreign_tram',
                  'competitor_crossing', 'extra_dynamite'):
        if after[field] != before[field]:
            raise AssertionError(f'{command} changed native {field}')
    if after['money'] >= before['money'] or after['map'] == before['map']:
        raise AssertionError(f'{command} lacked a paid native map change')
    if command == 'a':
        if after['demolition'] != before['demolition'] or after['crossing'].get('crossing') is not True:
            raise AssertionError('public road crossing did not construct the native crossing')
        for field in ('tile', 'road_owner', 'tram_owner', 'road_bits'):
            if after['crossing'].get(field) != before['crossing'].get(field):
                raise AssertionError(f'public road crossing changed {field}')
    elif command == 'd':
        if after['crossing'] != before['crossing'] or after['demolition']['type'] == before['demolition']['type']:
            raise AssertionError('connected town road was not cleared by the native command')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("test_binary", help="built openttd_test executable")
    parser.add_argument("--resource-surveys", action="store_true", help="Exercise private surveys and competing industry construction")
    parser.add_argument("--megacity-designation", action="store_true", help="Exercise free live-town designation, invalid and duplicate denial, and MEGA reload")
    parser.add_argument("--construction-policy", choices=("classic", "v1", "v2"),
                        help="Exercise paid public construction permissions in three independent native workers")
    args = parser.parse_args()
    if sum((args.resource_surveys, args.megacity_designation, args.construction_policy is not None)) > 1:
        parser.error("choose one scoped command fixture")
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
            if args.resource_surveys:
                env["OSTTD_RESOURCE_LOOPBACK"] = "1"
            if args.megacity_designation:
                env["OSTTD_MEGACITY_LOOPBACK"] = "1"
            if args.construction_policy is not None:
                env["OSTTD_CONSTRUCTION_POLICY_LOOPBACK"] = {"classic": "0", "v1": "1", "v2": "2"}[args.construction_policy]
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
        policy_mode = {"classic": 0, "v1": 1, "v2": 2}.get(args.construction_policy)
        if policy_mode is not None:
            initial = policy_state(baseline, policy_mode)
            if initial['crossing'].get('crossing') is not False or initial['demolition']['type'] != initial['crossing']['type']:
                raise AssertionError('construction-policy starting roads differ')
        changed = 0

        operations = (
            (1, "D", False),  # company 0 attempts company 1 HQ
            (1, "I", False),  # invalid HQ tier
            (1, "U", True),   # one HQ tier
            (2, "C", False),  # insufficient company funds; money lowered below
            (2, "C", True),   # colony and canonical directory
            (1, "P", True),   # one world phase promotion
            (2, "U", False),  # duplicate HQ target
            (1, "C", False),  # already colonized world
            (2, "V", False),  # unknown world
        )
        if args.resource_surveys:
            operations = ((1, "N", False), (1, "Q", True), (2, "N", False),
                          (2, "J", True), (1, "M", True), (2, "N", False))
        if args.megacity_designation:
            operations = ((1, "F", False), (1, "G", True), (2, "G", False),
                          (2, "X", False), (2, "E", True), (1, "E", False))
        if args.construction_policy is not None:
            integrated = args.construction_policy != "classic"
            operations = ((1, "f", False), (2, "a", integrated),
                          (1, "a", False), (1, "d", integrated))
        for origin, command, success in operations:
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
            if args.construction_policy is not None:
                policy_state(states[0], policy_mode)
                if success:
                    assert_policy_transition(before, states[0], command, policy_mode)
            state = states[0].decode("utf-8")
            if success and args.construction_policy is None:
                required = {
                    "Q": (";survey:0:1950", "company:0:9999000;"),
                    "J": (";survey:1:1950", "company:1:9999000;"),
                    "M": (";industries:1", ";site:1:1950:0"),
                    "U": ("hq:0:2:Authority HQ;", "company:0:10000000;"),
                    "C": ("world:0:3:10100:Relay Outpost:", "company:0:9995000;"),
                    "P": ("world:0:2:10350:Relay Outpost:", "company:0:9987000;"),
                    "G": (";megacity:0:0:Authority settlement:1240:62:31:12", "company:0:10000000;"),
                    "E": (";megacity:1:0:Spectator settlement:1000:50:30:10", "company:0:10000000;"),
                }[command]
                if any(value not in state for value in required):
                    raise AssertionError(f"{command} did not apply exactly one expected change: {state}")
            changed += int(states[0] != before)
            print(f"{command}: {'accepted' if success else 'denied'}; frame state {states[0].decode('utf-8', 'replace')}")

        expected_changes = (0 if args.construction_policy == "classic" else 2) if args.construction_policy is not None else 2 if args.megacity_designation else 3
        if changed != expected_changes or (states[0] == baseline) != (expected_changes == 0):
            raise AssertionError(f"expected exactly {expected_changes} accepted state changes")

        if args.construction_policy is None:
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
        if args.construction_policy is None:
            print(f"save/reload: {reloaded[0].decode('utf-8', 'replace')}")
            print("WP-05 TCP loopback: native server queue and two independent client queues, denial, save/reload equality PASS")
        else:
            print(f"Construction policy {args.construction_policy}: native server queue, two independent client queues, public construction and foreign-tram denial PASS")
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
