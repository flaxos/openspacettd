#!/usr/bin/env python3
"""Execute and verify the isolated Capacity or Reserve prototype simulation.

Runs one bounded native execution on a fresh disposable map under V2 rules:
trains haul the 160 STEL + 80 BALL works kit into the World 1 warehouse, the
works are commissioned from that physical stock, and the run continues to exactly
24 monthly food evaluations with a SILC feeder interruption and a FOOD delivery
interruption. A fresh process then reloads the final save and runs exactly four
more evaluations. The first failed invariant stops the run; partial evidence is kept.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

from test_wp11_slice import Engine, ROOT, require

RECIPE_BALLAST_CRUSHING = 101
RECIPE_STEEL_SMELTING = 102
CRUSHER_WORLD = 1  # Ballast crusher (Node 4) and its warehouse (Stop 30) are on World 1.
KIT_STEL = 160
KIT_BALL = 80
COMMISSION_COST = 50000
SILC_PER_BATCH = 2
RUN_EVALUATIONS = 24
PROBE_EVALUATIONS = 4
SILICA_STOP, FOOD_STOP, RESUME = 12, 14, 18  # Evaluation counts at which feeds change.
MAX_ADVANCES = 40  # Each 2048-tick advance spans at most one month boundary.


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


def facility(status, recipe):
    """Return the single facility row running a recipe."""
    rows = [f for f in status.get("facilities", []) if f.get("recipe") == recipe]
    require(len(rows) == 1, f"Expected one facility with recipe {recipe}, found {len(rows)}")
    return rows[0]


def kit_cargos(status):
    """Resolve STEL, BALL and SILC cargo slots from the steel and ballast recipes."""
    ballast = facility(status, RECIPE_BALLAST_CRUSHING)
    steel = facility(status, RECIPE_STEEL_SMELTING)
    return {"STEL": steel["outputs"][0][0], "BALL": ballast["outputs"][0][0], "SILC": ballast["inputs"][0][0]}


def world_stock(status, world):
    """Company warehouse inventory on one world as {cargo: units}."""
    stock = {}
    for row in status.get("stocks", []):
        if row.get("world") == world:
            for cargo, units in row.get("inventory", []):
                stock[cargo] = stock.get(cargo, 0) + units
    return stock


def kit_ready(status, cargos):
    stock = world_stock(status, CRUSHER_WORLD)
    return stock.get(cargos["STEL"], 0) >= KIT_STEL and stock.get(cargos["BALL"], 0) >= KIT_BALL


def check_commission_debit(before, after, cargos):
    """The commission must debit exactly the cash and kit, and nothing else, from World 1."""
    require(before.get("money", 0) - after.get("money", 0) == COMMISSION_COST,
            f"Expected £{COMMISSION_COST} commission debit, got £{before.get('money', 0) - after.get('money', 0)}")
    old, new = world_stock(before, CRUSHER_WORLD), world_stock(after, CRUSHER_WORLD)
    expected = {cargos["STEL"]: KIT_STEL, cargos["BALL"]: KIT_BALL}
    for cargo in set(old) | set(new):
        debit = old.get(cargo, 0) - new.get(cargo, 0)
        require(debit == expected.get(cargo, 0), f"World {CRUSHER_WORLD} cargo {cargo} debit {debit}, expected {expected.get(cargo, 0)}")


def factory_row(status, industry_id):
    """V2 economy factory row: [id, owner, capacity, last, remainder, total, work, input_cap, output_cap]."""
    rows = [f for f in status.get("economy", {}).get("factories", []) if f[0] == industry_id]
    require(len(rows) == 1, f"Expected one economy factory row for industry {industry_id}")
    require(len(rows[0]) == 9, "Expected a V2 factory row with works and buffer capacities")
    return rows[0]


def crusher_buffer(status, cargos):
    """Crusher SILC waiting, the enforced input cap and works state."""
    crusher = facility(status, RECIPE_BALLAST_CRUSHING)
    row = factory_row(status, crusher["id"] - 1)
    waiting = dict((c, n) for c, n in crusher["inputs"]).get(cargos["SILC"], 0)
    cap = min(65535, row[7] * SILC_PER_BATCH)
    require(waiting <= cap, f"Crusher holds {waiting} SILC above its {cap} input cap")
    return {"silc_waiting": waiting, "silc_cap": cap, "work": row[6], "batches": crusher["batches"]}


class EvaluationCounter:
    """Counts monthly food evaluations and rejects duplicated or out-of-order months."""

    def __init__(self, last_date=None):
        self.total = 0
        self.last_date = last_date

    def add(self, city_months):
        for month in city_months:
            date = month[0]
            require(self.last_date is None or date > self.last_date, f"Evaluation date {date} is not after {self.last_date}")
            self.last_date = date
        self.total += len(city_months)
        return len(city_months)


def write_evidence(output, report):
    (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")


def verify_prototype(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config = make_config(output)

    report = {
        "status": "RUNNING",
        "works_choice": args.works_choice,
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "start_time_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "envelope": {"run_evaluations": RUN_EVALUATIONS, "probe_evaluations": PROBE_EVALUATIONS,
                     "run_wall_seconds": 1800, "probe_wall_seconds": 300},
        "ledger": [],
        "probe_ledger": [],
    }

    try:
        print("Launching OpenSpaceTTD headless engine (seed=11)...", flush=True)
        engine = Engine(args.binary.resolve(), config, output, "works_engine", None, year=1950)
        engine.deadline = time.monotonic() + 1800
        try:
            print("Preparing works prototype scenario...", flush=True)
            engine.command("connected_economy prepare-works", "CONNECTED prepared")

            initial_status = json.loads(engine.command("connected_economy status", "CONNECTED state "))
            report["initial_status"] = initial_status
            require(initial_status.get("economy", {}).get("version") == 2,
                    f"Expected economy version 2, got {initial_status.get('economy', {}).get('version')}")
            require(len(initial_status.get("cities", [])) >= 1, "Expected at least 1 megacity")
            cargos = kit_cargos(initial_status)
            report["cargos"] = cargos
            require(not kit_ready(initial_status, cargos), "World 1 already holds the works kit before any haulage")

            print("Setting town food service rate to 20...", flush=True)
            engine.command("connected_economy set-food-rate 20", "CONNECTED food-rate 20")

            evaluations = EvaluationCounter()
            commissioned_at = None
            last_state = initial_status
            for advance in range(1, MAX_ADVANCES + 1):
                if evaluations.total >= RUN_EVALUATIONS:
                    break
                if commissioned_at is None and kit_ready(last_state, cargos):
                    print(f"Kit hauled after {evaluations.total} evaluations; commissioning {args.works_choice} works...", flush=True)
                    before = json.loads(engine.command("connected_economy status", "CONNECTED state "))
                    engine.command(f"connected_economy commission-{args.works_choice}", f"CONNECTED commissioned {args.works_choice}")
                    after = json.loads(engine.command("connected_economy status", "CONNECTED state "))
                    check_commission_debit(before, after, cargos)
                    buffer = crusher_buffer(after, cargos)
                    require(buffer["work"] == (1 if args.works_choice == "capacity" else 2), f"Unexpected works state {buffer['work']}")
                    commissioned_at = evaluations.total
                    report["commission"] = {"evaluation": commissioned_at, "tick": after.get("tick"),
                                            "world_stock_before": world_stock(before, CRUSHER_WORLD),
                                            "world_stock_after": world_stock(after, CRUSHER_WORLD),
                                            "money_before": before.get("money"), "money_after": after.get("money"),
                                            "silc_cap_after": buffer["silc_cap"]}
                    report["post_commission_status"] = after
                    commissioned_sav = output / "commissioned.sav"
                    engine.save(commissioned_sav)
                    report["commissioned_save_sha256"] = hashlib.sha256(commissioned_sav.read_bytes()).hexdigest()
                if evaluations.total == SILICA_STOP and "silica_stopped_at" not in report:
                    require(commissioned_at is not None, "Works kit was not hauled and commissioned before the SILC interruption")
                    engine.command("connected_economy stop-silica", "CONNECTED silica stopped")
                    report["silica_stopped_at"] = evaluations.total
                if evaluations.total == FOOD_STOP and "food_stopped_at" not in report:
                    engine.command("connected_economy stop-food", "CONNECTED food stopped")
                    report["food_stopped_at"] = evaluations.total
                if evaluations.total == RESUME and "resumed_at" not in report:
                    engine.command("connected_economy start-silica", "CONNECTED silica started")
                    engine.command("connected_economy start-food", "CONNECTED food started")
                    report["resumed_at"] = evaluations.total

                step = json.loads(engine.command("connected_economy advance", "CONNECTED advance "))
                require(step.get("conserved"), f"Cargo conservation failure at advance {advance}: {step.get('errors')}")
                observed = evaluations.add(step.get("city_months", []))
                require(evaluations.total <= RUN_EVALUATIONS, "Run exceeded its evaluation budget")
                buffer = crusher_buffer(step, cargos)
                town = step.get("cities", [{}])[0]
                stock = world_stock(step, CRUSHER_WORLD)
                report["ledger"].append({
                    "advance": advance,
                    "evaluations_observed": observed,
                    "evaluations_total": evaluations.total,
                    "tick": step.get("tick"), "date": step.get("date"), "year": step.get("year"), "month": step.get("month"),
                    "money": step.get("money"),
                    "crusher": buffer,
                    "world1_stel": stock.get(cargos["STEL"], 0), "world1_ball": stock.get(cargos["BALL"], 0),
                    "town_population": town.get("population"), "town_houses": town.get("houses"),
                    "food_rate": town.get("food_rate", 0), "food_reserve": town.get("food_reserve", 0),
                    "food_consumed": sum(m[2] for m in step.get("city_months", [])),
                    "food_delivered_last": town.get("food_delivered_last", 0),
                    "conserved": step.get("conserved"),
                })
                print(f"Advance {advance}: evaluations={evaluations.total} {step.get('year')}-{step.get('month')} "
                      f"silc={buffer['silc_waiting']}/{buffer['silc_cap']} batches={buffer['batches']} "
                      f"w1 STEL/BALL={stock.get(cargos['STEL'], 0)}/{stock.get(cargos['BALL'], 0)} pop={town.get('population')}", flush=True)
                last_state = step
            require(evaluations.total == RUN_EVALUATIONS, f"Observed {evaluations.total} evaluations, expected exactly {RUN_EVALUATIONS}")
            require(commissioned_at is not None, "Works kit was never hauled and commissioned")
            require(all(k in report for k in ("silica_stopped_at", "food_stopped_at", "resumed_at")), "Interruption schedule did not complete")
            require(sum(e["food_consumed"] for e in report["ledger"]) > 0, "Zero FOOD consumption observed across the run")

            final_sav = output / "checkpoint-24.sav"
            engine.save(final_sav)
            report["checkpoint_24_save_sha256"] = hashlib.sha256(final_sav.read_bytes()).hexdigest()
        finally:
            engine.close()

        print("Testing cold reload in fresh process...", flush=True)
        probe = Engine(args.binary.resolve(), config, output, "works_reload_probe", final_sav, year=1950)
        probe.deadline = time.monotonic() + 300
        try:
            loaded = json.loads(probe.command("connected_economy status", "CONNECTED state "))
            for key in ("tick", "date", "money", "held", "stocks", "facilities", "economy"):
                require(loaded.get(key) == last_state.get(key), f"Cold reload mismatch for {key}")
            report["cold_reload_equal"] = True
            probe_evaluations = EvaluationCounter(evaluations.last_date)
            for advance in range(1, MAX_ADVANCES + 1):
                if probe_evaluations.total >= PROBE_EVALUATIONS:
                    break
                step = json.loads(probe.command("connected_economy advance", "CONNECTED advance "))
                require(step.get("conserved"), f"Cold cargo conservation failure at probe advance {advance}: {step.get('errors')}")
                probe_evaluations.add(step.get("city_months", []))
                report["probe_ledger"].append({"advance": advance, "evaluations_total": probe_evaluations.total,
                                               "date": step.get("date"), "crusher": crusher_buffer(step, cargos)})
            require(probe_evaluations.total == PROBE_EVALUATIONS, f"Probe observed {probe_evaluations.total} evaluations, expected {PROBE_EVALUATIONS}")
            report["post_reload_evaluations"] = probe_evaluations.total
        finally:
            probe.close()

        report["summary"] = summarize(report)
        report["status"] = "PASSED"
    except BaseException as exc:
        report["status"] = "FAILED"
        report["failure"] = str(exc)
        raise
    finally:
        report["end_time_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        write_evidence(output, report)
        print(f"Evidence ({report['status']}) written to {output / 'evidence.json'}", flush=True)


def summarize(report):
    """Observed reserve and interruption behaviour; facts, not acceptance."""
    after = [e for e in report["ledger"] if e["evaluations_total"] > report["commission"]["evaluation"]]
    window = [e for e in report["ledger"] if report["silica_stopped_at"] < e["evaluations_total"] <= report["resumed_at"]]
    max_silc = max((e["crusher"]["silc_waiting"] for e in after), default=0)
    return {
        "commissioned_after_evaluations": report["commission"]["evaluation"],
        "max_silc_waiting_after_commission": max_silc,
        "baseline_silc_cap": 300 * SILC_PER_BATCH,
        "reserve_capacity_used": max_silc > 300 * SILC_PER_BATCH,
        "batches_during_silica_interruption": (window[-1]["crusher"]["batches"] - window[0]["crusher"]["batches"]) if len(window) > 1 else 0,
        "food_consumed_total": sum(e["food_consumed"] for e in report["ledger"]),
        "population_start": report["ledger"][0]["town_population"],
        "population_end": report["ledger"][-1]["town_population"],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--output", type=Path, default=ROOT / "build/audit-capacity-or-reserve")
    parser.add_argument("--works-choice", choices=["capacity", "reserve"], default="capacity")
    args = parser.parse_args()
    try:
        verify_prototype(args)
    except Exception as exc:
        print(f"FAILED: {exc}", file=sys.stderr, flush=True)
        sys.exit(1)
