#!/usr/bin/env python3
"""Fresh-process resource generation, reload, and native multiplayer map-join checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from test_wp11_slice import Engine, ROOT, require


def status(engine):
    return json.loads(engine.command('resource_sites', 'RESOURCE state '))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/resource-generation')
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    base = (ROOT / 'demo/wp11_slice.cfg').read_text().split('[newgrf]')[0]
    base = base.replace('map_y = 7', 'map_y = 9').replace('autosave_on_exit = true', 'autosave_on_exit = false')
    base = base.replace('threaded_saves = true', 'threaded_saves = false')
    report = {'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'cases': [], 'passed': False}
    for name, mode, density in [('default', None, 3), ('funding-only', True, 0), ('classic', False, 3)]:
        text = base.replace('player_built_economy = false\n', '')
        if mode is not None:
            text = text.replace('[game_creation]', f'[game_creation]\nplayer_built_economy = {str(mode).lower()}')
        text = text.replace('[game_creation]', f'[game_creation]\nresource_density = {density}')
        text = text.replace('industry_density = 0', f'industry_density = {density}')
        config = output / f'{name}.cfg'
        config.write_text(text)
        engine = Engine(binary, config, output, name, world_count=3)
        clients = []
        streams = []
        try:
            initial = status(engine)
            require(initial['player_built'] == (mode is not False), f'{name}: wrong economy mode')
            if mode is not False:
                require(initial['unoccupied_sites'] > 0, f'{name}: no hidden sites')
                require(initial['processing'] == initial['advanced'] == 0, f'{name}: invalid starter types')
                require((initial['industries'] == 0) == (density == 0), f'{name}: incorrect starter count')
            else:
                require(initial['sites'] == 0 and initial['industries'] > 0, 'Classic generation changed')
            if name == 'default':
                for i in range(2):
                    config_client = output / f'observer{i}.cfg'
                    config_client.write_text(f'[network]\nclient_name = Resource Observer {i}\n[gui]\nautosave_on_exit = false\n')
                    stream = (output / f'observer{i}.log').open('w')
                    streams.append(stream)
                    clients.append(subprocess.Popen([str(binary), '-n', f'127.0.0.1:{engine.port}#255',
                        '-c', str(config_client), '-x', '-v', 'sdl', '-s', 'null', '-m', 'null', '-d', 'net=2,desync=2'],
                        cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                        env={**os.environ, 'SDL_VIDEODRIVER': 'dummy'}))
                    engine.wait(f'Resource Observer {i} has joined the game')
                engine.command('unpause', 'unpaused')
                time.sleep(2)
                for i, client in enumerate(clients):
                    require(client.poll() is None, f'Observer {i} exited')
                    text_log = (output / f'observer{i}.log').read_text().lower()
                    require(not any(x in text_log for x in ('desync error', 'sync error detected', 'assertion failed', 'crash encountered')), f'Observer {i} failed')
            save = output / f'{name}.sav'
            engine.save(save)
            before = status(engine)
        finally:
            for client in clients:
                client.terminate()
                try:
                    client.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    client.kill()
                    client.wait()
            for stream in streams:
                stream.close()
            engine.close()
        loaded = Engine(binary, config, output, f'{name}-reload', save=save, world_count=3)
        try:
            after = status(loaded)
            require(before == after, f'{name}: resource state changed after cold reload')
        finally:
            loaded.close()
        report['cases'].append({'name': name, 'initial': initial, 'reloaded': after, 'joined_clients': len(clients)})
        print(name, initial, 'reload PASS', flush=True)
    report['passed'] = True
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
