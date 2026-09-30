#!/usr/bin/env python3
"""Compare the four-platform static artwork on isolated native WP11 fixtures.

The baseline and candidate use identical published industry/rail files. The art
GRF enters the candidate's static list before map generation. No GRFs are edited
inside saves. Graphical samples require SDL2, Xvfb, libX11 and libXtst; they are
rendering evidence, not human UAT or a general performance benchmark.
"""
import argparse
from contextlib import contextmanager
import ctypes
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

from test_wp11_slice import Engine, ROOT, catalog, console_payload, require
import test_wp11_slice as fixture

GRFID = "4f535406"
PACKS = ("openspacettd_industry.grf", "openspacettd_rail.grf")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def configuration(output, grf=None):
    """Pin native fixture inputs; disposable config cannot autosave or run ahead."""
    output.mkdir(parents=True, exist_ok=True)
    text = (ROOT / "demo/wp11_slice.cfg").read_text()
    text = text.replace("threaded_saves = true", "threaded_saves = false")
    text = text.replace("autosave_on_exit = true", "autosave_on_exit = false")
    text = text.replace("min_active_clients = 0", "min_active_clients = 1")
    text = text.replace("[gui]\n", "[gui]\nnewgrf_developer_tools = true\nzoom_min = 0\nzoom_max = 5\n"
                        "sprite_zoom_min = 0\nstation_numtracks = 1\n"
                        "station_platlength = 3\nstation_dragdrop = false\n")
    for name in PACKS:
        text = text.replace(f"{name} =", f"{ROOT / 'bin/newgrf' / name} =")
    text += "\n[misc]\nlanguage = english.lng\ngui_scale = 100\n"
    if grf:
        text += f"\n[newgrf-static]\n{grf} =\n"
    config = output / "platform-art.cfg"
    config.write_text(text)
    return config


def profile_environment(output):
    """Give every process its own data/config/cache trees and scanned content."""
    env = {}
    for kind in ("DATA", "CONFIG", "CACHE"):
        directory = output / "profile" / kind.lower()
        directory.mkdir(parents=True, exist_ok=True)
        env[f"XDG_{kind}_HOME"] = str(directory)
    data = Path(env["XDG_DATA_HOME"]) / "openttd"
    (data / "newgrf").mkdir(parents=True, exist_ok=True)
    # Read the installed base graphics from the environment, without changing it.
    base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "openttd/baseset"
    if base.is_dir() and not (data / "baseset").exists():
        (data / "baseset").symlink_to(base.resolve(), target_is_directory=True)
    for path in (ROOT / "bin/newgrf").glob("*.grf"):
        link = data / "newgrf" / path.name
        if not link.exists():
            link.symlink_to(path.resolve())
    return env


@contextmanager
def environment(values):
    previous = {key: os.environ.get(key) for key in values}
    os.environ.update(values)
    try:
        yield
    finally:
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


class ProfileEngine(Engine):
    """Keep the existing native runner while isolating its personal folders."""
    def __init__(self, binary, config, output, name, save=None, **kwargs):
        original_root = fixture.ROOT
        try:
            # Engine's only ROOT use during launch is cwd. An explicit -c makes
            # cwd the personal directory as well, so keep both in this profile.
            fixture.ROOT = output
            with environment(profile_environment(output)):
                super().__init__(binary, config, output, name, save, **kwargs)
        finally:
            fixture.ROOT = original_root


def collect(engine, command):
    """Drain a console command through an echo barrier, including multi-line output."""
    start = len(engine.lines)
    # Dedicated stdin uses getline plus fd readiness: batching raw input lines can
    # strand a buffered second line. Native exec supplies an explicit barrier.
    script = Path(engine.log.name).parent / "console-command.scr"
    script.write_text(command + "\necho PLATFORM_ART_BARRIER\n")
    engine.command(f'exec "{script}"', "PLATFORM_ART_BARRIER")
    return [console_payload(line) for line in engine.lines[start:]
            if "PLATFORM_ART_BARRIER" not in line]


def snapshot(engine):
    return {"state": engine.state(), "date": engine.command("getdate", "Date: ")}


