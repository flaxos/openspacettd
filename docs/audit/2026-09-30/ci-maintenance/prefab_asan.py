"""Build a separate targeted prefab lifetime diagnostic from configured Ninja objects."""

import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--generator-ref", help="Compile the generator from this Git ref")
    args = parser.parse_args()
    build = args.build.resolve()
    root = Path.cwd()
    out = build / "agent-logs"
    out.mkdir(exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    replacements = {}
    sources = ["src/vehicle.cpp"]
    if args.generator_ref:
        sources.append("src/portal/prompt_scenario_generator.cpp")

    with (out / "prefab-asan-build.log").open("w") as log:
        for source in sources:
            entry = next(item for item in commands if item["file"] == str(root / source))
            command = shlex.split(entry["command"])
            object_index = command.index("-o") + 1
            original_object = command[object_index]
            diagnostic_object = out / (Path(source).stem + "-prefab-asan.o")
            command[object_index] = str(diagnostic_object)
            if source == "src/vehicle.cpp":
                command += ["-fsanitize=address", "-fno-omit-frame-pointer"]
            else:
                # Resolve the baseline source's relative includes against its real directory.
                baseline = out / "baseline-prompt-scenario-generator.cpp"
                baseline.write_bytes(subprocess.check_output(
                    ["git", "show", f"{args.generator_ref}:{source}"], cwd=root))
                command[command.index("-c") + 1] = str(baseline)
                command += ["-iquote", str((root / source).parent)]
            subprocess.run(command, cwd=entry["directory"], stdout=log,
                           stderr=subprocess.STDOUT, check=True)
            replacements[original_object] = str(diagnostic_object)

        ninja = next(line.split("=", 1)[1] for line in
                     (build / "CMakeCache.txt").read_text().splitlines()
                     if line.startswith("CMAKE_MAKE_PROGRAM:"))
        lines = subprocess.check_output([ninja, "-t", "commands", "openttd_test"],
                                        cwd=build, text=True).splitlines()
        link = next(line for line in reversed(lines) if " -o openttd_test " in line)
        link = link.removeprefix(": && ").split(" && :", 1)[0]
        command = shlex.split(link)
        command = [replacements.get(part, part) for part in command]
        command[command.index("-o") + 1] = str(out / "openttd_test-prefab-asan")
        command += ["-fsanitize=address"]
        subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    print(out / "openttd_test-prefab-asan")


if __name__ == "__main__":
    main()
