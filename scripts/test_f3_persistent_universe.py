#!/usr/bin/env python3
"""
OpenSpaceTTD Phase F3: Persistent Universe & Corporate Ledger Integration Spike
Tests player authentication, corporate charters, dynamic world directory,
multi-cargo transfer handoffs, and strict per-cargo commodity conservation.
"""

import json
import os
import subprocess
import sys
import time
import urllib.request
import urllib.error

def post_json(url, payload):
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8"))

def get_json(url):
    try:
        with urllib.request.urlopen(url) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8"))

def main():
    print("=" * 70)
    print(" OpenSpaceTTD Federation F3 Persistent Universe Integration Test")
    print("=" * 70)

    port = 37333
    base_url = f"http://127.0.0.1:{port}"
    script_path = os.path.join(os.path.dirname(__file__), "universe_authority.py")

    # 1. Start Universe Authority daemon
    print(f"[*] Starting Universe Authority daemon on {base_url}...")
    daemon = subprocess.Popen([sys.executable, script_path, "--port", str(port)])
    time.sleep(0.6)

    try:
        # Test responsiveness
        status, res = get_json(f"{base_url}/directory/worlds")
        assert status == 200, f"Daemon not responsive: {res}"
        print("[+] Universe Authority daemon online.")

        # ---------------------------------------------------------------------
        # 2. Player Account Registration & Authentication
        # ---------------------------------------------------------------------
        print("\n[*] Testing Player Registration & Authentication...")
        status, alice_reg = post_json(f"{base_url}/auth/register", {
            "username": "alice",
            "display_name": "Alice Vanguard",
            "password_hash": "hash_alice_123"
        })
        assert status == 200, f"Registration failed: {alice_reg}"
        alice_id = alice_reg["player_id"]
        alice_token = alice_reg["token"]
        print(f"[+] Registered Alice: {alice_id} (Token: {alice_token[:8]}...)")

        # Duplicate username should fail
        status, dup_reg = post_json(f"{base_url}/auth/register", {
            "username": "alice"
        })
        assert status == 400, "Expected duplicate registration to fail"
        print("[+] Duplicate username registration rejected as expected.")

        # Bob registration
        status, bob_reg = post_json(f"{base_url}/auth/register", {
            "username": "bob",
            "display_name": "Bob Logistics",
            "password_hash": "hash_bob_456"
        })
        assert status == 200
        bob_id = bob_reg["player_id"]

        # Alice login
        status, alice_login = post_json(f"{base_url}/auth/login", {
            "username": "alice",
            "password_hash": "hash_alice_123"
        })
        assert status == 200 and alice_login["player_id"] == alice_id
        print(f"[+] Alice authenticated successfully.")

        # Invalid login
        status, bad_login = post_json(f"{base_url}/auth/login", {
            "username": "alice",
            "password_hash": "wrong_password"
        })
        assert status == 401, "Expected invalid password to fail"
        print("[+] Bad password rejected.")

        # ---------------------------------------------------------------------
        # 3. Corporate Charters & Multi-World Ownership
        # ---------------------------------------------------------------------
        print("\n[*] Testing Corporate Charters & Multi-World Ownership...")
        status, corp_res = post_json(f"{base_url}/companies/register", {
            "owner_player_id": alice_id,
            "company_name": "Trans-Galactic Haulage",
            "home_world_id": 1,
            "treasury_balance": 1500000
        })
        assert status == 200, f"Company registration failed: {corp_res}"
        charter_id = corp_res["charter_id"]
        print(f"[+] Chartered Company: {charter_id} ('{corp_res['company_name']}')")
        assert corp_res["treasury_balance"] == 1500000
        assert 1 in corp_res["world_presences"]

        # Add presence on Mars (world 2)
        status, pres_res = post_json(f"{base_url}/companies/presence", {
            "charter_id": charter_id,
            "world_id": 2
        })
        assert status == 200
        assert 2 in pres_res["world_presences"]
        print(f"[+] Expanded presence of {charter_id} to World 2 (Presences: {pres_res['world_presences']})")

        # List companies
        status, comp_list = get_json(f"{base_url}/companies/list?owner={alice_id}")
        assert status == 200 and len(comp_list) == 1
        print(f"[+] Queried corporate charters for owner {alice_id}: {len(comp_list)} found.")

        # ---------------------------------------------------------------------
        # 4. Dynamic World Directory & Heartbeats
        # ---------------------------------------------------------------------
        print("\n[*] Testing Dynamic World Directory & Heartbeats...")
        # Register Earth (World 1) and Mars (World 2)
        post_json(f"{base_url}/directory/register", {
            "world_id": 1,
            "phase": 1,
            "name": "Earth Hub",
            "address": "127.0.0.1:3979",
            "description": "Core Industrial Capital",
            "active_clients": 5,
            "max_clients": 32,
            "active_trains": 35,
            "status": "online"
        })
        post_json(f"{base_url}/directory/register", {
            "world_id": 2,
            "phase": 3,
            "name": "Mars Outpost",
            "address": "127.0.0.1:3980",
            "description": "Frontier Mining Colony",
            "active_clients": 2,
            "max_clients": 16,
            "active_trains": 12,
            "status": "online"
        })

        # Update heartbeat on Mars
        status, hb_res = post_json(f"{base_url}/directory/heartbeat", {
            "world_id": 2,
            "active_clients": 3,
            "active_trains": 14,
            "status": "online"
        })
        assert status == 200 and hb_res["active_clients"] == 3
        print("[+] World heartbeat updated successfully.")

        # Query directory with min_phase filter
        status, all_worlds = get_json(f"{base_url}/directory/worlds")
        assert status == 200 and len(all_worlds) == 2

        status, frontier_worlds = get_json(f"{base_url}/directory/worlds?min_phase=3")
        assert status == 200 and len(frontier_worlds) == 1 and frontier_worlds[0]["name"] == "Mars Outpost"
        print(f"[+] Dynamic world directory verified (Total: {len(all_worlds)}, Frontier: {len(frontier_worlds)}).")

        # ---------------------------------------------------------------------
        # 5. Multi-Commodity Transfer & Conservation Accounting
        # ---------------------------------------------------------------------
        print("\n[*] Testing Multi-Commodity Transfer & Strict Conservation Ledger...")
        # Register inter-server route
        post_json(f"{base_url}/portal_links", {
            "route_id": "route-earth-mars",
            "source_world": 1,
            "source_gate": 101,
            "dest_world": 2,
            "dest_gate": 201,
            "transit_delay_sec": 0.5
        })

        # Multi-cargo consist:
        # Cargo 0 (Coal): 70 units
        # Cargo 1 (Iron Ore): 50 units
        # Cargo 2 (Steel): 30 units
        # Total = 150 units, Valuation = 45,000 Credits
        transfer_payload = {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 101,
            "dest_gate": 201,
            "transit_delay_sec": 0.5,
            "cargo_breakdown": {
                "0": 70,
                "1": 50,
                "2": 30
            },
            "valuation_credits": 45000
        }

        status, init_tx = post_json(f"{base_url}/transfers/initiate", transfer_payload)
        assert status == 200
        tx_id = init_tx["transfer_id"]
        print(f"[+] Transfer initiated: {tx_id} (Total Cargo: {init_tx['total_cargo']}, Valuation: 45,000 Cr)")

        # Verify mid-flight audit
        status, detailed_mid = get_json(f"{base_url}/ledger/audit_detailed")
        assert status == 200 and detailed_mid["all_conserved"] == True
        print(f"[+] Mid-transfer conservation audit: ALL CONSERVED ({detailed_mid['total_commodities_tracked']} cargo types).")

        # Depart
        post_json(f"{base_url}/transfers/depart", {"transfer_id": tx_id})

        # Wait for transit
        time.sleep(0.6)

        # Claim
        status, claim_res = post_json(f"{base_url}/transfers/claim", {
            "transfer_id": tx_id,
            "dest_world": 2
        })
        assert status == 200 and claim_res["state"] == "ARRIVAL_PENDING"

        # Confirm arrival
        status, conf_res = post_json(f"{base_url}/transfers/confirm", {
            "transfer_id": tx_id,
            "dest_world": 2,
            "success": True
        })
        assert status == 200 and conf_res["state"] == "COMPLETED"
        print(f"[+] Transfer completed and emerged on Mars.")

        # ---------------------------------------------------------------------
        # 6. Detailed Conservation Audit & Trade Balances
        # ---------------------------------------------------------------------
        print("\n[*] Verifying Final Commodity Audit & Trade Balances...")
        status, final_audit = get_json(f"{base_url}/ledger/audit_detailed")
        assert status == 200
        assert final_audit["all_conserved"] == True, f"Conservation violation: {final_audit}"

        for rep in final_audit["commodity_reports"]:
            ctype = rep["cargo_type"]
            init = rep["initiated"]
            comp = rep["completed"]
            transit = rep["in_transit"]
            assert rep["conserved"] == True
            assert init == comp + transit
            print(f"    - Cargo Type {ctype}: Initiated={init}, In-Transit={transit}, Completed={comp} [CONSERVED]")

        print("[+] Strict Commodity Conservation Invariant PASS across all commodities.")

        # Verify Inter-World Trade Balances
        status, trade_data = get_json(f"{base_url}/ledger/trade_balance")
        assert status == 200
        wb = {w["world_id"]: w for w in trade_data["world_balances"]}

        assert 1 in wb and 2 in wb
        assert wb[1]["total_exported"] == 150
        assert wb[1]["total_imported"] == 0
        assert wb[1]["net_credits"] == 45000

        assert wb[2]["total_imported"] == 150
        assert wb[2]["total_exported"] == 0
        assert wb[2]["net_credits"] == -45000
        print(f"[+] Inter-world trade balances verified: Earth net +45,000 Cr, Mars net -45,000 Cr.")

        # ---------------------------------------------------------------------
        # 7. Engine Executable Verification
        # ---------------------------------------------------------------------
        print("\n[*] Verifying OpenSpaceTTD binary...")
        ot_res = subprocess.run(["./build/openttd", "-h"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        assert ot_res.returncode == 0
        assert "OpenTTD" in ot_res.stdout
        print("[+] openttd binary verified responsive.")

        print("\n" + "=" * 70)
        print(" ALL FEDERATION F3 PERSISTENT UNIVERSE TESTS PASSED SUCCESSFULLY!")
        print("=" * 70)

    finally:
        print("[*] Terminating Universe Authority daemon...")
        daemon.terminate()
        daemon.wait()
        print("[+] Daemon shutdown complete.")

if __name__ == "__main__":
    main()
