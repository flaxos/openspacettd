#!/usr/bin/env python3
"""Print bounded, read-only orientation from Git and the canonical project docs."""

from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
DOCUMENTS = (
    "docs/PROJECT_STATUS_AND_ROADMAP.md",
    "docs/CURRENT_ARCHITECTURE.md",
    "docs/KNOWN_LIMITATIONS.md",
    "docs/SPRINT_LEDGER.md",
)


def git(*args):
    return subprocess.run(
        ["git", *args], cwd=ROOT, check=True, capture_output=True, text=True,
        timeout=10,
    ).stdout.rstrip()


def first_update(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    start = next((i for i, line in enumerate(lines) if line.startswith("## ")), 0)
    end = next(
        (i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")),
        len(lines),
    )
    section = "\n".join(lines[start:end]).strip()
    return section if len(section) <= 650 else section[:650] + "\n[excerpt; read the relevant section]"


def main():
    print("OpenSpaceTTD orientation (excerpts, not fresh test or acceptance evidence)")
    print("Branch:", git("branch", "--show-current") or "(detached)")
    print("HEAD:", git("log", "-1", "--format=%h %s"))
    status = git("status", "--short", "--untracked-files=normal").splitlines()
    print("Working tree:", "clean" if not status else "\n" + "\n".join(status[:12]))
    if len(status) > 12:
        print(f"... {len(status) - 12} more status lines; inspect git status before editing")
    for document in DOCUMENTS:
        print(f"\n{document}\n{first_update(ROOT / document)}")
    print("\nNext: read task-relevant canonical sections, then owning source and tests.")
    print("Newer dated updates supersede historical snapshots; verify against this checkout.")
    print("Skills: ost-dev | ost-uat | ost-deliver (.agents/skills/)")


if __name__ == "__main__":
    main()
