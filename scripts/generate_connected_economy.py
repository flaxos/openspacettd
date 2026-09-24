#!/usr/bin/env python3
"""Generate and verify the isolated, fully connected Commonwealth economy demo."""
import argparse
import hashlib
import json
from pathlib import Path
import time
from test_wp11_slice import Engine, ROOT, catalog, require


def month(state):
    return state['year'] * 12 + state['month']


def route_keys(state):
    result = {}
    require(len(state['trains']) == 37, 'Connected fixture must contain 37 operating trains')
    for train in state['trains']:
        for cargo in train['cargo_types']:
            key = (train['id'], cargo)
            require(key not in result, f'Duplicate delivery evidence key {key}')
            result[key] = train['name'] + f' cargo {cargo}'
    return result


def count_deliveries(counts, state):
    for station, quantities in state.get('vehicle_deliveries', []):
        for cargo, amount in enumerate(quantities):
            if amount:
                key = (station, cargo)
                counts[key] = counts.get(key, 0) + 1


def verified_advance(engine, report, phase):
    state = json.loads(engine.command('connected_economy advance', 'CONNECTED advance '))
    require(state['conserved'], f"Cargo mismatch: {state['errors']}")
    require(not any(t['crashed'] for t in state['trains']), 'Train crashed')
    require(state['money'] > 0, 'Company ran out of cash')
    report.setdefault(phase, []).append(state)
    return state


def acceptance(engine, config, output, args, report):
    import shutil
    state = report['initial']
    routes = route_keys(state)
    counts = {}
    streak = 0
    previous_month = month(state)
    initial_houses = state['cities'][0]['houses']
    for step in range(args.steps):
        state = verified_advance(engine, report, 'warmup')
        count_deliveries(counts, state)
        city = state['cities'][0]
        if month(state) != previous_month:
            streak = streak + 1 if all(a >= b for a, b in zip(city['last'], city['quotas'])) else 0
            previous_month = month(state)
        if step % 10 == 0:
            print(f"Warm-up {state['year']}-{state['month']+1}: houses={city['houses']}, supplied streak={streak}, routes={sum(counts.get(k, 0)>=2 for k in routes)}/{len(routes)}", flush=True)
        if streak >= 3 and city['houses'] > initial_houses and all(counts.get(k, 0) >= 2 for k in routes) and all(f['batches'] > 0 for f in state['facilities']):
            break
    else:
        raise RuntimeError('Warm-up did not prove every service and three consecutive fully supplied growth months')
    checkpoint = output / 'checkpoint.sav'
    engine.save(checkpoint)
    print('Growth checkpoint selected; running 24-month soak', flush=True)
    report['checkpoint'] = state
    report['routes'] = [{'name': name, 'delivery_intervals': counts[key]} for key, name in routes.items()]
    start = state
    counts = {}
    while month(state) - month(start) < 24:
        state = verified_advance(engine, report, 'soak')
        count_deliveries(counts, state)
    require(all(counts.get(k, 0) >= 2 for k in routes), 'A route failed to deliver repeatedly during the 24-month soak')
    require(all(a['batches'] > b['batches'] for a, b in zip(state['facilities'], start['facilities'])), 'A processing facility stopped producing')
    require(state['cities'][0]['houses'] > start['cities'][0]['houses'], 'City did not add houses during the soak')
    print('Soak passed; checking reload, food stop/restart, fabrication and research', flush=True)
    engine.close()
    probe = Engine(args.binary.resolve(), config, output, 'reload-and-starvation', checkpoint)
    probe.deadline = time.monotonic() + 1800
    try:
        loaded = json.loads(probe.command('connected_economy status', 'CONNECTED state '))
        def compact_buffers(facilities):
            for facility in facilities:
                for buffer in ('inputs', 'outputs'):
                    facility[buffer] = [pair for pair in facility[buffer] if pair[1]]
        compact_buffers(loaded['facilities'])
        compact_buffers(start['facilities'])
        for key in ('tick', 'date', 'money', 'held', 'stocks', 'facilities', 'cities', 'research'):
            require(loaded[key] == start[key], f'Reload changed {key}')
        report['reload_equal'] = True
        probe.command('connected_economy stop-food', 'CONNECTED food stopped')
        state = loaded
        while month(state) - month(loaded) < 3:
            state = verified_advance(probe, report, 'starvation')
        require(state['cities'][0]['state'] == 0 and state['cities'][0]['last'][0] == 0, 'Stopping food did not cause starvation')
        starved = state
        while month(state) - month(starved) < 3:
            state = verified_advance(probe, report, 'starvation')
        require(state['cities'][0]['houses'] <= starved['cities'][0]['houses'], 'City added houses while starved')
        probe.command('connected_economy start-food', 'CONNECTED food started')
        baseline = state
        while month(state) - month(baseline) < 12:
            state = verified_advance(probe, report, 'restoration')
            if state['cities'][0]['state'] == 3 and state['cities'][0]['houses'] > baseline['cities'][0]['houses']:
                break
        else:
            raise RuntimeError('Food restoration did not restore fully supplied city growth')
        report['starvation_and_recovery'] = True
        report['fabrication'] = json.loads(probe.command('connected_economy fabricate', 'CONNECTED fabricated '))
        require(report['fabrication']['exact'], 'CST prefab did not consume its exact material bill')
        probe.command('connected_economy research', 'CONNECTED research selected')
        for _ in range(4):
            verified_advance(probe, report, 'research')
        require(any(any(sample['research_consumed']) for sample in report['research']), 'Research did not consume delivered feedstock')
    finally:
        probe.close()
    report['visible_growth'] = visible_growth_evidence(report)
    report['passed'] = True
    report['save_sha256'] = hashlib.sha256(checkpoint.read_bytes()).hexdigest()
    if args.publish:
        args.publish.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(checkpoint, args.publish)
        report['published_save'] = str(args.publish)
    print('PASS: all routes, 24-month soak, reload, starvation and restored growth', flush=True)


