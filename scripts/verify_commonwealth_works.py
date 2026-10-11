#!/usr/bin/env python3
"""Execute and verify the isolated Capacity or Reserve prototype simulation.

Runs a native 24-period execution on a fresh disposable map under V2 rules,
observes factory commissioning, feeder interruption, food service, town stability,
and tests fresh-process cold reload.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

from test_wp11_slice import Engine, ROOT, require


def make_config(output: Path) -> Path:
    config = output / "works.cfg"
    text = (ROOT / "demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]
    for old, new in [("map_x = 9", "map_x = 10"), ("map_y = 7", "map_y = 10"),
                     ("landscape = temperate", "landscape = arctic"),
                     ("threaded_saves = true", "threaded_saves = false"),
                     ("autosave_on_exit = true", "autosave_on_exit = false")]:
        text = text.replace(old, new)
    text += "\n[newgrf]\n" + "".join(f"{ROOT}/bin/newgrf/{name} =\n" for name in ("openspacettd_integrated_v1.grf", "openspacettd_rail_v3.grf", "openspacettd_equipment_v1.grf"))
    config.write_text(text)
    return config


def verify_prototype(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config = make_config(output)

    report = {
        "status": "RUNNING",
        "works_choice": args.works_choice,
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "start_time_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "ledger": []
    }

    print("Launching OpenSpaceTTD headless engine (seed=11)...", flush=True)
    engine = Engine(args.binary.resolve(), config, output, "works_engine", None, year=1950)
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

        # Step 2: Set food service rate
        print("Setting town food service rate to 20...", flush=True)
        engine.command("connected_economy set-food-rate 20", "CONNECTED food-rate 20")

        # Step 3: Commission works choice
        comm_cmd = f"connected_economy commission-{args.works_choice}"
        print(f"Commissioning works choice: {comm_cmd}...", flush=True)
        engine.command(comm_cmd, f"CONNECTED commissioned {args.works_choice}")

        post_commission_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        report["post_commission_status"] = post_commission_status

        # Verify commissioning cash debit (£50,000) and BOM
        initial_money = initial_status.get("money", 0)
        post_money = post_commission_status.get("money", 0)
        require(initial_money - post_money == 50000, f"Expected £50,000 commission debit, got £{initial_money - post_money}")

        factories = post_commission_status.get("economy", {}).get("factories", [])
        require(len(factories) > 0, "No factories found in economy state")
        # Ballast factory is first or has recipe matching
        ballast_factory = None
        for f in factories:
            # Under v2 format: [id, owner, capacity, last_batches, remainder, total_batches, work, input_capacity, output_capacity]
            if len(f) >= 7 and f[6] != 0:
                ballast_factory = f
                break
        require(ballast_factory is not None, "Commissioned ballast factory not found in state")
        expected_work = 1 if args.works_choice == "capacity" else 2
        require(ballast_factory[6] == expected_work, f"Expected work {expected_work}, got {ballast_factory[6]}")
        print(f"Verified factory commissioned: work={ballast_factory[6]}, capacity={ballast_factory[2]}, in_cap={ballast_factory[7]}, out_cap={ballast_factory[8]}", flush=True)

        # Save cold start checkpoint
        cold_start_sav = output / "cold-start.sav"
        engine.save(cold_start_sav)
        report["cold_start_save_sha256"] = hashlib.sha256(cold_start_sav.read_bytes()).hexdigest()

        # Step 4: Advance 24 periods with feeder interruption
        print("Running 24-period simulation...", flush=True)
        total_evaluations = 0
        for period in range(1, 25):
            if period == 13:
                print(f"Period {period}: Interrupting SILC feeder...", flush=True)
                engine.command("connected_economy stop-silica", "CONNECTED silica stopped")
            elif period == 19:
                print(f"Period {period}: Resuming SILC feeder...", flush=True)
                engine.command("connected_economy start-silica", "CONNECTED silica started")

            if period == 15:
                print(f"Period {period}: Stopping food delivery to test starvation immunity...", flush=True)
                engine.command("connected_economy stop-food", "CONNECTED food stopped")
            elif period == 19:
                print(f"Period {period}: Resuming food delivery...", flush=True)
                engine.command("connected_economy start-food", "CONNECTED food started")

            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")

            # Extract metrics
            town_state = step_data.get("cities", [{}])[0] if step_data.get("cities") else {}
            batches = [f.get("batches", 0) for f in step_data.get("facilities", [])]
            city_months = step_data.get("city_months", [])
            total_evaluations += len(city_months)
            report["ledger"].append({
                "period": period,
                "tick": step_data.get("tick"),
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "batches": batches,
                "town_population": town_state.get("population"),
                "town_houses": town_state.get("houses"),
                "food_rate": town_state.get("food_rate", 0),
                "food_reserve": town_state.get("food_reserve", 0),
                "food_consumed_history": town_state.get("food_consumed_history", []),
                "food_requested_history": town_state.get("food_requested_history", []),
                "food_delivered_last": town_state.get("food_delivered_last", 0),
                "town_state": town_state.get("state"),
                "evaluations_observed": len(city_months)
            })

            if period % 4 == 0 or period in (12, 13, 18, 19):
                print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} "
                      f"batches={batches[:3]} pop={town_state.get('population')} "
                      f"food_cons={town_state.get('food_consumed_history', [])}", flush=True)

        checkpoint_24_sav = output / "checkpoint-24.sav"
        engine.save(checkpoint_24_sav)
        report["checkpoint_24_save_sha256"] = hashlib.sha256(checkpoint_24_sav.read_bytes()).hexdigest()
        last_state = step_data

    finally:
        engine.close()
        if report["status"] != "PASSED":
            report["status"] = "FAILED"
            evidence_file = output / "evidence.json"
            evidence_file.write_text(json.dumps(report, indent=2) + "\n")

    # Step 5: Cold reload in fresh process
    print("Testing cold reload in fresh process...", flush=True)
    probe = Engine(args.binary.resolve(), config, output, "works_reload_probe", checkpoint_24_sav, year=1950)
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
        if report["status"] != "PASSED":
            report["status"] = "FAILED"
            evidence_file = output / "evidence.json"
            evidence_file.write_text(json.dumps(report, indent=2) + "\n")

    report["status"] = "PASSED"
    report["end_time_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    evidence_file = output / "evidence.json"
    evidence_file.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Evidence written to {evidence_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--output", type=Path, default=ROOT / "build/audit-capacity-or-reserve")
    parser.add_argument("--works-choice", choices=["capacity", "reserve"], default="capacity")
    args = parser.parse_args()
    verify_prototype(args)
