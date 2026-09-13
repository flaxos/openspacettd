#!/usr/bin/env python3
"""
OpenSpaceTTD Multi-Process Federation Spike (Phase F2 Prototype)
Automated end-to-end integration test validating the Universe Authority daemon,
consist transfer lifecycle, commodity conservation ledger, and headless server invocation.
"""

import json
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request

def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

def http_json_request(url, method="GET", data=None):
    req = urllib.request.Request(url, method=method)
    req.add_header("Content-Type", "application/json")
    body = json.dumps(data).encode("utf-8") if data is not None else None
    try:
        with urllib.request.urlopen(req, data=body, timeout=5) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8")
        try:
            return e.code, json.loads(err_body)
        except Exception:
            return e.code, {"error": err_body}

def main():
    print("=" * 70)
    print(" OpenSpaceTTD Federation F2 Integration Test Spike")
    print("=" * 70)

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    authority_script = os.path.join(repo_root, "scripts", "universe_authority.py")
    openttd_bin = os.path.join(repo_root, "build", "openttd")

    if not os.path.exists(openttd_bin):
        print(f"[ERROR] openttd binary not found at {openttd_bin}")
        return 1

    auth_port = find_free_port()
    base_url = f"http://127.0.0.1:{auth_port}"

    print(f"[*] Starting Universe Authority daemon on {base_url}...")
    auth_proc = subprocess.Popen(
        [sys.executable, authority_script, "--host", "127.0.0.1", "--port", str(auth_port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    try:
        # 1. Wait for Universe Authority to start
        ready = False
        for _ in range(30):
            time.sleep(0.1)
            try:
                status, data = http_json_request(f"{base_url}/ledger/status")
                if status == 200:
                    ready = True
                    break
            except Exception:
                pass

        if not ready:
            print("[ERROR] Failed to connect to Universe Authority daemon")
            return 1
        print("[+] Universe Authority daemon is responsive.")

        # 2. Register World 1 (Sol-Prime) and World 2 (Mars-Colony)
        print("\n[*] Registering participating worlds...")
        status, res = http_json_request(f"{base_url}/worlds/register", method="POST", data={
            "world_id": 1,
            "phase": 1,
            "name": "Sol-Prime",
            "manifest_token": "manifest-token-sol",
        })
        assert status == 200, f"World 1 registration failed: {res}"

        status, res = http_json_request(f"{base_url}/worlds/register", method="POST", data={
            "world_id": 2,
            "phase": 3,
            "name": "Mars-Colony",
            "manifest_token": "manifest-token-mars",
        })
        assert status == 200, f"World 2 registration failed: {res}"

        status, worlds = http_json_request(f"{base_url}/worlds")
        assert status == 200 and len(worlds) == 2, f"Unexpected worlds list: {worlds}"
        print(f"[+] Successfully registered 2 worlds: {[w['name'] for w in worlds]}")

        # 3. Register inter-server portal route
        print("\n[*] Registering inter-server wormhole route...")
        status, route = http_json_request(f"{base_url}/portal_links", method="POST", data={
            "route_id": 1,
            "source_world": 1,
            "source_gate": 10,
            "dest_world": 2,
            "dest_gate": 20,
            "transit_delay_sec": 0.5,
        })
        assert status == 200, f"Route registration failed: {route}"
        print(f"[+] Route registered: World {route['source_world']} Gate {route['source_gate']} -> World {route['dest_world']} Gate {route['dest_gate']}")

        # 4. Initiate consist transfer with cargo
        print("\n[*] Initiating consist departure on Server 1...")
        transfer_payload = {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 10,
            "dest_gate": 20,
            "snapshot_base64": "U1BSTlQyAAAA...",
            "transit_delay_sec": 0.5,
            "total_cargo": 160,
        }
        status, tx = http_json_request(f"{base_url}/transfers/initiate", method="POST", data=transfer_payload)
        assert status == 200, f"Initiate transfer failed: {tx}"
        tx_id = tx["transfer_id"]
        assert tx["state"] == "LOCKED"
        print(f"[+] Transfer initiated with ID: {tx_id} (State: LOCKED, Cargo: {tx['total_cargo']})")

        # 5. Check premature claim fails
        status, claim_res = http_json_request(f"{base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx_id,
            "dest_world": 2,
        })
        assert status == 400, "Premature claim should have been rejected"
        print("[+] Premature claim rejected correctly.")

        # 6. Depart transfer
        print(f"\n[*] Consist departing portal throat on Server 1...")
        status, depart_res = http_json_request(f"{base_url}/transfers/depart", method="POST", data={
            "transfer_id": tx_id,
        })
        assert status == 200 and depart_res["state"] == "IN_TRANSIT"
        print(f"[+] Consist departed. Status: IN_TRANSIT (Wormhole crossing)")

        # 7. Query pending before arrival
        status, pending_early = http_json_request(f"{base_url}/transfers/pending?dest_world=2")
        assert status == 200
        print(f"[+] Immediate pending query for World 2: {len(pending_early['pending_transfers'])} ready (expect 0)")

        # 8. Wait for transit duration (0.5s)
        time.sleep(0.6)

        # 9. Query pending at destination server
        status, pending_ready = http_json_request(f"{base_url}/transfers/pending?dest_world=2")
        assert status == 200 and tx_id in pending_ready["pending_transfers"]
        print(f"[+] Consist reached destination wormhole boundary! Pending query for World 2: {pending_ready['pending_transfers']}")

        # 10. Claim transfer on destination server
        print(f"\n[*] Server 2 claiming transfer {tx_id}...")
        status, claim_res = http_json_request(f"{base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx_id,
            "dest_world": 2,
        })
        assert status == 200 and claim_res["state"] == "ARRIVAL_PENDING"
        print(f"[+] Transfer claimed. Status: ARRIVAL_PENDING (Preparing materialization)")

        # 11. Prevent double claim
        status, dup_claim = http_json_request(f"{base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx_id,
            "dest_world": 2,
        })
        assert status == 400, "Duplicate claim should have been rejected"
        print("[+] Duplicate claim prevented successfully.")

        # 12. Confirm successful emergence and materialization
        print(f"\n[*] Server 2 confirming consist materialization...")
        status, confirm_res = http_json_request(f"{base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx_id,
            "dest_world": 2,
            "success": True,
        })
        assert status == 200 and confirm_res["state"] == "COMPLETED"
        print(f"[+] Transfer confirmed! Status: COMPLETED")

        # 13. Audit Commodity Conservation Ledger
        print(f"\n[*] Verifying Commodity Conservation Ledger invariants...")
        status, audit = http_json_request(f"{base_url}/ledger/status")
        assert status == 200
        print(f"    - Transfers Initiated: {audit['total_transfers_initiated']}")
        print(f"    - Transfers Completed: {audit['total_transfers_completed']}")
        print(f"    - Cargo Initiated:     {audit['total_cargo_initiated']}")
        print(f"    - Cargo Completed:     {audit['total_cargo_completed']}")
        print(f"    - Cargo In Transit:    {audit['total_cargo_in_transit']}")
        print(f"    - Strict Conservation: {audit['is_conserved']}")
        assert audit["is_conserved"] is True, "Commodity conservation invariant violated!"
        assert audit["total_cargo_completed"] == 160, "Cargo completed does not match initiated!"
        assert audit["total_cargo_in_transit"] == 0, "Cargo unexpectedly left in transit!"
        print("[+] Commodity Conservation Invariant PASS: 0 cargo loss, 0 cargo duplication.")

        # 14. Verify OpenSpaceTTD headless executable responds correctly
        print(f"\n[*] Verifying openttd executable flags...")
        res = subprocess.run([openttd_bin, "-h"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5)
        assert res.returncode == 0, f"openttd -h returned {res.returncode}"
        assert "OpenTTD" in res.stdout or "OpenSpaceTTD" in res.stdout
        print("[+] openttd binary verified (-h help output responsive).")

        print("\n" + "=" * 70)
        print(" ALL FEDERATION F2 PROTOCOL SPIKE TESTS PASSED SUCCESSFULLY!")
        print("=" * 70)
        return 0

    finally:
        auth_proc.terminate()
        try:
            auth_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            auth_proc.kill()

if __name__ == "__main__":
    sys.exit(main())