def rss(pid):
    """Linux process RSS/high-water mark; machine noise prevents hard budgets here."""
    fields = {}
    for line in Path(f"/proc/{pid}/status").read_text().splitlines():
        if line.startswith(("VmRSS:", "VmHWM:")):
            key, value, unit = line.split()
            fields[key.rstrip(":") + "_KiB"] = int(value)
    return fields


def native_run(binary, config, output, steps, expected=None):
    report = {"advances": [], "passed": False}
    engine = ProfileEngine(binary, config, output, "fresh", seed=11)
    engine.deadline = time.monotonic() + 600
    try:
        # Saving this disposable profile serializes only GRFs admitted by the
        # engine's config load/safety scan. Profiling is unavailable on dedicated.
        engine.command("saveconfig", "Saved config.")
        admitted = config.read_text()
        report["static_config_after_admission"] = admitted.split("[newgrf-static]", 1)[-1].split("[", 1)[0].strip().splitlines() if "[newgrf-static]" in admitted else []
        report["catalog"] = catalog(engine)
        engine.command("wp11_slice prepare", "WP11 prepared")
        engine.command("wp11_slice build", "WP11 built")
        report["initial"] = snapshot(engine)
        if expected:
            require(report["catalog"] == expected["catalog"], "Art changed active content catalog")
            require(report["initial"] == expected["initial"], "Art changed initial physical/cash/date state")
        engine.save(output / "operational.sav")
        for i in range(steps):
            started = time.monotonic()
            state = engine.advance()
            row = {"state": state, "date": engine.command("getdate", "Date: ")}
            report["advances"].append(row)
            report.setdefault("advance_wall_seconds", []).append(time.monotonic() - started)
            if expected:
                require(row == expected["advances"][i], f"Art changed simulation at bounded advance {i + 1}")
            if i == 3:
                report["render_checkpoint"] = snapshot(engine)
                engine.save(output / "render-checkpoint.sav")
            if i >= 3 and state["deposited"] >= 120:
                break
        require(report["advances"][-1]["state"]["deposited"] >= 120,
                "Fewer than 120 steel units deposited in bounded fixture")
        amounts = [row["state"]["deposited"] for row in report["advances"]]
        report["deposit_increase_events"] = sum(after > before for before, after in zip([0] + amounts, amounts))
        require(report["deposit_increase_events"] >= 3, "Fewer than three distinct deposit observations")
        report["delivered_steel_units"] = report["advances"][-1]["state"]["deposited"]
        report["final"] = snapshot(engine)
        report["memory"] = rss(engine.process.pid)
        engine.save(output / "completed.sav")
    finally:
        engine.close()
    engine = ProfileEngine(binary, config, output, "cold-reload", output / "completed.sav")
    try:
        require(snapshot(engine) == report["final"], "Cold save/reload changed cargo/cash/vehicles/date")
        report["cold_reload_equal"] = True
        continued = engine.advance()
        report["continuation"] = {"state": continued, "date": engine.command("getdate", "Date: ")}
        if expected:
            require(report["continuation"] == expected["continuation"], "Art changed post-reload route continuation")
        engine.save(output / "continued.sav")
    finally:
        engine.close()
    report["saves_sha256"] = {p.name: sha256(p) for p in output.glob("*.sav")}
    report["passed"] = True
    (output / "native-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


class XKeyboard:
    """Drive only this harness's private X server using ordinary key/mouse events."""
    def __init__(self, display):
        self.x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
        self.xtst = ctypes.CDLL(ctypes.util.find_library("Xtst"))
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        self.x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XKeysymToKeycode.restype = ctypes.c_uint
        self.x11.XFlush.argtypes = [ctypes.c_void_p]
        self.x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        self.xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        self.xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
        self.xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        self.display = self.x11.XOpenDisplay(display.encode())
        require(self.display, "Cannot open private Xvfb display")

    def key(self, symbol, release=True):
        keysym = ord(symbol) if len(symbol) == 1 else self.x11.XStringToKeysym(symbol.encode())
        code = self.x11.XKeysymToKeycode(self.display, keysym)
        require(code, f"No X keycode for {symbol!r}")
        self.xtst.XTestFakeKeyEvent(self.display, code, 1, 0)
        if release:
            self.xtst.XTestFakeKeyEvent(self.display, code, 0, 0)
        self.x11.XFlush(self.display)

    def text(self, text):
        shifted = dict(zip('~!@#$%^&*()_+{}|:"<>?', '`1234567890-=[]\\;\',./'))
        for char in text:
            symbol = {" ": "space", "\n": "Return"}.get(char, char)
            shift = char.isupper() or char in shifted
            if shift:
                self.key("Shift_L", release=False)
                symbol = shifted.get(char, char.lower())
            self.key(symbol)
            if shift:
                code = self.x11.XKeysymToKeycode(self.display, self.x11.XStringToKeysym(b"Shift_L"))
                self.xtst.XTestFakeKeyEvent(self.display, code, 0, 0)
                self.x11.XFlush(self.display)
        self.x11.XFlush(self.display)

    def click(self, x, y):
        self.xtst.XTestFakeMotionEvent(self.display, 0, x, y, 0)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 1, 0)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 0, 0)
        self.x11.XFlush(self.display)

    def close(self):
        self.x11.XCloseDisplay(self.display)


