#!/usr/bin/env python3

"""Run upstream commit validators, including preserved integration merges."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


def git_bytes(*args):
    return subprocess.check_output(["git", *args])


def validation_message(message, parent_count):
    """Preserve all message bytes while recognizing two conventional merge forms."""
    subject = message.split(b"\n", 1)[0]
    if parent_count == 2 and (
        re.fullmatch(rb"Merge pull request #[1-9][0-9]* from [^\s/]+/[^\s]+", subject)
        or re.fullmatch(rb"Merge: \S.*", subject)
    ):
        # The upstream validator still checks the entire original subject/body.
        return b"Change: " + message
    return message


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("revision_range", nargs="?", default="HEAD^..HEAD^2")
    parser.add_argument("--hooks", required=True, type=Path)
    args = parser.parse_args()
    hooks = args.hooks.resolve()
    for name in ("check-diff.py", "check-message.py"):
        if not (hooks / name).is_file():
            parser.error(f"Missing upstream validator: {hooks / name}")

    commits = git_bytes("rev-list", args.revision_range, "--").decode("ascii").split()
    failed = False
    with tempfile.TemporaryDirectory(prefix="ost-pr-commits-") as temporary:
        diff_path = Path(temporary) / "commit.diff"
        message_path = Path(temporary) / "commit.message"
        for commit in commits:
            parents = git_bytes("rev-list", "--parents", "-n", "1", commit).split()[1:]
            message = git_bytes("cat-file", "commit", commit).split(b"\n\n", 1)[1]
            subject = message.split(b"\n", 1)[0].decode("utf-8", errors="replace")
            print(f">>> {commit[:10]} >>> {subject}", flush=True)
            diff_path.write_bytes(git_bytes("diff", f"{commit}^..{commit}"))
            message_path.write_bytes(validation_message(message, len(parents)))
            # Every commit retains the same upstream first-parent diff validation.
            diff_result = subprocess.run([str(hooks / "check-diff.py"), str(diff_path)])
            message_result = subprocess.run([
                str(hooks / "check-message.py"), str(message_path), "server",
            ])
            failed |= diff_result.returncode != 0 or message_result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
