#!/usr/bin/env python3
"""Author, prove and play the Alien Frontier map "The Hollowfrost Survey".

prepare  Generate a fresh 512x256 game, author Hollowfrost over it, let the mission
         script brief the company while paused, audit terrain and text, save the
         opening and check that a cold reload is identical.
prove    On a copy of an opening, build the reference rail service, run the real
         server loop at full speed until all three survey goals complete (bounded
         by game years), then save and cold-reload the result.
shots    Render review screenshots of a save on a private SDL offscreen display.
play     Copy an opening into a new player profile and launch the desktop game.

Every run writes to a new output directory and never modifies its input save.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path

from test_wp11_slice import Engine, ROOT, require

CONFIG = ROOT / 'demo/alien_worlds/hollowfrost.cfg'
PACKS = ('integrated_v1', 'rail_v3', 'equipment_v1', 'gates_v1')
SEED = 2026
YEAR = 2150


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_config(binary, folder, hold=False):
    """Config copy whose content packs resolve beside the chosen binary.

    hold keeps a dedicated server paused from generation onwards, so authoring
    starts from the same tick and the opening is byte-reproducible.
    """
    text = CONFIG.read_text()
    if hold:
        text = text.replace('min_active_clients = 0', 'min_active_clients = 1')
    for pack in PACKS:
        text = text.replace(f'openspacettd_{pack}.grf =', f'{binary.parent}/newgrf/openspacettd_{pack}.grf =')
    config = folder / 'hollowfrost.cfg'
    config.write_text(text)
    return config


class Console:
    """Synchronous console: one stdin line per command, acknowledged by a unique echo marker.

    The dedicated server polls stdin before each read, so a second buffered line
    would wait for more input; a retained exec script keeps each command atomic.
    """
    def __init__(self, engine, folder):
        self.engine = engine
        self.folder = folder
        self.sequence = 0

    def run(self, command):
        self.sequence += 1
        marker = f'ALIEN_ACK_{self.sequence}'
        script = self.folder / f'console-{self.sequence:03}.scr'
        require('"' not in str(script) and '\n' not in str(script), 'Console script path cannot be quoted')
        script.write_text(f'{command}\necho {marker}\n')
        return self.engine.command(f'exec "{script}"', marker)

    def json(self, command, marker):
        return json.loads(self.engine.command(command, marker))

    def status(self):
        return self.json('alienworld status', 'ALIEN state ')


def stable(state):
    """Fields that must survive a save and cold reload unchanged."""
    return {key: state[key] for key in ('date', 'company', 'money', 'rules', 'worlds', 'industries', 'stations', 'stock', 'goals', 'pages', 'portals')} | {
        'trains': [{key: train[key] for key in ('id', 'tile', 'world', 'orders', 'stopped', 'load')} for train in state['trains']]}


def check_opening(state, audit):
    require(state['rules'] == 2 and [w['name'] for w in state['worlds']] == ['Hollowfrost', 'Kestrel Forge'], 'Works rules on both authored worlds')
    require([(w['biome'], w['phase'], w['role']) for w in state['worlds']] == [(1, 3, 0), (4, 2, 1)], 'Sub-arctic frontier and volcanic industrial roles')
    require(sorted(i['local'] for i in state['industries']) == [0x14, 0x18, 0x1B, 0x1C] and all(i['founder'] == 0 for i in state['industries']), 'Four charter industries')
    require(all(not i['produced'] or set(i['produced'].values()) == {0} for i in state['industries']), 'Industry stores start empty')
    require(all(n == 0 for world in state['stock'].values() for n in world.values()), 'World stockpiles start empty')
    require(len(state['stations']) == 5 and not any(s['waiting'] for s in state['stations']), 'Five empty platforms')
    require(len(state['trains']) == 1 and state['trains'][0]['stopped'] and state['trains'][0]['orders'] == 0, 'One idle starter train')
    require(state['portals'] == 1 and state['money'] == 2000000, 'One public gate pair and the declared charter cash')
    require(len(state['pages']) == 1 and [g['completed'] for g in state['goals']] == [False, False, False], 'Mission briefing and three open goals')
    require(audit['invalid_slopes'] == 0, 'Valid terrain')
    require(audit['water'] > 500 and audit['snow'] > 20000 and audit['rough'] > 20000 and audit['rocks'] > 2000 and audit['trees'] > 500, 'Lakes, snow, basalt, exposed rock and groves')
    for cargo in audit['cargos']:
        require(all(cargo[k] and 'Undefined' not in cargo[k] for k in ('name', 'units', 'quantity', 'abbreviation')), f'Cargo text for {cargo["label"]}')


def prepare(args):
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = write_config(binary, output, hold=True)
    report = {'passed': False, 'human_uat': 'Pending', 'binary_sha256': sha256(binary), 'config_sha256': sha256(config)}
    engine = loaded = None
    try:
        engine = Engine(binary, config, output, 'prepare', year=YEAR, world_count=1, seed=SEED)
        console = Console(engine, output)
        report['receipt'] = console.json('alienworld prepare hollowfrost', 'ALIEN prepared ')
        deadline = time.monotonic() + 60
        while True:
            state = console.status()
            if state['pages'] and len(state['goals']) == 3:
                break
            require(time.monotonic() < deadline, 'Mission script did not brief the company while paused')
            time.sleep(0.5)
        audit = console.json('alienworld audit', 'ALIEN audit ')
        report['opening'], report['audit'] = state, audit
        check_opening(state, audit)
        save = output / 'hollowfrost-opening.sav'
        engine.save(save)
        engine.close()
        engine = None
        report['save_sha256'] = sha256(save)
        loaded = Engine(binary, config, output, 'reload', save=save, year=YEAR, world_count=1, seed=SEED)
        restored = Console(loaded, output).status()
        require(stable(restored) == stable(state), 'Cold reload changed the opening')
        report['passed'] = True
        print(f'Hollowfrost opening prepared and reloaded: {save}', flush=True)
    finally:
        for instance in (engine, loaded):
            if instance is not None:
                instance.close()
        (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    if args.publish:
        args.publish.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(save, args.publish)
        print(f'Published {args.publish}', flush=True)


def prove(args):
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = write_config(binary, output)
    opening = output / 'opening-copy.sav'
    shutil.copyfile(args.opening, opening)
    report = {'passed': False, 'human_uat': 'Pending', 'opening_sha256': sha256(args.opening), 'binary_sha256': sha256(binary), 'samples': []}
    engine = loaded = None
    try:
        engine = Engine(binary, config, output, 'service', save=opening, year=YEAR, world_count=1, seed=SEED)
        engine.deadline = time.monotonic() + args.seconds + 120
        console = Console(engine, output)
        start = console.status()
        report['built'] = console.json('alienworld build', 'ALIEN built ')
        console.run('alienworld speed 0')
        console.run('unpause')
        limit = start['date'] + int(args.years * 365)
        wall = time.monotonic() + args.seconds
        report['goal_days'] = [None, None, None]
        while True:
            # Full speed covers roughly a month per quarter second; goal days are first seen, not exact.
            time.sleep(0.25)
            state = console.status()
            report['samples'].append({k: state[k] for k in ('date', 'money', 'goals', 'stock', 'trains', 'industries', 'stations')})
            train = state['trains'][0]
            require(not train['crashed'] and not train['lost'], f'Reference train failed: {train}')
            for n, goal in enumerate(state['goals']):
                if goal['completed'] and report['goal_days'][n] is None:
                    report['goal_days'][n] = state['date'] - start['date']
            if all(goal['completed'] for goal in state['goals']):
                break
            require(state['date'] < limit and time.monotonic() < wall, 'Survey goals did not complete within the bound')
        console.run('pause')
        console.run('alienworld speed 100')
        final = console.status()
        report['final'] = final
        report['game_days'] = final['date'] - start['date']
        require(len(final['pages']) >= 3, 'Contract offer and completion pages')
        save = output / 'hollowfrost-complete.sav'
        engine.save(save)
        engine.close()
        engine = None
        report['save_sha256'] = sha256(save)
        loaded = Engine(binary, config, output, 'reload', save=save, year=YEAR, world_count=1, seed=SEED)
        restored = Console(loaded, output).status()
        require(stable(restored) == stable(final), 'Cold reload changed the completed survey')
        report['passed'] = True
        print(f'Hollowfrost survey goals first seen complete at game days {report["goal_days"]}; saved and reloaded', flush=True)
    finally:
        for instance in (engine, loaded):
            if instance is not None:
                instance.close()
        (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')


def shots(args):
    """Minimap plus eight named views; nothing is drawn on the user's display."""
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = write_config(binary, output)
    (output / 'scripts').mkdir()
    (output / 'scripts/game_start.scr').write_text('screenshot minimap hollowfrost-minimap\nalienworld shots\n')
    save = output / 'shots-copy.sav'
    shutil.copyfile(args.save, save)
    env = {k: v for k, v in os.environ.items() if k not in ('DISPLAY', 'WAYLAND_DISPLAY')}
    env |= {'SDL_VIDEODRIVER': 'offscreen', 'SDL_RENDER_DRIVER': 'software'}
    with (output / 'shots.log').open('x') as log:
        result = subprocess.run([str(binary), '-v', 'sdl', '-r', '1920x1200', '-s', 'null', '-m', 'null', '-x', '-c', str(config), '-g', str(save)],
                                cwd=ROOT, env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    images = sorted((output / 'screenshot').glob('hollowfrost-*.png'))
    require(result.returncode == 0 and len(images) == 9, f'Screenshot run exited {result.returncode} with {len(images)} images')
    print('\n'.join(str(image) for image in images), flush=True)


def play(args):
    """Open a fresh copy of the opening in a new or empty profile, or resume the newest save of an earlier one."""
    binary = args.binary.resolve()
    profile = args.profile.resolve()
    config = profile / 'hollowfrost.cfg'
    saves = profile / 'save'
    if config.is_file() and saves.is_dir():
        save = max(saves.glob('*.sav'), key=lambda path: path.stat().st_mtime, default=None)
        require(save is not None, f'{saves} has no saves to resume')
        print(f'Resuming {save}', flush=True)
    else:
        require(not profile.exists() or (profile.is_dir() and not any(profile.iterdir())),
                f'{profile} already holds other files; choose a new or empty folder')
        profile.mkdir(parents=True, exist_ok=True)
        config = write_config(binary, profile)
        saves.mkdir()
        save = saves / 'Hollowfrost-Survey.sav'
        shutil.copyfile(args.opening, save)
        print(f'New profile {profile}; your copy of the opening is {save}', flush=True)
    subprocess.run([str(binary), '-c', str(config), '-g', str(save)], cwd=ROOT, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--publish', type=Path, help='Also copy the verified opening here')
    p = commands.add_parser('prove')
    p.add_argument('--opening', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--years', type=float, default=4.0, help='Game-time bound')
    p.add_argument('--seconds', type=float, default=900.0, help='Wall-clock bound')
    p = commands.add_parser('shots')
    p.add_argument('--save', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('play')
    p.add_argument('--opening', type=Path, default=ROOT / 'demo/alien_worlds/Hollowfrost-Survey.sav')
    p.add_argument('--profile', type=Path, required=True)
    args = parser.parse_args()
    {'prepare': prepare, 'prove': prove, 'shots': shots, 'play': play}[args.command](args)


if __name__ == '__main__':
    main()
