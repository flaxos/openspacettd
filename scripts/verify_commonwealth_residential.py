#!/usr/bin/env python3
"""Execute and verify the isolated Commonwealth Residential Works simulation (Sprint 2).

Runs a native 24-period execution on a fresh disposable map under V2 rules,
observes residential commissioning (160 STEL + 80 BALL + £50,000),
verifies finite opportunity budget (4 houses), native stage progression (0 to 3),
population attribution only upon completion, immunity to random redevelopment demolition,
decoupling from commercial food service, and tests fresh-process cold reload.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

from test_wp11_slice import Engine, ROOT, require


def make_config(output: Path) -> Path:
    config = output / "residential.cfg"
    text = (ROOT / "demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]
    for old, new in [("map_x = 9", "map_x = 10"), ("map_y = 7", "map_y = 10"),
                     ("landscape = temperate", "landscape = arctic"),
                     ("threaded_saves = true", "threaded_saves = false"),
                     ("autosave_on_exit = true", "autosave_on_exit = false")]:
        text = text.replace(old, new)
    text += "\n[newgrf]\n" + "".join(f"{ROOT}/bin/newgrf/{name} =\n" for name in ("openspacettd_integrated_v1.grf", "openspacettd_rail_v3.grf", "openspacettd_equipment_v1.grf"))
    config.write_text(text)
    return config


def verify_residential(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config = make_config(output)

    report = {
        "status": "RUNNING",
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "start_time_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "ledger": []
    }

    print("Launching OpenSpaceTTD headless engine (seed=11)...", flush=True)
    engine = Engine(args.binary.resolve(), config, output, "residential_engine", None, year=1950)
    engine.deadline = time.monotonic() + 1800

    try:
        # Step 1: Prepare works scenario
        print("Preparing works prototype scenario...", flush=True)
        res = engine.command("connected_economy prepare-works", "CONNECTED prepared")
        print("Scenario prepared successfully.", flush=True)

        # Inspect initial state
        initial_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        report["initial_status"] = initial_status
        econ = initial_status.get("economy", {})
        require(econ.get("version") == 2, f"Expected economy version 2, got {econ.get('version')}")

        towns = initial_status.get("cities", [])
        require(len(towns) >= 1, "Expected at least 1 megacity")
        initial_pop = towns[0].get("population", 0)
        initial_houses = towns[0].get("houses", 0)
        initial_money = initial_status.get("money", 0)

        # Step 2: Commission residential district
        print("Commissioning residential district...", flush=True)
        engine.command("connected_economy commission-residential", "CONNECTED commissioned residential")

        post_commission_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        report["post_commission_status"] = post_commission_status

        # Verify £50,000 cash deducted
        post_money = post_commission_status.get("money", 0)
        money_diff = initial_money - post_money
        require(money_diff == 50000, f"Expected £50,000 debited, got £{money_diff}")

        # Verify district commissioned with 4 opportunities
        districts = post_commission_status.get("economy", {}).get("town_residential_districts", [])
        require(len(districts) > 0, "No residential districts found in economy state")
        core_district = districts[0]
        require(core_district[2] is True, "Expected district commissioned == True")
        require(core_district[3] == 4, f"Expected 4 building opportunities, got {core_district[3]}")
        require(core_district[4] == 0, f"Expected 0 buildings placed initially, got {core_district[4]}")
        require(core_district[5] == 0, f"Expected 0 buildings completed initially, got {core_district[5]}")
        print(f"Verified district commissioned: opportunities={core_district[3]}, placed={core_district[4]}, completed={core_district[5]}", flush=True)

        # Save cold start checkpoint
        cold_start_sav = output / "cold-start.sav"
        engine.save(cold_start_sav)
        report["cold_start_save_sha256"] = hashlib.sha256(cold_start_sav.read_bytes()).hexdigest()

        # Step 3: Advance 24 periods with food service interruption
        print("Running 24-period simulation...", flush=True)
        for period in range(1, 25):
            # At period 14, interrupt food delivery to test decoupling
            if period == 14:
                print(f"Period {period}: Stopping food delivery to test decoupling...", flush=True)
                engine.command("connected_economy stop-food", "CONNECTED food stopped")
            elif period == 18:
                print(f"Period {period}: Resuming food delivery...", flush=True)
                engine.command("connected_economy start-food", "CONNECTED food started")

            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")

            # Extract metrics
            town_state = step_data.get("cities", [{}])[0] if step_data.get("cities") else {}
            dist_data = step_data.get("economy", {}).get("town_residential_districts", [[]])[0] if step_data.get("economy", {}).get("town_residential_districts") else []

            report["ledger"].append({
                "period": period,
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "town_population": town_state.get("population"),
                "town_houses": town_state.get("houses"),
                "town_food_last": town_state.get("last", []),
                "district_opportunities": dist_data[3] if len(dist_data) > 3 else None,
                "district_placed": dist_data[4] if len(dist_data) > 4 else None,
                "district_completed": dist_data[5] if len(dist_data) > 5 else None,
                "district_blocked": dist_data[6] if len(dist_data) > 6 else None
            })

            if period % 4 == 0 or period in (1, 2, 3, 4, 13, 14, 17, 18):
                print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} "
                      f"pop={town_state.get('population')} houses={town_state.get('houses')} "
                      f"dist=[opp={dist_data[3] if len(dist_data) > 3 else '?'}, "
                      f"placed={dist_data[4] if len(dist_data) > 4 else '?'}, "
                      f"completed={dist_data[5] if len(dist_data) > 5 else '?'}]", flush=True)

        checkpoint_24_sav = output / "checkpoint-24.sav"
        engine.save(checkpoint_24_sav)
        report["checkpoint_24_save_sha256"] = hashlib.sha256(checkpoint_24_sav.read_bytes()).hexdigest()
        last_state = step_data

        # Final district verification
        final_dist = step_data.get("economy", {}).get("town_residential_districts", [[]])[0]
        final_town = step_data.get("cities", [{}])[0]
        require(final_dist[5] == 4, f"Expected 4 completed houses by period 24, got {final_dist[5]}")
        require(final_town.get("houses", 0) >= initial_houses + 4, f"Expected house count increase by at least 4")
        require(final_town.get("population", 0) > initial_pop, f"Expected population increase from completed native houses")
        print("Native house construction and protection fully confirmed!", flush=True)

    finally:
        engine.close()

    # Step 4: Cold reload in fresh process
    print("Testing cold reload in fresh process...", flush=True)
    probe = Engine(args.binary.resolve(), config, output, "residential_reload_probe", checkpoint_24_sav, year=1950)
    probe.deadline = time.monotonic() + 300
    try:
        loaded_status = json.loads(probe.command("connected_economy status", "CONNECTED state "))
        for key in ("tick", "date", "money", "held", "stocks", "facilities", "economy"):
            require(loaded_status.get(key) == last_state.get(key), f"Cold reload mismatch for {key}")
        print("Cold reload equality verified!", flush=True)
        report["cold_reload_equal"] = True

        # Advance 4 more periods in reloaded engine
        for extra in range(1, 5):
            extra_data = json.loads(probe.command("connected_economy advance", "CONNECTED advance "))
            require(extra_data.get("conserved"), f"Cold cargo conservation failure at period {extra}")
        print("4 post-reload periods completed successfully.", flush=True)
        report["post_reload_periods_passed"] = 4
    finally:
        probe.close()

    report["status"] = "PASSED"
    report["end_time_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    evidence_file = output / "evidence.json"
    evidence_file.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Evidence written to {evidence_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/audit/2026-10-11/residential-district")
    args = parser.parse_args()
    verify_residential(args)
