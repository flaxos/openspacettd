#!/usr/bin/env python3
"""Check cargo text, viewport geometry and reload on a connected UAT save copy."""
import argparse
import hashlib
import json
from pathlib import Path

from test_wp11_slice import Engine, ROOT, require


def audit(engine):
    result = json.loads(engine.command('connected_economy audit', 'CONNECTED audit '))
    require(not result['invalid_slopes'], 'Invalid terrain remains')
    require(result['pixel_queries'] == 3 * 1023 * 1023, 'Incomplete pixel-height coverage')
    require(result['viewport_queries'] == 1023 * 1023, 'Incomplete viewport coverage')
    require(len(result['cargo']) == 24, 'Missing native or Commonwealth cargo')
    for cargo in result['cargo']:
        for field, value in cargo.items():
            if field != 'id':
                require(value and '(undefined' not in value and '(invalid' not in value,
                        f'Invalid cargo {cargo["id"]} {field}: {value}')
        if cargo['id'] >= 16:
            require(cargo['large_quantity'].startswith('100,000'), 'Truncated large cargo quantity')
    return result


def run(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = args.save.resolve()
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    config = output / 'recovery.cfg'
    template = (ROOT / 'demo/connected_economy.cfg').read_text().replace('threaded_saves = true', 'threaded_saves = false').replace('min_active_clients = 0', 'min_active_clients = 1').replace('autosave_interval = 10', 'autosave_interval = 0')
    # Exercise the player's English-US fallback, and British English independently.
    results = {}
    original = None
    recovered = output / 'OpenSpaceTTD-Recovered-UAT.sav'
    for language in ('english_US.lng', 'english.lng'):
        text = '\n'.join(line for line in template.splitlines() if not line.startswith('language ='))
        text += f'\n[misc]\nlanguage = {language}\n'
        config.write_text(text)
        engine = Engine(args.binary.resolve(), config, output, language, source)
        try:
            state = json.loads(engine.command('connected_economy status', 'CONNECTED state '))
            results[language] = audit(engine)
            require(results[language]['language'] == ('en_US' if language == 'english_US.lng' else 'en_GB'), 'Wrong language loaded')
            if original is None:
                original = state
                engine.save(recovered)
            else:
                require(state == original, 'Language selection changed simulation state')
        finally:
            engine.close()
    engine = Engine(args.binary.resolve(), config, output, 'reloaded', recovered)
    try:
        state = json.loads(engine.command('connected_economy status', 'CONNECTED state '))
        # Persistent buffers omit zero-valued entries on disk.
        for snapshot in (original, state):
            for facility in snapshot['facilities']:
                for field in ('inputs', 'outputs'):
                    facility[field] = [pair for pair in facility[field] if pair[1]]
        require(state == original, 'Saving/reloading recovery changed simulation state')
        results['reload'] = audit(engine)
        for _ in range(4):
            state = json.loads(engine.command('connected_economy advance', 'CONNECTED advance '))
            require(state['conserved'], 'Recovery failed cargo conservation')
            require(not any(t['crashed'] for t in state['trains']), 'Train crashed after recovery')
        results['after_running'] = audit(engine)
        results['city'] = original['cities']
        results['trains'] = len(original['trains'])
        results['passed'] = True
        results['source_sha256'] = source_hash
        results['recovered_sha256'] = hashlib.sha256(recovered.read_bytes()).hexdigest()
        require(hashlib.sha256(source.read_bytes()).hexdigest() == source_hash, 'Original save changed')
        (output / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')
        print(f'PASS: 24 cargo types, full-map pixel/viewport audit, two languages, reload and live simulation. Recovered save: {recovered}')
    finally:
        engine.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save', type=Path, default=ROOT / 'demo/OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/connected-recovery')
    run(parser.parse_args())