class GraphicalEngine(Engine):
    """Reuse Engine's command/state/save assertions through the native SDL console."""
    def __init__(self, binary, config, output, save, display, blitter="32bpp-anim"):
        self.log_path = output / "sdl-console.log"
        self.log = (output / "sdl-transcript.log").open("w")
        self.raw_log = (output / "sdl-process.log").open("w")
        self.lines = []
        self.queue = queue.Queue()
        self.deadline = time.monotonic() + 600
        self.keyboard = None
        self.output = output
        self.config = config
        scripts = output / "scripts"
        scripts.mkdir(exist_ok=True)
        (scripts / "game_start.scr").write_text(
            f'script "{self.log_path}"\necho PLATFORM_SDL_READY\nscript\n')
        self.profile = profile_environment(output)
        self.process = subprocess.Popen(
            [str(binary), "-v", "sdl", "-b", blitter, "-s", "null", "-m", "null",
             "-r", "1024x768", "-x", "-c", str(config), "-g", str(save)],
            cwd=output, stdout=self.raw_log, stderr=subprocess.STDOUT,
            env={**os.environ, **self.profile, "DISPLAY": display, "OPENSPACETTD_WORLD_COUNT": "1"})
        threading.Thread(target=self.read, daemon=True).start()
        try:
            self.keyboard = XKeyboard(display)
            self.wait("PLATFORM_SDL_READY")
            self.keyboard.key("grave")
            time.sleep(0.2)
        except BaseException:
            self.close()
            raise

    def read(self):
        stream = None
        try:
            while self.process.poll() is None:
                if stream is None and self.log_path.is_file():
                    stream = self.log_path.open()
                line = stream.readline() if stream else ""
                if line:
                    self.queue.put(line)
                else:
                    time.sleep(0.02)
        finally:
            if stream:
                for line in stream:
                    self.queue.put(line)
                stream.close()
            self.queue.put(None)

    def command(self, command, marker):
        script = self.output / "sdl-command.scr"
        # Native GUI console file output is buffered. Closing each bounded script
        # flushes it, permitting the inherited Engine marker/assertion protocol.
        script.write_text(f'script "{self.log_path}"\n{command}\nscript\n')
        self.keyboard.text(f'exec "{script}"\n')
        return self.wait(marker)

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        if self.keyboard:
            self.keyboard.close()
        self.raw_log.close()
        self.log.close()


def screenshot(engine, path):
    started = time.monotonic()
    name = f"{engine.output.name}-{path.stem}"
    actual = engine.config.parent / "screenshot" / (name + ".png")
    require(not actual.exists(), "Use a fresh SDL output name; preserve prior captures")
    collect(engine, f'screenshot viewport no_con "{name}"')
    deadline = time.monotonic() + 20
    while not (actual.is_file() and actual.read_bytes().endswith(b'IEND\xaeB`\x82')):
        require(engine.process.poll() is None, "SDL process exited during screenshot")
        require(time.monotonic() < deadline, f"SDL screenshot missing or incomplete: {path}")
        time.sleep(0.05)
    actual.rename(path)
    # no_con closes the native console. Open it again only after the frame is captured.
    engine.keyboard.key("grave")
    time.sleep(0.1)
    return {"file": path.name, "sha256": sha256(path),
            "capture_wall_seconds": time.monotonic() - started}


