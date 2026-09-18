#!/usr/bin/env python3
"""OpenSpaceTTD Prompt-to-Savegame Generator CLI.

Converts narrative descriptions into fully initialized, working OpenSpaceTTD
.sav scenario files with multi-world partitions, pre-built corridors, and active fleets.
Can run 100% offline deterministically, or optionally enrich narrative lore with Gemini.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def expand_prompt_with_gemini(prompt: str, model_name: str = "gemini-2.5-flash") -> str:
    """Optionally use Google Gemini to enrich the scenario prompt with Peter F. Hamilton lore."""
    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        print("[Gemini] No GEMINI_API_KEY found in environment; proceeding with deterministic offline engine.", file=sys.stderr)
        return prompt

    try:
        from google import genai
        client = genai.Client(api_key=api_key)
        system_instruction = (
            "You are a narrative scenario designer for OpenSpaceTTD, an interplanetary railway simulation "
            "inspired by Peter F. Hamilton's Commonwealth Saga. Given a player's scenario prompt, enrich it "
            "with authentic Commonwealth terminology (CST wormholes, Phase 1 Core worlds, Phase 3 Frontier outposts, "
            "Superalloys, Quantum Crystals, and Megacities) while preserving all original gameplay constraints. "
            "Return a concise 2-sentence scenario description."
        )
        response = client.models.generate_content(
            model=model_name,
            contents=prompt,
            config={"system_instruction": system_instruction}
        )
        if response and response.text:
            enriched = response.text.strip()
            print(f"[Gemini Enriched] {enriched}\n")
            return enriched
    except Exception as e:
        print(f"[Gemini] Optional enrichment failed ({e}); falling back to original prompt.", file=sys.stderr)

    return prompt


def generate_scenario(prompt: str, output_path: str, binary_path: str = "./build/openttd") -> bool:
    """Invoke the OpenSpaceTTD engine in dedicated headless mode to synthesize the scenario."""
    binary = Path(binary_path).resolve()
    if not binary.exists() or not os.access(binary, os.X_OK):
        print(f"Error: OpenSpaceTTD binary not found at '{binary}'. Build the project first.", file=sys.stderr)
        return False

    out_file = Path(output_path).resolve()
    out_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(binary),
        "-D127.0.0.1:0",
        "-s", "null",
        "-m", "null",
        "-v", "dedicated",
        "-b", "null",
        "-Y", prompt,
        "-g", str(out_file),
    ]

    print(f"Running OpenSpaceTTD Scenario Synthesis Engine...")
    print(f"Prompt: \"{prompt}\"")
    print(f"Output: {out_file}")

    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(proc.stdout)

    if proc.returncode != 0 or not out_file.is_file() or out_file.stat().st_size < 10000:
        print(f"Error: Synthesis failed with exit code {proc.returncode}.", file=sys.stderr)
        return False

    print(f"Scenario successfully generated: {out_file} ({out_file.stat().st_size:,} bytes)")
    return True


def verify_savegame(save_path: str, binary_path: str = "./build/openttd") -> bool:
    """Verify that the generated .sav file can be cleanly opened and inspected."""
    binary = Path(binary_path).resolve()
    save_file = Path(save_path).resolve()

    cmd = [str(binary), "-q", str(save_file)]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode == 0:
        print(f"Verification PASSED: {save_file} is a valid, readable OpenSpaceTTD scenario savegame.")
        return True
    else:
        print(f"Verification FAILED for {save_file}:\n{res.stderr}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="OpenSpaceTTD Prompt-to-Savegame Generator (Sprint 48)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  ./scripts/prompt_to_savegame.py "Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys"
  ./scripts/prompt_to_savegame.py "4-world system: glacial quantum research, smelting hub, and core megacity" -o demo/Glacial-Quantum.sav --verify
"""
    )
    parser.add_argument("prompt", nargs="?", default="Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys",
                        help="Narrative prompt describing worlds, industries, and conflict.")
    parser.add_argument("-o", "--output", default="demo/Arid-Mining-Vs-Greedy-Core.sav",
                        help="Destination .sav savegame path (default: demo/Arid-Mining-Vs-Greedy-Core.sav)")
    parser.add_argument("--binary", default="./build/openttd",
                        help="Path to openttd executable (default: ./build/openttd)")
    parser.add_argument("--use-gemini", action="store_true",
                        help="Optionally enhance the narrative prompt with Gemini API before synthesis")
    parser.add_argument("--verify", action="store_true",
                        help="Verify the generated scenario savegame with -q after creation")

    args = parser.parse_args()

    prompt = args.prompt
    if args.use_gemini:
        prompt = expand_prompt_with_gemini(prompt)

    success = generate_scenario(prompt, args.output, args.binary)
    if not success:
        sys.exit(1)

    if args.verify:
        if not verify_savegame(args.output, args.binary):
            sys.exit(2)


if __name__ == "__main__":
    main()
