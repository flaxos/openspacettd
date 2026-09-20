#!/usr/bin/env python3
"""OpenSpaceTTD Autonomous Headless Balancing Critic CLI.

Executes multi-decade accelerated simulation runs, collects macroeconomic & logistics
telemetry, renders an ASCII health dashboard, and synthesizes actionable balance
tuning patches (BOM recipes, megacity quotas, freight tariffs).
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional


def run_headless_critic(
    savegame: str,
    years: int,
    report_path: str,
    binary_path: str = "./build/openttd"
) -> bool:
    """Execute OpenSpaceTTD in dedicated headless mode with the Balancing Critic harness."""
    binary = Path(binary_path).resolve()
    if not binary.exists() or not os.access(binary, os.X_OK):
        print(f"Error: Binary '{binary}' not found or not executable.", file=sys.stderr)
        return False

    save = Path(savegame).resolve()
    if not save.exists():
        print(f"Error: Savegame '{save}' does not exist.", file=sys.stderr)
        return False

    rep = Path(report_path).resolve()
    rep.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(binary),
        "-D127.0.0.1:0",
        "-s", "null",
        "-m", "null",
        "-v", "dedicated",
        "-b", "null",
        "-g", str(save),
        f"-Z{years},{rep}"
    ]

    print(f"Executing OpenSpaceTTD Autonomous Balancing Critic ({years} years)...")
    print(f"Command: {' '.join(cmd)}")
    sys.stdout.flush()

    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
        print(proc.stdout)
        if proc.returncode != 0:
            print(f"Error: openttd exited with code {proc.returncode}", file=sys.stderr)
            return False
    except Exception as ex:
        print(f"Execution failed: {ex}", file=sys.stderr)
        return False

    if not rep.exists() or rep.stat().st_size == 0:
        print(f"Error: Report JSON '{rep}' was not generated.", file=sys.stderr)
        return False

    return True


def render_ascii_dashboard(report: Dict[str, Any], savegame: str) -> None:
    """Render a comprehensive, formatted ASCII dashboard of the macroeconomic health."""
    summary = report.get("summary", {})
    timeseries = report.get("timeseries", [])
    diagnoses = report.get("diagnoses", [])
    recommendations = report.get("recommendations", [])
    years = report.get("simulation_years", 0)

    is_sustainable = summary.get("is_sustainable", True)
    verdict_str = "SUSTAINABLE (BALANCED)" if is_sustainable else "CRITICAL (UNBALANCED)"

    print("\n" + "=" * 80)
    print("           OPENSPACETTD AUTONOMOUS BALANCING CRITIC DASHBOARD           ")
    print("=" * 80)
    print(f" Target Scenario Savegame : {savegame}")
    print(f" Simulation Duration      : {years} simulated calendar years")
    print(f" Telemetry Snapshots      : {len(timeseries)} points recorded")
    print(f" Overall Balance Verdict  : [{verdict_str}]")
    print("-" * 80)

    print("\n[1] MACROECONOMICS & MONETARY HEALTH")
    inf_cagr = summary.get("compound_annual_inflation", 0.0)
    divergence = summary.get("price_payment_divergence", 0.0)
    final_tariffs = timeseries[-1].get("tariffs_generated", 0) if timeseries else 0

    print(f"  * Compound Annual Inflation (CAGR) : {inf_cagr:+.2f}%")
    print(f"  * Price vs Payment Divergence Ratio : {divergence:.4f}")
    print(f"  * Interplanetary Tariffs Accrued   : Cr {final_tariffs:,}")

    print("\n[2] CORPORATE SOLVENCY & PROFITABILITY")
    avg_margin = summary.get("avg_profit_margin", 0.0)
    human_money = 0
    if timeseries and timeseries[-1].get("companies"):
        human_money = timeseries[-1]["companies"][0].get("money", 0)
    print(f"  * Human Operator Treasury (Final)  : Cr {human_money:,}")
    print(f"  * 20-Year Average Profit Margin    : {avg_margin:+.1f}%")

    print("\n[3] LOGISTICS HUBS & INTER-WORLD GATEWAYS")
    choke_index = summary.get("portal_choke_point_index", 0.0)
    starvation_rate = summary.get("stockpile_starvation_rate", 0.0)
    peak_wait = 0
    stuck_trains = 0
    if timeseries:
        for snap in timeseries:
            p = snap.get("portals", {})
            peak_wait = max(peak_wait, p.get("peak_wait_ticks", 0))
            stuck_trains = max(stuck_trains, p.get("stuck_trains", 0))

    print(f"  * Portal Choke Point Index         : {choke_index:.2f} (0.0=free-flowing, 1.0=saturated)")
    print(f"  * Peak Portal Queue Latency        : {peak_wait} ticks")
    print(f"  * Stuck Fleet Consists             : {stuck_trains}")
    print(f"  * Stockpile Starvation Frequency   : {starvation_rate:.1f}%")

    print("\n[4] MEGACITY COMMODITY SATISFACTION")
    mega_sat = summary.get("megacity_avg_satisfaction", 0.0)
    print(f"  * Average Commodity Quota Delivery : {mega_sat:.1f}%")

    print("\n" + "-" * 80)
    print(f" DIAGNOSTIC FINDINGS ({len(diagnoses)})")
    print("-" * 80)
    if diagnoses:
        for diag in diagnoses:
            print(f"  ! {diag}")
    else:
        print("  ✓ No critical macroeconomic or logistics anomalies identified.")

    print("\n" + "-" * 80)
    print(f" ACTIONABLE BALANCING RECOMMENDATIONS ({len(recommendations)})")
    print("-" * 80)
    if recommendations:
        header = f" {'SEV':<5} | {'CATEGORY':<10} | {'PARAMETER':<22} | {'CHANGE':<20} | {'RATIONALE'}"
        print(header)
        print(" " + "-" * 78)
        for rec in recommendations:
            sev = rec.get("severity", "INFO")
            cat = rec.get("category", "General")
            param = rec.get("parameter_name", "")
            curr = rec.get("current_value", "")
            rec_val = rec.get("recommended_value", "")
            change = f"{curr} -> {rec_val}"
            msg = rec.get("message", "")
            print(f" {sev:<5} | {cat:<10} | {param:<22} | {change:<20} | {msg}")
    else:
        print("  ✓ All telemetry parameters are within optimal equilibrium bounds.")

    print("=" * 80 + "\n")


def export_bom_tuning_patch(report: Dict[str, Any], output_path: str) -> bool:
    """Generate a structured JSON tuning patch that can adjust production formulas and quotas."""
    patch = {
        "version": "1.0",
        "description": "Autonomous Balancing Critic Tuning Patch",
        "simulation_years": report.get("simulation_years", 20),
        "is_sustainable": report.get("summary", {}).get("is_sustainable", True),
        "recipe_modifications": {},
        "megacity_quota_modifications": {},
        "tariff_rate_modifications": {},
        "metadata": {
            "diagnoses": report.get("diagnoses", []),
            "recommendation_count": len(report.get("recommendations", []))
        }
    }

    for rec in report.get("recommendations", []):
        cat = rec.get("category", "")
        param = rec.get("parameter_name", "")
        recommended = rec.get("recommended_value", "")

        if cat == "BOM" or "Recipe" in param or "Foundry" in param:
            patch["recipe_modifications"][param] = {
                "recommended_value": recommended,
                "rationale": rec.get("message", "")
            }
        elif cat == "Megacity" or "Quota" in param:
            patch["megacity_quota_modifications"][param] = {
                "recommended_value": recommended,
                "rationale": rec.get("message", "")
            }
        elif cat == "Tariff" or "Tariff" in param:
            patch["tariff_rate_modifications"][param] = {
                "recommended_value": recommended,
                "rationale": rec.get("message", "")
            }

    p = Path(output_path).resolve()
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(patch, f, indent=2)

    print(f"Exported balancing tuning patch to: {p}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="OpenSpaceTTD Autonomous Balancing Critic and Multi-Decade Simulator."
    )
    parser.add_argument(
        "savegame",
        nargs="?",
        default="demo/Arid-Mining-Vs-Greedy-Core.sav",
        help="Path to the scenario savegame (.sav) file (default: demo/Arid-Mining-Vs-Greedy-Core.sav)"
    )
    parser.add_argument(
        "--years",
        type=int,
        default=20,
        help="Number of simulated calendar years to fast-forward (default: 20)"
    )
    parser.add_argument(
        "--report",
        default="reports/critic_report.json",
        help="Path where the JSON telemetry report will be exported (default: reports/critic_report.json)"
    )
    parser.add_argument(
        "--binary",
        default="./build/openttd",
        help="Path to the OpenSpaceTTD binary executable (default: ./build/openttd)"
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Assert scenario macroeconomic sustainability (exit 0 on pass, 1 on critical imbalance)"
    )
    parser.add_argument(
        "--export-bom-patch",
        metavar="PATH",
        help="Export an actionable balance tuning patch (JSON) for BOM formulas and quotas"
    )

    args = parser.parse_args()

    # Step 1: Execute simulation harness
    if not run_headless_critic(args.savegame, args.years, args.report, args.binary):
        return 1

    # Step 2: Parse telemetry report
    try:
        with open(args.report, "r", encoding="utf-8") as f:
            report_data = json.load(f)
    except Exception as ex:
        print(f"Failed to read report JSON: {ex}", file=sys.stderr)
        return 1

    # Step 3: Render ASCII Dashboard
    render_ascii_dashboard(report_data, args.savegame)

    # Step 4: Optionally export tuning patch
    if args.export_bom_patch:
        export_bom_tuning_patch(report_data, args.export_bom_patch)

    # Step 5: Verification check
    if args.verify:
        is_sustainable = report_data.get("summary", {}).get("is_sustainable", True)
        if not is_sustainable:
            print("Verification FAILED: Scenario diagnosed with critical macroeconomic or logistics failure.", file=sys.stderr)
            return 1
        print("Verification PASSED: Scenario macroeconomic and logistics simulation is sustainable.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