def axis_y_scene(engine, output):
    """Add one disconnected native station through the UI, on a disposable copy."""
    before = snapshot(engine)
    # This graphical-only clone permits ordinary native GUI construction while
    # paused. It is kept separate from the unmodified paired route proof.
    collect(engine, "setting construction.command_pause_level 3")
    collect(engine, "zoomto 1")
    collect(engine, "scrollto instant 32 60")
    engine.keyboard.key("grave")
    engine.keyboard.key("a")  # Native global autorail hotkey opens rail toolbar.
    time.sleep(0.3)
    engine.keyboard.key("9")  # Native station selector.
    time.sleep(0.3)
    # At the pinned 1024x768 / 100% GUI scale, orientation Y is the right preview.
    engine.keyboard.click(545, 113)
    time.sleep(0.1)
    engine.keyboard.click(512, 384)
    time.sleep(0.4)
    engine.keyboard.key("Delete")
    time.sleep(0.1)
    engine.keyboard.key("grave")
    time.sleep(0.1)
    after = snapshot(engine)
    require(len(after["state"]["stations"]) == len(before["state"]["stations"]) + 1,
            "Native Axis Y UI station was not constructed")
    require(after["date"] == before["date"] and after["state"]["trains"] == before["state"]["trains"],
            "Graphical station construction advanced the paused route")
    require(after["state"]["money"] < before["state"]["money"], "Native station construction was not paid")
    collect(engine, "setting construction.command_pause_level 1")
    report = {"before": before, "after": after, "captures": [],
              "construction": "One disconnected Axis Y, one-track/three-tile default station; native SDL UI; paid in disposable clone"}
    for zoom, name in ((2, "normal"), (1, "2x")):
        collect(engine, f"zoomto {zoom}")
        collect(engine, "scrollto instant 32 61")
        time.sleep(0.3)
        report["captures"].append(screenshot(engine, output / f"axis-y-{name}.png"))
    engine.save(output / "both-axes-review.sav")
    time.sleep(0.2)
    engine.keyboard.key("grave")
    time.sleep(0.2)
    engine.keyboard.key("x")  # Native transparency toggle, client appearance only.
    time.sleep(0.2)
    engine.keyboard.key("grave")
    time.sleep(0.2)
    report["captures"].append(screenshot(engine, output / "axis-y-2x-transparent.png"))
    require(snapshot(engine) == after, "Graphical-only transparency capture changed route state")
    return report


