#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path("/home/flax/games/openspacettd").resolve()
sys.path.insert(0, str(ROOT / "scripts"))
from test_wp11_slice import OfflineEngine

SOURCE_SAV = Path("/home/flax/Documents/Codex/2026-10-05/task/core-helper-evidence/retained-final-checkpoint.sav")
EXPECTED_SOURCE_SHA256 = "89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f"

SUMMARY_PATH = Path("/home/flax/Documents/Codex/2026-10-05/task/core-helper-evidence/verified-retained-summary.json")

WORK_DIR = ROOT / "build" / "core-basket-repeat"
WORK_DIR.mkdir(parents=True, exist_ok=True)

def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main():
    print(f"Checking source save: {SOURCE_SAV}")
    actual_source_sha = sha256_file(SOURCE_SAV)
    assert actual_source_sha == EXPECTED_SOURCE_SHA256, f"Source SHA mismatch: {actual_source_sha} vs {EXPECTED_SOURCE_SHA256}"
    print(f"Source SHA256 verified: {actual_source_sha}")

    cfg_path = WORK_DIR / "functional.cfg"
    if not cfg_path.exists():
        cfg_path.write_text("""[gui]
autosave_interval = 0
autosave_on_exit = false
threaded_saves = false

[difficulty]
infinite_money = true

[newgrf]
/home/flax/games/openspacettd/bin/newgrf/openspacettd_integrated_v1.grf =
/home/flax/games/openspacettd/bin/newgrf/openspacettd_rail_v3.grf =
/home/flax/games/openspacettd/bin/newgrf/openspacettd_equipment_v1.grf =

[misc]
language = english.lng
""")

    staged_sav = WORK_DIR / "source_checkpoint.sav"
    shutil.copy2(SOURCE_SAV, staged_sav)

    with open(SUMMARY_PATH) as f:
        summary = json.load(f)
    services = summary["final_services"]

    for f in WORK_DIR.glob("core_basket_repeat*"):
        f.unlink()
    for f in WORK_DIR.glob("cold_reload_verify*"):
        f.unlink()

    engine = OfflineEngine(ROOT / "build/openttd", cfg_path, WORK_DIR, "core_basket_repeat", save=staged_sav)
    evidence = {
        "source_save": {
            "path": str(SOURCE_SAV),
            "sha256": actual_source_sha,
            "tick": 304384
        },
        "adapter_staging": {},
        "simulation_milestones": [],
        "basket_consumption_event": None,
        "saved_checkpoint": {},
        "cold_reload": {}
    }

    try:
        status_raw = engine.command("connected_economy basket-status", "CONNECTED basket-state ")
        status = json.loads(status_raw)
        initial_state = status["state"]
        print(f"Loaded tick: {initial_state['tick']}")
        assert initial_state["tick"] == 304384

        token = "coretownstationrepeat20261010tok"
        envelope = {
            "version": 1,
            "token": token,
            "town": 0,
            "train": 4,
            "pickup": 2,
            "delivery": 3,
            "x": 795,
            "y": 62,
            "connector_x": 794,
            "connector_y": 62,
            "connector_track": 0,
            "maximum_native_cost": 100000,
            "state": initial_state
        }
        envelope_path = WORK_DIR / "envelope.json"
        envelope_path.write_text(json.dumps(envelope, indent=2))

        quote_raw = engine.command(f'connected_economy town-station-quote "{envelope_path}"', "CONNECTED town-station ")
        quote_resp = json.loads(quote_raw)
        print("Quote response keys:", list(quote_resp.keys()))
        if "stopped" in quote_resp and quote_resp["stopped"]:
            print("Quote stopped:", quote_resp.get("reason"), quote_resp.get("secondary_errors"))
        print("Quote:", quote_resp.get("quote"))
        evidence["adapter_staging"]["quote"] = quote_resp.get("quote")

        st_resp = json.loads(engine.command(f"connected_economy town-station-apply {token} station", "CONNECTED town-station "))
        print("Station built, paid:", st_resp["native_paid"])
        evidence["adapter_staging"]["station_applied"] = {"paid": st_resp["native_paid"], "station_id": st_resp["observation"]["adapter"].get("station")}

        link_resp = json.loads(engine.command(f"connected_economy town-station-apply {token} link", "CONNECTED town-station "))
        print("Link built, total paid:", link_resp["native_paid"])
        evidence["adapter_staging"]["link_applied"] = {"paid": link_resp["native_paid"]}

        ord_resp = json.loads(engine.command(f"connected_economy town-station-apply {token} orders", "CONNECTED town-station "))
        print("Orders retargeted, total paid:", ord_resp["native_paid"])
        evidence["adapter_staging"]["orders_applied"] = {"paid": ord_resp["native_paid"], "train_orders": [t for t in ord_resp["state"]["trains"] if t["id"] == 4][0]["orders"]}

        arm_data = {
            "state": ord_resp["state"],
            "observation": {k: v for k, v in ord_resp["observation"].items() if k != "adapter"},
            "basket": {
                "source_sha256": "206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7",
                "initial_advances": 71,
                "gross_debits": 373556 + 5672 + ord_resp["native_paid"],
                "quoted_total": 364281,
                "resume_initial": True,
                "services": services
            }
        }
        arm_path = WORK_DIR / "cold-arm.json"
        arm_path.write_text(json.dumps(arm_data, indent=2))

        arm_resp = json.loads(engine.command(f'connected_economy basket-arm "{arm_path}"', "CONNECTED basket-result "))
        print("Basket armed failed:", arm_resp.get("failed"), "reason:", arm_resp.get("reason"))
        assert not arm_resp.get("failed"), f"Basket arm failed: {arm_resp.get('reason')}"

        restart_resp = json.loads(engine.command("connected_economy basket-restart 28", "CONNECTED basket-result "))
        print("Basket restart 28 failed:", restart_resp.get("failed"), "reason:", restart_resp.get("reason"))
        assert not restart_resp.get("failed"), f"Basket restart failed: {restart_resp.get('reason')}"

        consumption_found = False
        target_adv = 36
        for adv_i in range(1, target_adv + 1):
            adv = json.loads(engine.command("connected_economy basket-advance", "CONNECTED basket-result "))
            tick = adv["state"]["tick"]
            res = adv["observation"]["towns"][0]["reserves"]
            months = adv["audit"]["basket_months"]
            arrivals = adv["audit"]["arrivals"]
            
            # Check milestones
            if arrivals:
                for arr in arrivals:
                    print(f"Adv {adv_i} (tick {tick}): Arrival: {arr}")
                    evidence["simulation_milestones"].append({
                        "advance": adv_i,
                        "tick": tick,
                        "type": "arrival",
                        "detail": arr
                    })
            if months:
                for m in months:
                    print(f"Adv {adv_i} (tick {tick}): Month Event: constr={m['construction_complete']} food={m['food_sufficient']} growth={m['growth']} hook={m.get('native_growth_hook_enabled')} pop={m['population']} houses={m.get('house_count')}")
                    if m["construction_complete"]:
                        consumption_found = True
                        evidence["basket_consumption_event"] = {
                            "advance": adv_i,
                            "tick": tick,
                            "month": m,
                            "town_reserves_after": res
                        }

        assert consumption_found, "Construction basket consumption not observed within 36 advances!"
        print("\n*** Second Core Construction Basket Consumption Confirmed! ***")

        consumed_sav = WORK_DIR / "basket-consumed.sav"
        if consumed_sav.exists():
            consumed_sav.unlink()
        engine.save(consumed_sav)
        print(f"Saved checkpoint to {consumed_sav}")

        consumed_sha256 = sha256_file(consumed_sav)
        print(f"Saved Checkpoint SHA256: {consumed_sha256}")
        saved_state = adv["state"]
        evidence["saved_checkpoint"] = {
            "path": str(consumed_sav),
            "sha256": consumed_sha256,
            "tick": adv["state"]["tick"],
            "money": adv["state"]["money"],
            "reserves": adv["observation"]["towns"][0]["reserves"],
            "population": adv["observation"]["towns"][0]["population"],
            "houses": evidence["basket_consumption_event"]["month"]["house_count"]
        }

    finally:
        engine.close()

    print("\n--- Starting Cold Reload in Fresh Process ---")
    cold_engine = OfflineEngine(ROOT / "build/openttd", cfg_path, WORK_DIR, "cold_reload_verify", save=consumed_sav)
    try:
        cold_raw = cold_engine.command("connected_economy basket-status", "CONNECTED basket-state ")
        cold_status = json.loads(cold_raw)
        cold_state = cold_status["state"]
        cold_obs = cold_status["observation"]

        print(f"Cold loaded tick: {cold_state['tick']}")
        assert cold_state["tick"] == evidence["saved_checkpoint"]["tick"], f"Tick mismatch: {cold_state['tick']} vs {evidence['saved_checkpoint']['tick']}"

        cold_reserves = cold_obs["towns"][0]["reserves"]
        print(f"Cold town reserves: {cold_reserves}")
        assert cold_reserves == evidence["saved_checkpoint"]["reserves"], f"Reserves mismatch: {cold_reserves} vs {evidence['saved_checkpoint']['reserves']}"

        cold_town = cold_obs["towns"][0]
        print(f"Cold town population: {cold_town['population']}")
        assert cold_town["population"] == evidence["saved_checkpoint"]["population"]

        for key in ("money", "rng", "all_held"):
            assert cold_state[key] == saved_state[key], f"Cold state {key} differs from the saved checkpoint"
        print("Cold money, RNG and total cargo custody equal the saved checkpoint")

        saved_orders = {t["id"]: t["orders"] for t in saved_state["trains"] if "orders" in t}
        cold_orders = {t["id"]: t["orders"] for t in cold_state["trains"] if "orders" in t}
        assert saved_orders == cold_orders, "Train orders (incl. Train 4 -> new station) differ after reload"
        assert cold_orders[4][1][1] == st_resp["station"], "Train 4 delivery order no longer targets the new station"

        evidence["cold_reload"] = {
            "verified": True,
            "tick": cold_state["tick"],
            "money": cold_state["money"],
            "reserves": cold_reserves,
            "population": cold_town["population"],
            "train4_orders": cold_orders[4],
            "advance_after_reload": "not attempted: basket session is intentionally not persisted"
        }
        print("Cold Reload Verification Passed Perfectly!")

    finally:
        cold_engine.close()

    # Save evidence json
    out_evidence_path = ROOT / "docs" / "audit" / "2026-10-10" / "core-basket-repeat" / "verification.json"
    out_evidence_path.parent.mkdir(parents=True, exist_ok=True)
    out_evidence_path.write_text(json.dumps(evidence, indent=2))
    print(f"\nEvidence written to {out_evidence_path}")

if __name__ == "__main__":
    main()