def visible_growth_evidence(report):
    start = report['checkpoint']
    growth = next((sample for sample in report['soak']
                   if sample['cities'][0]['houses'] >= start['cities'][0]['houses'] + 3
                   and sample['cities'][0]['population'] > start['cities'][0]['population']
                   and (sample['tick'] - start['tick']) * 27 <= 600_000), None)
    require(growth is not None, 'No clear population and house growth within ten normal-speed minutes')
    return {
        'before': start['cities'][0], 'after': growth['cities'][0],
        'elapsed_ticks': growth['tick'] - start['tick'],
        'nominal_seconds_at_normal_speed': (growth['tick'] - start['tick']) * 0.027,
    }


def publish_evidence(report, save):
    """Publish compact review evidence and the complete reproducible run log."""
    import gzip
    require(report['passed'], 'Cannot publish failed acceptance evidence')
    start = report['checkpoint']
    report['visible_growth'] = visible_growth_evidence(report)
    report['human_visual_uat'] = 'pending'
    require(hashlib.sha256(save.read_bytes()).hexdigest() == report['save_sha256'], 'Published save hash differs')
    summary = {
        'passed': True, 'save': save.name, 'save_sha256': report['save_sha256'],
        'binary_sha256': report['binary_sha256'], 'content_manifest': report['content_manifest'],
        'startup': {'cash': 100_000_000, 'research': report['initial']['research'],
                    'cargo_seeded': False, 'growth_forced': False},
        'checkpoint': {'year': start['year'], 'month_zero_based': start['month'],
                       'city': start['cities'][0], 'trains': len(start['trains']),
                       'stations': len(start['stations']), 'facilities': start['facilities'],
                       'stockpiles': start['stocks']},
        'visible_growth': report['visible_growth'], 'routes': report['routes'],
        'soak_months': month(report['soak'][-1]) - month(start),
        'soak_end_city': report['soak'][-1]['cities'][0],
        'reload_equal': report['reload_equal'],
        'starvation_and_recovery': report['starvation_and_recovery'],
        'fabrication': report['fabrication'],
        'research_feedstock_consumed': sum(sum(s['research_consumed']) for s in report['research']),
        'human_visual_uat': 'pending', 'terrain_and_text': report['terrain_and_text'],
    }
    save.with_suffix('.evidence.json').write_text(json.dumps(summary, indent=2) + '\n')
    save.with_suffix('.run.json.gz').write_bytes(gzip.compress(json.dumps(report).encode(), mtime=0))


def run(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config = output / 'connected.cfg'
    text = (ROOT / 'demo/wp11_slice.cfg').read_text()
    for old, new in [('map_x = 9', 'map_x = 10'), ('map_y = 7', 'map_y = 10'),
                     ('landscape = temperate', 'landscape = arctic'),
                     ('threaded_saves = true', 'threaded_saves = false'),
                     ('autosave_on_exit = true', 'autosave_on_exit = false')]:
        text = text.replace(old, new)
    for name in ('industry', 'rail'):
        filename = 'openspacettd_industry_v4.grf' if name == 'industry' else 'openspacettd_rail_v3.grf'
        text = text.replace(f'openspacettd_{name}.grf =', f'{ROOT}/bin/newgrf/{filename} =')
    config.write_text(text)
    engine = Engine(args.binary.resolve(), config, output, 'connected', args.resume, year=1950)
    engine.deadline = time.monotonic() + 1800
    report = {'passed': False, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(), 'content_manifest': json.loads((ROOT / 'pkg/commonwealth_manifest.json').read_text()), 'samples': []}
    try:
        report['catalog'] = catalog(engine, native_refits=True)
        if not args.resume:
            print(engine.command('connected_economy prepare', 'CONNECTED prepared'), flush=True)
            engine.save(output / 'cold-start.sav')
        state = json.loads(engine.command('connected_economy status', 'CONNECTED state '))
        report['initial'] = state
        report['terrain_and_text'] = json.loads(engine.command('connected_economy audit', 'CONNECTED audit '))
        require(not report['terrain_and_text']['invalid_slopes'], 'Invalid terrain slopes')
        for cargo in report['terrain_and_text']['cargo']:
            require(all(value and '(undefined' not in value and '(invalid' not in value for key, value in cargo.items() if key != 'id'), f'Incomplete cargo text: {cargo}')
        if args.verify:
            acceptance(engine, config, output, args, report)
            return
        for step in range(args.steps):
            state = json.loads(engine.command('connected_economy advance', 'CONNECTED advance '))
            report['samples'].append(state)
            require(state['conserved'], f"Cargo mismatch at step {step}: {state['errors']}")
            require(not any(t['crashed'] for t in state['trains']), 'Train crashed')
            if step % 5 == 0:
                print(step, state['year'], state['month'], state['cities'], 'batches', [f['batches'] for f in state['facilities']], flush=True)
        engine.save(output / 'checkpoint.sav')
        report['last'] = state
    finally:
        engine.close()
        if report['passed'] and args.publish:
            publish_evidence(report, args.publish)
        (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/connected-economy')
    parser.add_argument('--steps', type=int, default=80)
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--publish', type=Path)
    args = parser.parse_args()
    if args.publish and not args.verify:
        parser.error('--publish requires --verify')
    run(args)
