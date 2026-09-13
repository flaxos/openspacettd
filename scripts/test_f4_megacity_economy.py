#!/usr/bin/env python3
"""
OpenSpaceTTD Phase F4: Megacity & Empire Economy Integration Spike
Tests Megacity sustained commodity demand cycles, freight corridor congestion,
dynamic delay scaling with priority QoS mitigation, and interplanetary supply chain matrix.
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
    print(" OpenSpaceTTD Phase F4 Megacity & Empire Economy Integration Test")
    print("=" * 70)

    port = 37444
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
        # 2. Megacity Sustained Multi-Tier Demand & Growth Evaluation
        # ---------------------------------------------------------------------
        print("\n[*] Testing Megacity Registration & Demand Quotas...")
        status, city_reg = post_json(f"{base_url}/megacity/register", {
            "town_id": 101,
            "world_id": 1,
            "name": "Metropolis Prime",
            "population": 10000
        })
        assert status == 200, f"Megacity registration failed: {city_reg}"
        assert city_reg["monthly_quota"] == [500, 250, 100] # 10000 / 20, / 40, / 100
        print(f"[+] Megacity registered with dynamic quotas: {city_reg['monthly_quota']}")

        # Test Custom Quota Megacity
        status, custom_city = post_json(f"{base_url}/megacity/register", {
            "town_id": 102,
            "world_id": 1,
            "name": "Nova Capital",
            "population": 5000,
            "custom_quota": [100, 50, 20]
        })
        assert status == 200
        assert custom_city["monthly_quota"] == [100, 50, 20]

        # 2.1 Cycle 1: Starvation (Tier 1 < 50%)
        print("\n[*] Testing Cycle 1: Starvation State...")
        # Deliver 40 food (T1), 50 goods (T2), 20 tech (T3) to Nova Capital (102)
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 0, "amount": 40})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 1, "amount": 50})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 2, "amount": 20})

        status, eval_res = post_json(f"{base_url}/megacity/eval", {"town_id": 102})
        assert status == 200
        city = eval_res[0]
        assert city["growth_state"] == "STARVATION"
        assert city["growth_multiplier"] == 0.0
        assert city["passenger_multiplier"] == 0.5
        assert city["delivered_last"] == [40, 50, 20]
        assert city["delivered_current"] == [0, 0, 0] # Reset for next cycle
        print("[+] Cycle 1 evaluated correctly: STARVATION (Growth 0.0x, Pax 0.5x)")

        # 2.2 Cycle 2: Subsistence (Tier 1 >= 50%, Tier 2 < 100%)
        print("\n[*] Testing Cycle 2: Subsistence State (using cargo type classification)...")
        # Cargo 11 = Food -> Tier 0
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "cargo_type": 11, "amount": 80})
        # Cargo 5 = Goods -> Tier 1
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "cargo_type": 5, "amount": 20})
        # Cargo 10 = Valuables/Diamonds -> Tier 2
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "cargo_type": 10, "amount": 20})

        status, eval_res = post_json(f"{base_url}/megacity/eval", {"town_id": 102})
        city = eval_res[0]
        assert city["growth_state"] == "SUBSISTENCE"
        assert city["growth_multiplier"] == 1.0
        assert city["passenger_multiplier"] == 1.0
        print("[+] Cycle 2 evaluated correctly: SUBSISTENCE (Growth 1.0x, Pax 1.0x)")

        # 2.3 Cycle 3: Metropolitan Boom (Tier 1 >= 100%, Tier 2 >= 100%, Tier 3 < 100%)
        print("\n[*] Testing Cycle 3: Metropolitan Boom State...")
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 0, "amount": 120})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 1, "amount": 60})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 2, "amount": 10})

        status, eval_res = post_json(f"{base_url}/megacity/eval", {"town_id": 102})
        city = eval_res[0]
        assert city["growth_state"] == "METROPOLITAN_BOOM"
        assert city["growth_multiplier"] == 1.5
        assert city["passenger_multiplier"] == 1.25
        print("[+] Cycle 3 evaluated correctly: METROPOLITAN_BOOM (Growth 1.5x, Pax 1.25x)")

        # 2.4 Cycle 4: HyperGrowth (All 3 Tiers >= 100%)
        print("\n[*] Testing Cycle 4: HyperGrowth State...")
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 0, "amount": 100})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 1, "amount": 50})
        post_json(f"{base_url}/megacity/demand", {"town_id": 102, "tier": 2, "amount": 25})

        status, eval_res = post_json(f"{base_url}/megacity/eval", {"town_id": 102})
        city = eval_res[0]
        assert city["growth_state"] == "HYPER_GROWTH"
        assert city["growth_multiplier"] == 2.0
        assert city["passenger_multiplier"] == 1.5
        print("[+] Cycle 4 evaluated correctly: HYPER_GROWTH (Growth 2.0x, Pax 1.5x)")

        # Verify Megacity Status Query Endpoint
        status, status_res = get_json(f"{base_url}/megacity/status?town_id=102")
        assert status == 200 and len(status_res) == 1
        assert status_res[0]["name"] == "Nova Capital"

        # ---------------------------------------------------------------------
        # 3. Freight Corridors, Congestion Escalation & Priority QoS
        # ---------------------------------------------------------------------
        print("\n[*] Registering Worlds for Freight Corridors & Empire Economy...")
        post_json(f"{base_url}/directory/register", {
            "world_id": 1, "phase": 3, "name": "Frontier Outpost 1", "address": "127.0.0.1:3980"
        })
        post_json(f"{base_url}/directory/register", {
            "world_id": 2, "phase": 2, "name": "Refinery Complex 2", "address": "127.0.0.1:3981"
        })
        post_json(f"{base_url}/directory/register", {
            "world_id": 3, "phase": 1, "name": "Core Metropolis 3", "address": "127.0.0.1:3982"
        })

        print("\n[*] Testing Freight Corridor Limits & Congestion Scaling...")
        status, corr_reg = post_json(f"{base_url}/corridors/register", {
            "route_id": "CORR-1-2",
            "source_world": 1,
            "source_gate": 10,
            "dest_world": 2,
            "dest_gate": 20,
            "transit_delay_sec": 4.0,
            "max_bandwidth_trains_per_min": 10,
            "max_active_in_transit": 4
        })
        assert status == 200, f"Corridor registration failed: {corr_reg}"

        # Check Corridor List Endpoint
        status, corrs = get_json(f"{base_url}/corridors/list")
        assert status == 200 and len(corrs) >= 1

        # Dispatch 1: Active = 1 / 4 (25%) -> CLEAR (1.0x = 4.0s)
        status, tx1 = post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "source_gate": 10, "dest_gate": 20,
            "total_cargo": 50, "cargo_breakdown": {"8": 50}, "priority": "STANDARD"
        })
        assert status == 200
        assert tx1["transit_delay_sec"] == 4.0
        print("[+] Train 1 (25% capacity): Congestion CLEAR, Delay = 4.0s")

        # Dispatch 2: Active = 2 / 4 (50%) -> MODERATE (1.2x = 4.8s)
        status, tx2 = post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "source_gate": 10, "dest_gate": 20,
            "total_cargo": 50, "cargo_breakdown": {"8": 50}, "priority": "STANDARD"
        })
        assert status == 200
        assert abs(tx2["transit_delay_sec"] - 4.8) < 0.001
        print("[+] Train 2 (50% capacity): Congestion MODERATE, Delay = 4.8s")

        # Dispatch 3: Active = 3 / 4 (75%) -> MODERATE
        # Test Express QoS: Penalty (0.2) halved to 0.1 -> 1.1x = 4.4s
        status, tx3 = post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "source_gate": 10, "dest_gate": 20,
            "total_cargo": 50, "cargo_breakdown": {"8": 50}, "priority": "EXPRESS"
        })
        assert status == 200
        assert abs(tx3["transit_delay_sec"] - 4.4) < 0.001
        print("[+] Train 3 (75% capacity, EXPRESS QoS): Congestion MODERATE, Delay = 4.4s (50% penalty relief)")

        # Dispatch 4: Active = 4 / 4 (100%) -> CONGESTED (1.5x = 6.0s)
        status, tx4 = post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "source_gate": 10, "dest_gate": 20,
            "total_cargo": 50, "cargo_breakdown": {"8": 50}, "priority": "STANDARD"
        })
        assert status == 200
        assert abs(tx4["transit_delay_sec"] - 6.0) < 0.001
        print("[+] Train 4 (100% capacity): Congestion CONGESTED, Delay = 6.0s")

        # Dispatch 5: Active = 5 / 4 (125%) -> SATURATED (2.0x base penalty)
        # Test PriorityUrgent: Penalty (1.0) halved to 0.5 -> 1.5x = 6.0s
        status, tx5 = post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "source_gate": 10, "dest_gate": 20,
            "total_cargo": 50, "cargo_breakdown": {"8": 50}, "priority": "PRIORITY_URGENT"
        })
        assert status == 200
        assert abs(tx5["transit_delay_sec"] - 6.0) < 0.001
        print("[+] Train 5 (125% capacity, PRIORITY_URGENT QoS): Congestion SATURATED, Delay = 6.0s (50% penalty relief)")

        # Confirm arrivals step by step and verify congestion relief
        for tx in [tx1, tx2, tx3, tx4, tx5]:
            post_json(f"{base_url}/transfers/depart", {"transfer_id": tx["transfer_id"]})
            post_json(f"{base_url}/transfers/claim", {"transfer_id": tx["transfer_id"], "dest_world": 2})
            post_json(f"{base_url}/transfers/confirm", {"transfer_id": tx["transfer_id"], "dest_world": 2, "success": True})

        # Corridor should be back to CLEAR
        status, corrs = get_json(f"{base_url}/corridors/list")
        corr = next(c for c in corrs if c["route_id"] == "CORR-1-2")
        assert corr["current_in_transit_count"] == 0
        assert corr["congestion_level"] == "CLEAR"
        print("[+] All trains arrived. Corridor congestion relieved back to CLEAR (0 in-transit).")

        # ---------------------------------------------------------------------
        # 4. Multi-Planet Empire Supply Chain Matrix & Tariffs
        # ---------------------------------------------------------------------
        print("\n[*] Testing Multi-Planet Empire Supply Chain Matrix...")
        # Flow A: Phase 3 -> Phase 2 (Raw ore) = 100
        post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 2, "total_cargo": 100, "cargo_breakdown": {"8": 100}
        })
        # Flow B: Phase 2 -> Phase 1 (Refined steel) = 70
        post_json(f"{base_url}/transfers/initiate", {
            "source_world": 2, "dest_world": 3, "total_cargo": 70, "cargo_breakdown": {"9": 70}
        })
        # Flow C: Phase 3 -> Phase 1 (Direct sustenance food) = 90
        post_json(f"{base_url}/transfers/initiate", {
            "source_world": 1, "dest_world": 3, "total_cargo": 90, "cargo_breakdown": {"11": 90}
        })
        # Flow D: Phase 1 -> Phase 1/3 (Core high-tech export) = 40
        post_json(f"{base_url}/transfers/initiate", {
            "source_world": 3, "dest_world": 1, "total_cargo": 40, "cargo_breakdown": {"10": 40}
        })

        status, matrix = get_json(f"{base_url}/economy/matrix")
        assert status == 200
        assert matrix["frontier_to_refinery_cargo"] >= 100
        assert matrix["refinery_to_core_cargo"] >= 70
        assert matrix["frontier_to_core_cargo"] >= 90
        assert matrix["core_export_cargo"] >= 40
        assert matrix["total_tariffs_generated"] >= (300 * 10)
        print(f"[+] Empire Supply Chain Matrix Verified:")
        print(f"    - Frontier -> Refinery: {matrix['frontier_to_refinery_cargo']} units")
        print(f"    - Refinery -> Core:     {matrix['refinery_to_core_cargo']} units")
        print(f"    - Frontier -> Core:     {matrix['frontier_to_core_cargo']} units")
        print(f"    - Core Export:          {matrix['core_export_cargo']} units")
        print(f"    - Total Tariffs:        {matrix['total_tariffs_generated']} Cr")

        # ---------------------------------------------------------------------
        # 5. Commodity Conservation Ledger Audit
        # ---------------------------------------------------------------------
        print("\n[*] Verifying Interplanetary Commodity Conservation Ledger...")
        status, audit = get_json(f"{base_url}/ledger/status")
        assert status == 200
        assert audit["is_conserved"] is True, f"Conservation invariant violated: {audit}"
        print(f"[+] Total Cargo Initiated: {audit['total_cargo_initiated']}")
        print(f"[+] Total Cargo Completed: {audit['total_cargo_completed']}")
        print(f"[+] Total Cargo In Transit: {audit['total_cargo_in_transit']}")
        print(f"[+] Commodity Conservation Invariant Confirmed: {audit['is_conserved']}")

        status, detailed_audit = get_json(f"{base_url}/ledger/audit_detailed")
        assert status == 200
        assert detailed_audit["all_conserved"] is True
        print(f"[+] Detailed Per-Cargo-Type Conservation Confirmed across {detailed_audit['total_commodities_tracked']} cargo types.")

        print("\n" + "=" * 70)
        print(" PHASE F4 INTEGRATION TEST COMPLETED SUCCESSFULLY!")
        print("=" * 70)

    finally:
        print("[*] Terminating Universe Authority daemon...")
        daemon.terminate()
        try:
            daemon.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            daemon.kill()

if __name__ == "__main__":
    main()
