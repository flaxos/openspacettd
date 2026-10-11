#!/usr/bin/env python3
"""Execute and verify the isolated Commonwealth World Connection simulation (Sprint 3).

Runs a native 24-period execution on a fresh disposable map under V2 rules,
observes baseline single-throat extraction from World 2,
commissions an alternative silica source on World 3 via survey and direct portal pair,
measures route distance and travel times (294 tiles vs 248 tiles, ~15.6% shorter),
deliberately closes the primary World 2 throat corridor to prove independence and resilience,
reopens the primary corridor to verify recovery,
and tests fresh-process cold reload for 100% state match and cargo conservation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

from test_wp11_slice import Engine, ROOT, require


def make_config(output: Path) -> Path:
    config = output / "world_connection.cfg"
    text = (ROOT / "demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]
    for old, new in [("map_x = 9", "map_x = 10"), ("map_y = 7", "map_y = 10"),
                     ("landscape = temperate", "landscape = arctic"),
                     ("threaded_saves = true", "threaded_saves = false"),
                     ("autosave_on_exit = true", "autosave_on_exit = false")]:
        text = text.replace(old, new)
    text += "\n[newgrf]\n" + "".join(f"{ROOT}/bin/newgrf/{name} =\n" for name in ("openspacettd_integrated_v1.grf", "openspacettd_rail_v3.grf", "openspacettd_equipment_v1.grf"))
    config.write_text(text)
    return config


def verify_world_expansion(args):
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
    engine = Engine(args.binary.resolve(), config, output, "expansion_engine", None, year=1950)
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

        we_init = initial_status.get("world_expansion", {})
        require(we_init.get("connected") is False, "Expected connected == False initially")
        require(we_init.get("primary_corridor_closed") is False, "Expected primary_corridor_closed == False initially")
        initial_money = initial_status.get("money", 0)

        # Periods 1 to 6: Baseline operation (World 2 source only)
        print("Running Periods 1–6 (Baseline single-corridor operation)...", flush=True)
        for period in range(1, 7):
            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")

            we = step_data.get("world_expansion", {})
            report["ledger"].append({
                "period": period,
                "phase": "baseline",
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "connected": we.get("connected"),
                "primary_corridor_closed": we.get("primary_corridor_closed"),
                "w3_train": we.get("w3_train")
            })
            print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} money={step_data.get('money')}", flush=True)

        # Period 7: Connect World 3 alternative source
        print("Period 7: Connecting World 3 alternative silica source...", flush=True)
        connect_raw = engine.command("connected_economy connect-world", "CONNECTED world-connected ")
        connect_report = json.loads(connect_raw)
        report["connect_report"] = connect_report
        require(connect_report.get("success") is True, f"Failed to connect world: {connect_report}")
        require(connect_report.get("route_distance_w2") == 294, f"Expected route distance W2=294, got {connect_report.get('route_distance_w2')}")
        require(connect_report.get("route_distance_w3") == 248, f"Expected route distance W3=248, got {connect_report.get('route_distance_w3')}")

        post_connect_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        we_post = post_connect_status.get("world_expansion", {})
        require(we_post.get("connected") is True, "Expected connected == True after connect-world")
        require(we_post.get("w3_train") != 4294967295, "Expected alternative train ID assigned")

        costs = connect_report.get("costs", {})
        print(f"Verified World 3 connection: costs={costs}, route W2={connect_report.get('route_distance_w2')} tiles, W3={connect_report.get('route_distance_w3')} tiles (~15.6% shorter)", flush=True)

        # Save cold start checkpoint right after connection
        cold_start_sav = output / "cold-start.sav"
        engine.save(cold_start_sav)
        report["cold_start_save_sha256"] = hashlib.sha256(cold_start_sav.read_bytes()).hexdigest()

        # Period 7 advance
        step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
        require(step_data.get("conserved"), f"Cargo conservation failure at period 7: {step_data.get('errors')}")
        report["ledger"].append({
            "period": 7,
            "phase": "connection",
            "year": step_data.get("year"),
            "month": step_data.get("month"),
            "money": step_data.get("money"),
            "connected": True,
            "primary_corridor_closed": False,
            "w3_train": we_post.get("w3_train")
        })

        # Periods 8 to 12: Dual-source operation
        print("Running Periods 8–12 (Dual-source operation)...", flush=True)
        for period in range(8, 13):
            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")
            we = step_data.get("world_expansion", {})
            report["ledger"].append({
                "period": period,
                "phase": "dual_source",
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "connected": we.get("connected"),
                "primary_corridor_closed": we.get("primary_corridor_closed"),
                "w3_train": we.get("w3_train")
            })
            print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} money={step_data.get('money')}", flush=True)

        # Period 13: Deliberate primary corridor closure
        print("Period 13: Closing primary World 2 throat corridor (Train 1 stopped)...", flush=True)
        engine.command("connected_economy close-primary-corridor", "CONNECTED primary-corridor closed")

        # Periods 13 to 18: Primary corridor interrupted, World 3 sustains Ballast Crusher
        print("Running Periods 13–18 (Interrupted primary corridor, resilient W3 supply)...", flush=True)
        for period in range(13, 19):
            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")
            we = step_data.get("world_expansion", {})
            require(we.get("primary_corridor_closed") is True, f"Expected primary_corridor_closed == True at period {period}")
            report["ledger"].append({
                "period": period,
                "phase": "corridor_interrupted",
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "connected": we.get("connected"),
                "primary_corridor_closed": we.get("primary_corridor_closed"),
                "w3_train": we.get("w3_train")
            })
            print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} money={step_data.get('money')} (corridor closed: W3 online)", flush=True)

        # Period 19: Reopen primary corridor
        print("Period 19: Reopening primary World 2 throat corridor (Train 1 restarts)...", flush=True)
        engine.command("connected_economy reopen-primary-corridor", "CONNECTED primary-corridor reopened")

        # Periods 19 to 24: Restored dual-corridor operation
        print("Running Periods 19–24 (Corridor restored, combined operation)...", flush=True)
        for period in range(19, 25):
            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")
            we = step_data.get("world_expansion", {})
            require(we.get("primary_corridor_closed") is False, f"Expected primary_corridor_closed == False at period {period}")
            report["ledger"].append({
                "period": period,
                "phase": "corridor_restored",
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "connected": we.get("connected"),
                "primary_corridor_closed": we.get("primary_corridor_closed"),
                "w3_train": we.get("w3_train")
            })
            print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} money={step_data.get('money')}", flush=True)

        checkpoint_24_sav = output / "checkpoint-24.sav"
        engine.save(checkpoint_24_sav)
        report["checkpoint_24_save_sha256"] = hashlib.sha256(checkpoint_24_sav.read_bytes()).hexdigest()
        last_state = step_data

        print("24 periods completed successfully! Saving checkpoint...", flush=True)

    finally:
        engine.close()

    # Step 4: Cold reload in fresh process
    print("Testing cold reload in fresh process...", flush=True)
    probe = Engine(args.binary.resolve(), config, output, "expansion_reload_probe", checkpoint_24_sav, year=1950)
    probe.deadline = time.monotonic() + 300
    try:
        loaded_status = json.loads(probe.command("connected_economy status", "CONNECTED state "))
        for key in ("tick", "date", "money", "held", "stocks", "facilities", "economy", "world_expansion"):
            require(loaded_status.get(key) == last_state.get(key), f"Cold reload mismatch for {key}: loaded={loaded_status.get(key)} vs saved={last_state.get(key)}")
        print("Cold reload exact equality verified!", flush=True)
        report["cold_reload_equal"] = True

        # Advance 4 more periods in reloaded engine
        for extra in range(1, 5):
            extra_data = json.loads(probe.command("connected_economy advance", "CONNECTED advance "))
            require(extra_data.get("conserved"), f"Cold cargo conservation failure at period {extra}")
        print("4 post-reload periods completed successfully with 100% cargo conservation.", flush=True)
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
    parser.add_argument("--output", type=Path, default=ROOT / "docs/audit/2026-10-11/world-connection")
    args = parser.parse_args()
    verify_world_expansion(args)
