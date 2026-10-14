#!/usr/bin/env python3
"""Execute and verify the isolated Commonwealth Material-Funded Research Unlock simulation (Card P1).

Runs a native simulation on a fresh disposable map under V2 rules,
establishes Corporate HQ and unlocks baseline Materials I & II,
commissions Pipeline B extractive/processing freight rail routes (COPR, WIRE, SAND, CHIP, STEL),
surveys a 16x16 Rare Earth site on World 2,
physically delivers 40 STEL and 20 CHIP to the World 0 company stockpile,
verifies negative paths (missing kit rejection, foreign company rejection, atomic escrow reservation and refund),
funds Materials III research project (303, £600k budget),
proves TECH_MATERIALS_3 unlock and Rare Earth discovery revelation,
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
    config = output / "material_research.cfg"
    text = (ROOT / "demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]
    for old, new in [("map_x = 9", "map_x = 10"), ("map_y = 7", "map_y = 10"),
                     ("landscape = temperate", "landscape = arctic"),
                     ("threaded_saves = true", "threaded_saves = false"),
                     ("autosave_on_exit = true", "autosave_on_exit = false")]:
        text = text.replace(old, new)
    text += "\n[newgrf]\n" + "".join(f"{ROOT}/bin/newgrf/{name} =\n" for name in ("openspacettd_integrated_v1.grf", "openspacettd_rail_v3.grf", "openspacettd_equipment_v1.grf"))
    config.write_text(text)
    return config


def verify_material_research(args):
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
    engine = Engine(args.binary.resolve(), config, output, "research_engine", None, year=1950)
    engine.deadline = time.monotonic() + 2700  # 45 minutes budget

    try:
        # Step 1: Prepare works scenario
        print("Preparing works prototype scenario...", flush=True)
        engine.command("connected_economy prepare-works", "CONNECTED prepared")
        print("Scenario prepared successfully.", flush=True)

        initial_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        report["initial_status"] = initial_status
        econ = initial_status.get("economy", {})
        require(econ.get("version") == 2, f"Expected economy version 2, got {econ.get('version')}")

        # Step 2: Commission Pipeline B research freight chain
        print("Commissioning Pipeline B research freight chain and surveyed Rare Earth site...", flush=True)
        comm_raw = engine.command("connected_economy commission-research", "CONNECTED research-commissioned ")
        comm_report = json.loads(comm_raw)
        report["commission_report"] = comm_report
        require(comm_report.get("success") is True, f"Failed to commission research chain: {comm_report}")
        rare_site_id = comm_report.get("rare_site_id")
        require(rare_site_id is not None and rare_site_id > 0, f"Expected valid rare_site_id, got {rare_site_id}")
        print(f"Commissioned successfully. Rare site ID: {rare_site_id}, costs: {comm_report.get('costs')}", flush=True)

        # Inspect post-commission state
        post_comm_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        hq_info = post_comm_status.get("hq", {})
        require(hq_info.get("has_hq") is True, "Expected Corporate HQ established")
        print(f"Corporate HQ verified: tile={hq_info.get('tile')} world={hq_info.get('world')}", flush=True)

        # Verify rare site is surveyed but not discovered initially
        sites = post_comm_status.get("resource_sites", [])
        rare_site = next((s for s in sites if s.get("id") == rare_site_id), None)
        require(rare_site is not None, f"Rare site {rare_site_id} not found in resource_sites")
        require(rare_site.get("surveyed") is True, "Expected rare site surveyed == True")
        require(rare_site.get("discovered") is False, "Expected rare site discovered == False before Materials III")
        print(f"Verified Rare Earth site {rare_site_id}: surveyed=True, discovered=False (gated behind Materials III)", flush=True)

        # Verify initial tech state: 301 (Materials I) and 302 (Materials II) unlocked, 303 locked
        unlocked_techs = post_comm_status.get("research", [{}])[0].get("unlocked", [])
        require(301 in unlocked_techs, "Expected Materials I (301) unlocked")
        require(302 in unlocked_techs, "Expected Materials II (302) unlocked")
        require(303 not in unlocked_techs, "Expected Materials III (303) NOT unlocked initially")

        # Step 3: Simulation loop - physical freight delivery and stock accumulation
        print("Starting physical freight simulation loop (target: 40 STEL + 20 CHIP at HQ warehouse)...", flush=True)
        unlocked_materials_3 = False
        kit_escrowed = False
        period = 0
        max_periods = 36

        while period < max_periods:
            period += 1
            step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
            require(step_data.get("conserved"), f"Cargo conservation failure at period {period}: {step_data.get('errors')}")

            # Check stockpile levels and negative paths
            neg_raw = engine.command("connected_economy test-research-negative", "CONNECTED research-negative-tested ")
            neg_report = json.loads(neg_raw)
            stel_stock = neg_report.get("stel_stock", 0)
            chip_stock = neg_report.get("chip_stock", 0)

            report["ledger"].append({
                "period": period,
                "year": step_data.get("year"),
                "month": step_data.get("month"),
                "money": step_data.get("money"),
                "stel_stock": stel_stock,
                "chip_stock": chip_stock,
                "materials_3_unlocked": unlocked_materials_3
            })

            print(f"Period {period}: year={step_data.get('year')} month={step_data.get('month')} money={step_data.get('money')} | STEL stock={stel_stock}/40, CHIP stock={chip_stock}/20", flush=True)

            if not unlocked_materials_3 and stel_stock >= 40 and chip_stock >= 20:
                print(f"Target stock reached at period {period}! Verifying negative paths and atomic escrow...", flush=True)
                require(neg_report["tests"].get("missing_kit_rejected") is True, "Expected missing kit rejection")
                require(neg_report["tests"].get("foreign_rejected") is True, "Expected foreign company rejection")
                require(neg_report["tests"].get("escrow_debited") is True, "Expected atomic escrow debit")
                require(neg_report["tests"].get("escrow_refunded") is True, "Expected atomic escrow cancel/refund")
                require(neg_report["tests"].get("escrow_restored") is True, "Expected atomic escrow restoration")
                print("Negative paths and atomic escrow contracts verified 100%!", flush=True)
                kit_escrowed = True

                # Fund and start Materials III research
                print("Starting Materials III research (budget £600,000)...", flush=True)
                engine.command("connected_economy start-research-3 600000", "CONNECTED research-3-started budget 600000")

                # Advance one month to evaluate research progress
                period += 1
                step_data = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
                require(step_data.get("conserved"), f"Cargo conservation failure on unlock period {period}: {step_data.get('errors')}")

                post_unlock_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
                unlocked_techs_now = post_unlock_status.get("research", [{}])[0].get("unlocked", [])
                require(303 in unlocked_techs_now, "Expected Materials III (303) unlocked after funding")
                unlocked_materials_3 = True
                print("SUCCESS: Materials III (303) unlocked!", flush=True)

                # Verify rare earth site discovery revelation
                sites_now = post_unlock_status.get("resource_sites", [])
                rare_now = next((s for s in sites_now if s.get("id") == rare_site_id), None)
                require(rare_now is not None, f"Rare site {rare_site_id} not found after unlock")
                require(rare_now.get("discovered") is True, "Expected rare earth site discovered == True after Materials III unlock")
                print(f"SUCCESS: Rare Earth site {rare_site_id} is now discovered! (Opportunity revealed)", flush=True)

                report["unlock_event"] = {
                    "period": period,
                    "year": step_data.get("year"),
                    "month": step_data.get("month"),
                    "rare_site_id": rare_site_id,
                    "discovered": True,
                    "unlocked_techs": unlocked_techs_now
                }

                # Run at least 6 more periods post-unlock to verify stability
                post_target = period + 6
                while period < post_target and period < max_periods:
                    period += 1
                    post_step = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
                    require(post_step.get("conserved"), f"Cargo conservation failure post-unlock period {period}: {post_step.get('errors')}")
                    report["ledger"].append({
                        "period": period,
                        "year": post_step.get("year"),
                        "month": post_step.get("month"),
                        "money": post_step.get("money"),
                        "materials_3_unlocked": True
                    })
                    print(f"Post-unlock Period {period}: year={post_step.get('year')} month={post_step.get('month')} money={post_step.get('money')}", flush=True)
                break

        require(unlocked_materials_3, "Simulation ended before Materials III could be unlocked")

        # Save checkpoint savegame
        checkpoint_sav = output / "checkpoint-materials3.sav"
        engine.save(checkpoint_sav)
        report["checkpoint_save_sha256"] = hashlib.sha256(checkpoint_sav.read_bytes()).hexdigest()
        last_state = json.loads(engine.command("connected_economy status", "CONNECTED state "))
        print(f"Simulation completed successfully in {period} periods. Checkpoint saved: {checkpoint_sav.name}", flush=True)

    finally:
        engine.close()

    # Step 4: Cold reload probe in fresh process
    print("Testing cold reload in fresh process...", flush=True)
    probe = Engine(args.binary.resolve(), config, output, "research_reload_probe", checkpoint_sav, year=1950)
    probe.deadline = time.monotonic() + 300
    try:
        loaded_status = json.loads(probe.command("connected_economy status", "CONNECTED state "))
        for key in ("tick", "date", "money", "held", "stocks", "facilities", "economy", "hq", "research", "resource_sites"):
            require(loaded_status.get(key) == last_state.get(key), f"Cold reload mismatch for {key}: loaded={loaded_status.get(key)} vs saved={last_state.get(key)}")
        print("Cold reload exact 100% state match verified!", flush=True)
        report["cold_reload_equal"] = True

        # Advance 4 more periods in reloaded engine
        for extra in range(1, 5):
            extra_data = json.loads(probe.command("connected_economy advance", "CONNECTED advance "))
            require(extra_data.get("conserved"), f"Cold cargo conservation failure at period {extra}: {extra_data.get('errors')}")
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
    parser.add_argument("--output", type=Path, default=ROOT / "docs/audit/2026-10-11/material-research")
    args = parser.parse_args()
    verify_material_research(args)
