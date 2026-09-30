#!/usr/bin/env python3
"""Launch a disposable, authored platform art scene through the native engine.

This is an art review fixture with authored capital and infrastructure. It does
not establish ordinary gameplay progression. Existing player profiles and saves
are never edited; quitting leaves the generated review profile for inspection.
"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from test_commonwealth_platform_art import ROOT, configuration, native_run, profile_environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--grf", type=Path, default=ROOT / "bin/newgrf/openspacettd_platforms_v1.grf")
    parser.add_argument("--baseline", action="store_true", help="launch the original platforms for comparison")
    parser.add_argument("--save", type=Path, help="use a copied review save instead of creating the native fixture")
    args = parser.parse_args()
    profile = Path(tempfile.mkdtemp(prefix="ost-platform-review-"))
    config = configuration(profile, None if args.baseline else args.grf.resolve())
    if args.save:
        save = profile / "review.sav"
        save.write_bytes(args.save.read_bytes())
    else:
        native_run(args.binary.resolve(), config, profile, 240)
        save = profile / "render-checkpoint.sav"
    env = dict(os.environ)
    env.update(profile_environment(profile))
    (profile / "scripts").mkdir(exist_ok=True)
    (profile / "scripts/game_start.scr").write_text("zoomto 1\nscrollto instant 21 40\n")
    print(f"Disposable art review profile: {profile}")
    print("Review the four platform sprites at normal/2x zoom; human acceptance is Pending.")
    print("Authored WP11 showcase: this is not the A1 ordinary-start mission.")
    subprocess.run([str(args.binary.resolve()), "-x", "-c", str(config), "-g", str(save),
                    "-v", "sdl", "-b", "32bpp-anim", "-s", "null", "-m", "null"],
                   cwd=profile, env=env, check=True)


if __name__ == "__main__":
    main()