def graphical_run(binary, config, output, save, expected, blitter="32bpp-anim", axes=True):
    """Record actual viewport frames; the WP11 scene itself has only Axis X."""
    output.mkdir(parents=True, exist_ok=True)
    xlog = (output / "xvfb.log").open("w")
    xvfb = subprocess.Popen(["Xvfb", "-displayfd", "1", "-screen", "0", "1024x768x24", "-nolisten", "tcp"],
                            stdout=subprocess.PIPE, stderr=xlog, text=True)
    engine = None
    report = {"captures": [], "human_uat": "Pending", "axis_x_map": True,
              "axis_y_map": False, "blitter": blitter,
              "performance_scope": "Paused small SDL/Xvfb scene; diagnostic samples only"}
    try:
        display_number = xvfb.stdout.readline().strip()
        require(display_number.isdigit(), "Private Xvfb display did not start")
        engine = GraphicalEngine(binary, config, output, save, f":{display_number}", blitter)
        require(snapshot(engine) == expected, "SDL load changed fixture physical/cash/date state")
        # Screenshot success warnings otherwise obscure the next viewport frame.
        # This client-only setting leaves critical errors visible and simulation intact.
        collect(engine, "setting gui.errmsg_duration 0")
        report["screenshot_success_popups_suppressed"] = True
        report["loaded_grfs"] = collect(engine, "newgrf_profile list")
        for label, tile in (("mine", (21, 40)), ("refinery", (146, 43))):
            for zoom, zoom_name in ((2, "normal"), (1, "2x")):
                collect(engine, f"zoomto {zoom}")
                collect(engine, f"scrollto instant {tile[0]} {tile[1]}")
                time.sleep(0.5)
                report["captures"].append(screenshot(engine, output / f"{label}-{zoom_name}.png"))
                report.setdefault("fps_samples", []).append({"scene": label, "zoom": zoom_name,
                                                              "console": collect(engine, "fps"),
                                                              "memory": rss(engine.process.pid)})
        require(snapshot(engine) == expected, "Paused SDL capture advanced or changed simulation")
        report["paused_capture_state_equal"] = True
        if axes:
            report["axis_y"] = axis_y_scene(engine, output)
            report["axis_y_map"] = True
        report["passed"] = True
    finally:
        if engine:
            engine.close()
        xvfb.terminate()
        xvfb.wait(timeout=5)
        xlog.close()
        (output / "graphical-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def run(args):
    binary = args.binary.resolve()
    grf = args.grf.resolve()
    output = args.output.resolve()
    require(not (output / "evidence.json").exists(), "Use a fresh output directory; preserve previous evidence")
    output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "human_uat": "Pending", "fixture": "wp11_slice v1, authored £100m showcase",
              "binary": str(binary), "binary_sha256": sha256(binary), "candidate": str(grf),
              "candidate_sha256": sha256(grf), "published_sha256": {name: sha256(ROOT / "bin/newgrf" / name) for name in PACKS},
              "scope": "Four default platform sprites, static GRF, exact native fixture equality; no A1/A2 proof"}
    try:
        configs = {name: configuration(output / name, grf if name == "candidate" else None)
                   for name in ("baseline", "candidate")}
        baseline = native_run(binary, configs["baseline"], output / "baseline", args.steps)
        candidate = native_run(binary, configs["candidate"], output / "candidate", args.steps, baseline)
        require(not any(GRFID in row.lower() for row in baseline["static_config_after_admission"]), "Art unexpectedly loaded in baseline")
        require(any(GRFID in row.lower() and grf.name in row for row in candidate["static_config_after_admission"]),
                "Candidate absent after engine static-safety admission")
        report["static_grf_admitted"] = True
        report["native"] = {"baseline": baseline, "candidate": candidate}
        # A static art GRF should not become a required saved content dependency.
        engine = ProfileEngine(binary, configs["baseline"], output, "candidate-without-art", output / "candidate/completed.sav")
        try:
            require(snapshot(engine) == candidate["final"], "Removing static art changed cold-loaded physical state")
            state = engine.advance()
            require({"state": state, "date": engine.command("getdate", "Date: ")} == candidate["continuation"],
                    "Removing static art changed native continuation")
            report["static_art_removable_without_save_mutation"] = True
        finally:
            engine.close()
        if not args.headless_only:
            report["graphical"] = {name: graphical_run(binary, configs[name], output / name / "sdl",
                                                    output / name / "render-checkpoint.sav", row["render_checkpoint"])
                                   for name, row in (("baseline", baseline), ("candidate", candidate))}
            require(report["graphical"]["baseline"]["axis_y"]["after"] == report["graphical"]["candidate"]["axis_y"]["after"],
                    "Art changed native UI station construction/cargo/cash/date")
            report["graphical"]["candidate_8bpp"] = graphical_run(
                binary, configs["candidate"], output / "candidate/sdl-8bpp", output / "candidate/render-checkpoint.sav",
                candidate["render_checkpoint"], "8bpp-optimized")
        else:
            report["graphical"] = {"status": "Not run (--headless-only)"}
        require(sha256(binary) == report["binary_sha256"], "Binary changed during evidence run")
        require(sha256(grf) == report["candidate_sha256"], "Candidate GRF changed during evidence run")
        require({name: sha256(ROOT / "bin/newgrf" / name) for name in PACKS} == report["published_sha256"],
                "Published native content changed during evidence run")
        report["passed"] = True
    finally:
        (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PLATFORM ART PASS: static admission, paired native state, cold reload, continuation. Evidence: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--grf", type=Path, default=ROOT / "bin/newgrf/openspacettd_platforms_v1.grf")
    parser.add_argument("--output", type=Path, default=ROOT / "build/commonwealth-platform-art")
    parser.add_argument("--steps", type=int, default=240, help="Maximum bounded 1024-tick advances")
    parser.add_argument("--headless-only", action="store_true")
    args = parser.parse_args()
    try:
        run(args)
    except Exception as error:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / "failure.json").write_text(json.dumps({"passed": False, "error": str(error)}, indent=2) + "\n")
        raise
