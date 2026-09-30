#!/usr/bin/env python3
"""Exercise the integrated rules with the existing native connected-economy authoring harness."""
import argparse
import json
import time
import hashlib
import subprocess
from pathlib import Path
from test_wp11_slice import Engine, ROOT, require


FIRST_FREIGHT_ADVANCES = 240
PHASE_SECONDS = 1800
PACKS = ('integrated_v1', 'rail_v3', 'equipment_v1')


def phase_deadline(engine):
    """Each bounded acceptance phase gets its own 30-minute ceiling."""
    engine.deadline = time.monotonic() + PHASE_SECONDS


def freight_advance_limit(requested):
    require(requested > 0, 'Advance limit must be positive')
    return min(requested, FIRST_FREIGHT_ADVANCES)


def acceptance_inputs(binary):
    """Pin the actual binary, working source and published content used by a run."""
    diff = subprocess.check_output(['git', 'diff', 'HEAD', '--', 'src', 'scripts', 'demo'], cwd=ROOT)
    source_files = ('src/genworld.cpp', 'src/misc.cpp', 'src/town_cmd.cpp', 'src/town_cmd.h',
        'src/portal/world_gen.cpp', 'src/portal/world_gen.h', 'src/portal/portal_terminal.cpp',
        'src/portal/connected_economy.cpp', 'src/portal/connected_economy.h', 'src/portal/integrated_economy.cpp',
        'scripts/test_integrated_economy.py', 'scripts/test_wp11_slice.py', 'demo/integrated_economy.cfg')
    return {'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
        'source_diff_sha256': hashlib.sha256(diff).hexdigest(),
        'source_files_sha256': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in source_files},
        'pack_sha256': {name: hashlib.sha256((ROOT / 'bin/newgrf' / f'openspacettd_{name}.grf').read_bytes()).hexdigest()
                       for name in PACKS}}


def ordinary_config(seed):
    text = (ROOT / 'demo/integrated_economy.cfg').read_text().replace(
        'min_active_clients = 0', 'min_active_clients = 1')
    for name in PACKS:
        text = text.replace(f'openspacettd_{name}.grf =', f'{ROOT}/bin/newgrf/openspacettd_{name}.grf =')
    return text.replace('generation_seed = 11', f'generation_seed = {seed}')


def generation_projection(state):
    """Only native persisted semantics participate in cold equality; search stats are transient."""
    return {key: value for key, value in state.items() if key != 'town_generation'}


def console_ack(engine, command, marker):
    """Acknowledge a synchronous native console command which prints no success line."""
    engine.process.stdin.write(command + '\n' + f'echo {marker}\n')
    engine.process.stdin.flush()
    engine.wait(marker)


def same_process_load(engine, save):
    """Use native FIOS navigation/load and a real load-chunk marker, with no sleeps or load adapter."""
    save = save.resolve()
    current = Path('/' + engine.command('pwd', '/')).resolve()
    target = save.parent
    moves = 0
    while current != target and current not in target.parents:
        require(current.parent != current, 'Native save browser cannot reach proof directory')
        console_ack(engine, 'cd ".."', 'GENERATION_BROWSER_READY')
        current = current.parent
        moves += 1
        require(moves <= 64, 'Native save browser parent navigation exceeded bound')
    for part in target.relative_to(current).parts:
        require('"' not in part and '\n' not in part, 'Proof directory cannot be safely quoted for native console')
        console_ack(engine, f'cd "{part}"', 'GENERATION_BROWSER_READY')
        current /= part
        moves += 1
        require(moves <= 64, 'Native save browser navigation exceeded bound')
    require(Path('/' + engine.command('pwd', '/')).resolve() == target, 'Native save browser did not reach proof directory')
    console_ack(engine, 'debug_level "sl=2"', 'GENERATION_LOAD_DEBUG_READY')
    require('"' not in str(save) and '\n' not in str(save), 'Proof save cannot be safely quoted for native console')
    engine.command(f'load "{save}"', 'Loading chunk TRAD')


def rejected_generation_mutation(engine, command, expected_error, expected_state):
    """Require a native lifecycle rejection and exact untouched game/RNG projection."""
    try:
        engine.command('connected_economy ' + command, 'CONNECTED generation-')
    except RuntimeError as exc:
        require(expected_error in str(exc), f'Unexpected generation guard rejection: {exc}')
    else:
        raise RuntimeError(f'Loaded checkpoint authorized {command}')
    observed = json.loads(engine.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
    require(observed == expected_state, f'Rejected {command} changed native state/RNG')
    return observed


def validate_generated_state(state, seed):
    require(state['seed'] == seed, 'Resolved generator seed differs')
    require(state['money'] == state['loan'] == 100000 and state['max_loan'] == 300000, 'Nonordinary initial finances')
    require(len(state['worlds']) == 7 and state['economy']['version'] == 1, 'Integrated seven-world rules inactive')
    require(not state['trains'] and not state['stations'] and not state['research'] and not state['stocks'] and not state['hubs'],
            'Unexpected starting player assets or grants')
    require(all(tile[1] == 16 for tile in state['rail']), 'Player rail at start')
    stats = state['town_generation']
    require(stats['active'] and stats['failure'] == 0 and stats['target'] == 1 and len(state['towns']) == 1,
            'Configured single global town slot was not preserved')
    require(stats['probes'] <= 10000 and stats['creation_attempts'] <= 20, 'Core search budget exceeded')
    core_worlds = {world['id'] for world in state['worlds'] if world['role'] == 'Core'}
    require(state['core_town_valid'] and any(town['population'] > 0 and town['world'] in core_worlds and
        any(house['world'] == town['world'] for house in town['houses']) for town in state['towns']),
        'No living Core town with an actual own house in its Core world')
    require(not any(town['megacity'] for town in state['towns']), 'Generation automatically designated a town')
    require(len(state['terminals']) == 2 * len(state['gates']) == 6, 'Generated backbone endpoint count differs')
    require(all(end['public'] for gate in state['gates'] for end in gate['ends']), 'Nonpublic generated gate')
    claimed = set()
    for terminal in state['terminals']:
        require(terminal['planned'] and len(terminal['rails']) == 34 and len(terminal['signals']) == 2,
                'Generated terminal has incomplete geometry')
        head, join = terminal['head'], terminal['join']
        footprint = [head, *terminal['rails'], join]
        require(all(tile['valid'] and tile['world'] == terminal['world'] and tile['slope'] == 0 and
                    tile['height'] == head['height'] for tile in footprint), 'Generated terminal/join geometry invalid')
        require(head['owner'] == 16 and head['type'] == 9 and head['dir'] == terminal['dir'] and head['railtype'] == 0,
                'Generated gate head/railtype differs')
        require(join['type'] in (0, 4), 'Generated exterior join occupied')
        require(all(tile['owner'] == 16 and tile['type'] == 1 and tile['railtype'] == 0 and
                    tile['tracks'] == tile['expected_tracks'] for tile in terminal['rails']), 'Native neutral terminal rails differ')
        require(all(signal['present'] and signal['type'] == signal['expected_type'] and
                    signal['variant'] == signal['expected_variant'] and signal['present_bits'] == signal['expected_present']
                    for signal in terminal['signals']), 'Native terminal PBS differs')
        tiles = {tile['tile'] for tile in footprint}
        require(len(tiles) == 36 and not tiles & claimed, 'Generated terminal footprints interfere')
        claimed |= tiles
    for zone in state['zones']:
        require(zone['planned'], 'Advertised arrival zone has no valid geometry')
        footprint = [zone['head'], *zone['rails'], zone['join']]
        require(all(tile['valid'] and tile['world'] == zone['world'] and tile['slope'] == 0 and
                    tile['height'] == zone['head']['height'] and tile['type'] in (0, 4) for tile in footprint),
                'Advertised arrival zone has obstructed geometry')
        tiles = {tile['tile'] for tile in footprint}
        require(len(tiles) == 36 and not tiles & claimed, 'Arrival zone consumes another generated footprint/join')
        claimed |= tiles


def validate_generation_paid(initial, query, paid):
    state = paid['state']
    total = sum(join['paid'] for join in paid['joins']) + paid['station']['paid']
    require(total == query['total_quote'] > 0 and all(join['paid'] == join['quote'] > 0 for join in paid['joins']) and
            paid['station']['paid'] == paid['station']['quote'] > 0, 'Native query/execute cost differs or was free')
    require(state['money'] == initial['money'] - total > 0 and state['loan'] == initial['loan'], 'Paid construction cash/debt identity failed')
    require(len(paid['follow']) == len(initial['terminals']) and all(row['ready'] and row['enter'] and row['exit']
            for row in paid['follow']), 'Native public/player boundary is not traversable in both directions')
    require(len(state['stations']) == 1 and len(state['rail']) == len(initial['rail']) + len(paid['joins']), 'Unexpected paid infrastructure')
    for old, new in zip(initial['terminals'], state['terminals']):
        require({k: v for k, v in old.items() if k != 'join'} == {k: v for k, v in new.items() if k != 'join'},
                'Paid proof changed neutral head/34 rail bits/PBS')
        paid_join = next(join for join in paid['joins'] if join['head'] == new['head']['tile'])
        require(new['join']['owner'] == 0 and new['join']['type'] == 1 and new['join']['railtype'] == 0 and
                new['join']['tracks'] == 1 << paid_join['track'], 'Paid exterior join ownership/track differs')
    for field in ('seed', 'tick', 'date', 'worlds', 'zones', 'towns', 'gates', 'economy', 'stellar', 'industries',
                  'stocks', 'held', 'research', 'hubs', 'trains', 'structures'):
        require(state[field] == initial[field], f'Paid generation proof changed unrelated {field}')
    station = state['stations'][0]
    target = query['station']
    town = next(town for town in state['towns'] if town['id'] == target['town'])
    require(station['owner'] == 0 and station['town'] == town['id'] and not station['warehouse'] and not station['consumer'] and
        any(house['tile'] == target['house'] and house['town'] == town['id'] and house['world'] == town['world']
            for house in station['catchment_houses']), 'Paid station does not catch an actual own house of its assigned Core town')


def generation_contract(args):
    """Two fresh native generations per seed, with isolated paid copies and two cold checkpoints."""
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    binary = args.binary.resolve()
    report = {'passed': False, 'human_uat': 'Pending', 'mode': 'ordinary-generation-contract',
        'repetitions_per_seed': 2, 'simulation_advances': 0, 'seeds': [
            {'requested_seed': seed, 'passed': None, 'status': 'not_run', 'repetitions': [
                {'repetition': repetition, 'passed': None, 'status': 'not_run'} for repetition in (1, 2)]}
            for seed in args.seeds], **acceptance_inputs(binary)}
    for seed_row in report['seeds']:
        seed = seed_row['requested_seed']
        seed_row.update(passed=False, status='running')
        reference = None
        for row in seed_row['repetitions']:
            repetition = row['repetition']
            folder = output / str(seed) / str(repetition)
            folder.mkdir(parents=True)
            config = folder / 'newgame.cfg'
            config.write_text(ordinary_config(seed))
            row.update(passed=False, status='running', config_sha256=hashlib.sha256(config.read_bytes()).hexdigest())
            engine = loaded = None
            try:
                engine = Engine(binary, config, folder, 'generation', world_count=7, seed=seed)
                phase_deadline(engine)
                if repetition == 2:
                    unowned = json.loads(engine.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                    row['unowned_initial'] = unowned
                    unowned_save = folder / 'generation-unowned.sav'
                    engine.save(unowned_save)
                    row['unowned_sha256'] = hashlib.sha256(unowned_save.read_bytes()).hexdigest()
                initial = json.loads(engine.command('connected_economy generation-contract-start', 'CONNECTED generation-state '))
                row['initial'] = initial
                validate_generated_state(initial, seed)
                if reference is None:
                    reference = initial
                else:
                    require(initial == reference, 'Repeated actual-seed generation changed native semantics/search/RNG')
                    seed_row['fresh_repeated_equal'] = True
                query = json.loads(engine.command('connected_economy generation-contract-queries', 'CONNECTED generation-query '))
                row['query'] = query
                require(generation_projection(initial) == json.loads(engine.command(
                    'connected_economy generation-contract-status', 'CONNECTED generation-state ')), 'Native query changed generation state')
                pristine = folder / 'generation-pristine.sav'
                engine.save(pristine)
                row['pristine_sha256'] = hashlib.sha256(pristine.read_bytes()).hexdigest()
                loaded = Engine(binary, config, folder, 'pristine-reload', save=pristine, world_count=7, seed=seed)
                phase_deadline(loaded)
                restored = json.loads(loaded.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                row['pristine_restored'] = restored
                require(restored == generation_projection(initial), 'Pristine cold reload changed generation semantics/RNG')
                restored_query = json.loads(loaded.command('connected_economy generation-contract-queries', 'CONNECTED generation-query '))
                require(restored_query == query, 'Pristine cold reload changed native join/station quotes')
                try:
                    loaded.command('connected_economy generation-contract-paid', 'CONNECTED generation-paid ')
                except RuntimeError as exc:
                    require('spending requires fresh process' in str(exc), f'Unexpected paid guard rejection: {exc}')
                    row['loaded_mutation_rejected'] = True
                else:
                    raise RuntimeError('Cold-loaded pristine checkpoint authorized spending')
                require(json.loads(loaded.command('connected_economy generation-contract-status', 'CONNECTED generation-state ')) == restored,
                        'Rejected loaded mutation changed native state')
                loaded.close()
                loaded = None
                phase_deadline(engine)
                if repetition == 2:
                    # The flag is still true: no paid command ran in this process.
                    # Loading the marked pristine checkpoint must revoke it.
                    same_process_load(engine, pristine)
                    pristine_in_process = json.loads(engine.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                    require(pristine_in_process == generation_projection(initial), 'Same-process pristine load changed native semantics/RNG')
                    row['same_process_pristine'] = rejected_generation_mutation(engine, 'generation-contract-paid',
                        'spending requires fresh process', pristine_in_process)
                    row['same_process_paid_rejected'] = True
                    # An unowned checkpoint avoids the company-marker rejection and
                    # tests that old generation diagnostics cannot authorize start.
                    same_process_load(engine, unowned_save)
                    unowned_in_process = json.loads(engine.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                    require(unowned_in_process == unowned, 'Same-process unowned load changed native semantics/RNG')
                    row['same_process_unowned'] = rejected_generation_mutation(engine, 'generation-contract-start',
                        'requires pristine fresh generation', unowned_in_process)
                    row['same_process_start_rejected'] = True
                    row['paid_proof'] = {'status': 'not_run', 'reason':
                        'Second fresh repetition tests same-process load revocation before spending; paid joins/station/cold follow proved in repetition 1.'}
                    row['passed'] = True
                    row['status'] = 'passed'
                    print(f'Seed {seed} repetition 2: deterministic fresh generation and both same-process lifecycle rejections passed', flush=True)
                    continue
                paid = json.loads(engine.command('connected_economy generation-contract-paid', 'CONNECTED generation-paid '))
                row['paid'] = paid
                validate_generation_paid(initial, query, paid)
                save = folder / 'generation-paid.sav'
                engine.save(save)
                row['paid_sha256'] = hashlib.sha256(save.read_bytes()).hexdigest()
                engine.close()
                engine = None
                loaded = Engine(binary, config, folder, 'paid-reload', save=save, world_count=7, seed=seed)
                phase_deadline(loaded)
                restored_paid = json.loads(loaded.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                row['paid_restored'] = restored_paid
                require(restored_paid == paid['state'], 'Paid cold reload changed native ownership/rails/PBS/town/catchment/cash/RNG')
                follow = json.loads(loaded.command('connected_economy generation-contract-follow', 'CONNECTED generation-follow '))
                require(follow == paid['follow'], 'Paid cold reload changed native boundary traversal')
                row['paid_cold_follow'] = follow
                row['passed'] = True
                row['status'] = 'passed'
                print(f'Seed {seed} repetition {repetition}: generation, native paid joins/Core station and both cold reloads passed', flush=True)
            except Exception as exc:
                row['failure'] = str(exc)
                row['status'] = seed_row['status'] = 'failed'
                if engine is not None and engine.process.poll() is None:
                    try:
                        row['failure_state'] = json.loads(engine.command('connected_economy generation-contract-status', 'CONNECTED generation-state '))
                        engine.save(folder / 'failure.sav')
                    except Exception as snapshot_error:
                        row['failure_snapshot_error'] = str(snapshot_error)
                print(f'Seed {seed} repetition {repetition}: FAIL: {exc}', flush=True)
            finally:
                for instance in (engine, loaded):
                    if instance is not None:
                        instance.close()
                (folder / 'evidence.json').write_text(json.dumps(row, indent=2) + '\n')
                (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
            require(row['passed'], f'Generation contract stopped at seed {seed} repetition {repetition}; see retained failure evidence; remaining cases NOT RUN')
        seed_row['passed'] = all(row['passed'] for row in seed_row['repetitions']) and seed_row.get('fresh_repeated_equal', False)
        seed_row['status'] = 'passed' if seed_row['passed'] else 'failed'
    report['passed'] = all(row['passed'] for row in report['seeds'])
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    require(report['passed'], 'Generation contract remains unproven; see retained per-seed failures')


def audit(engine):
    result=json.loads(engine.command('connected_economy audit','CONNECTED audit '))
    require(not result['invalid_slopes'],'Invalid terrain')
    require(all(row['engines'] for row in result['freight_refits']),'Unsupported freight refit')
    for cargo in result['cargo']:
        for field in ('name','single','units','quantity','zero_quantity','large_quantity','abbreviation'):
            require(cargo[field] and not any(word in cargo[field].lower() for word in ('undefined','invalid string','(null)')),f'Broken cargo text: {cargo}')
    return result


def first_freight(args):
    """Prove the ordinary generator/command path, retaining failures for every seed."""
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    binary = args.binary.resolve()
    advances = freight_advance_limit(args.steps)
    report = {'passed': False, 'human_uat': 'Pending', 'mode': 'ordinary-first-freight',
              'cargo_proof': 'IRON', 'steps_per_phase_limit': advances, 'ticks_per_advance': 2048,
              'seconds_per_phase_limit': PHASE_SECONDS, 'seeds': [
                  {'requested_seed': seed, 'passed': None, 'status': 'not_run'} for seed in args.seeds], **acceptance_inputs(binary)}
    for row in report['seeds']:
        seed = row['requested_seed']
        folder = output / str(seed)
        folder.mkdir()
        config = folder / 'newgame.cfg'
        config.write_text(ordinary_config(seed))
        row.update(passed=False, status='running', plans=[], samples=[], config_sha256=hashlib.sha256(config.read_bytes()).hexdigest())
        engine = None
        try:
            engine = Engine(binary, config, folder, 'generation', world_count=7, seed=seed)
            phase_deadline(engine)
            initial = json.loads(engine.command('connected_economy first-freight-start', 'FREIGHT state '))
            row['initial'] = initial
            require(initial['seed'] == seed, 'Resolved generator seed differs')
            require(initial['economy']['version'] == 1, 'Integrated rules inactive')
            require(initial['money'] == initial['loan'] == 100000, 'Nonordinary initial finances')
            require(not initial['trains'] and not initial['stations'] and not initial['research'] and not initial['stocks'], 'Unexpected starting player assets or grants')
            require(all(t[1] == 16 for t in initial['rail']), 'Player rail at start')
            engine.save(folder / 'ordinary-start.sav')
            candidates = []
            for label in ('IRON',):
                cargo = initial['cargo_labels'][label]
                for source in initial['industries']:
                    if not any(c[0] == cargo for c in source['outputs']):
                        continue
                    for dest in initial['industries']:
                        if not any(c[0] == cargo for c in dest['inputs']):
                            continue
                        for gate in initial['gates']:
                            ends = sorted(gate['ends'], key=lambda p: p['world'] != source['world'])
                            if [p['world'] for p in ends] != [source['world'], dest['world']]:
                                continue
                            require(all(p['public'] for p in ends), 'Nonpublic starter gate')
                            distance = sum(abs(i['tile'] % 1024 - p['tile'] % 1024) +
                                abs(i['tile'] // 1024 - p['tile'] // 1024)
                                for i, p in zip((source, dest), ends))
                            candidates.append((distance, label, source['id'], dest['id'], gate['id']))
            for choice in sorted(candidates):
                _, label, source, dest, gate = choice
                suffix = f'{source} {dest} {gate}'
                plan = json.loads(engine.command('connected_economy first-freight-plan ' + suffix, 'FREIGHT plan '))
                row['plans'].append({'choice': choice, 'plan': plan})
            legal = [p for p in row['plans'] if p['plan']['legal']]
            require(legal, 'Bounded planner found no legal IRON starter route; not proof that every route is impossible')
            selected = min(legal, key=lambda p: p['plan']['construction_quote'])
            row['selected'] = selected
            _, label, source, dest, gate = selected['choice']
            cargo = initial['cargo_labels'][label]
            built = json.loads(engine.command(f'connected_economy first-freight-build {source} {dest} {gate}', 'FREIGHT state '))
            row['built'] = built
            row['build_cost'] = initial['money'] + built['loan'] - initial['loan'] - built['money']
            row['borrowing_headroom'] = built['max_loan'] - built['loan']
            engine.save(folder / 'construction.sav')
            capacity = sum(t['capacity'] for t in built['trains'] if t['cargo_type'] == cargo)
            require(capacity > 0, 'Empty or wrong-cargo consist')
            total_delivered = 0
            consumed = 0
            cycles = 0
            phase_deadline(engine)
            for step in range(advances):
                state = json.loads(engine.command('connected_economy first-freight-advance', 'FREIGHT state '))
                row['samples'].append(state)
                require(state['cash_conserved'] and not state['cargo_errors'], 'Native cash or physical cargo conservation failed')
                require(state['money'] > 0 and state['loan'] == built['loan'], 'Starter operating cash exhausted or debt changed')
                require(not any(t['crashed'] or t['lost'] for t in state['trains']), 'Train crashed or lost')
                delivered = sum(units[cargo] for _, units in state['audit']['vehicle_deliveries'])
                total_delivered += delivered
                consumed += state['audit']['consumed'][cargo]
                completed = total_delivered // capacity
                if completed > cycles:
                    cycles = completed
                    print(f'Seed {seed}: paid load {cycles}, cargo={total_delivered}, cash={state["money"]}', flush=True)
                if cycles >= 3 and consumed > 0:
                    break
            row['complete_loads'] = cycles
            row['delivered'] = total_delivered
            row['consumed'] = consumed
            require(cycles >= 3 and consumed > 0, 'Three complete loads and real processor consumption did not complete')
            row['operating_cash_delta'] = state['money'] - built['money']
            require(row['operating_cash_delta'] > 0, 'Repeated starter service has negative net cash after costs/interest/tolls')
            row['before_onward'] = state
            phase_deadline(engine)
            row['onward'] = json.loads(engine.command(f'connected_economy first-freight-onward {dest}', 'FREIGHT onward '))
            row['onward_built'] = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
            row['onward_borrowing_headroom'] = row['onward_built']['max_loan'] - row['onward_built']['loan']
            row['onward_cost'] = (state['money'] + row['onward_built']['loan'] - state['loan'] - row['onward_built']['money'])
            row['onward_samples'] = []
            for _ in range(16):
                state = json.loads(engine.command('connected_economy first-freight-advance', 'FREIGHT state '))
                row['onward_samples'].append(state)
                require(state['cash_conserved'] and not state['cargo_errors'], 'Output-service conservation failed')
                require(state['money'] > 0 and state['loan'] == row['onward_built']['loan'], 'Output operating cash exhausted or debt changed')
                require(not any(t['crashed'] or t['lost'] for t in state['trains']), 'Output service crashed or lost')
                if any(hub[3] > 0 for hub in state['hubs']):
                    break
            require(any(hub[3] > 0 for hub in state['hubs']), 'Affordable output step did not remove real processor output')
            row['continuation_proven'] = True
            save = folder / 'operation.sav'
            engine.save(save)
            final = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
            row['before_reload'] = final
            engine.close()
            engine = Engine(binary, config, folder, 'reload', save=save, world_count=7, seed=seed)
            phase_deadline(engine)
            restored = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
            row['restored'] = restored
            require(restored == final, 'Cold reload changed native route/orders/cargo/finances/economy state')
            row['cold_reload_equal'] = True
            further = 0
            further_delivered = 0
            row['reload_samples'] = []
            for _ in range(advances):
                state = json.loads(engine.command('connected_economy first-freight-advance', 'FREIGHT state '))
                row['reload_samples'].append(state)
                require(state['cash_conserved'] and not state['cargo_errors'], 'Reload conservation failed')
                require(state['money'] > 0 and state['loan'] == restored['loan'], 'Reload operating cash exhausted or debt changed')
                require(not any(t['crashed'] or t['lost'] for t in state['trains']), 'Reload train crashed or lost')
                further_delivered += sum(units[cargo] for _, units in state['audit']['vehicle_deliveries'])
                further = further_delivered // capacity
                if further >= 3:
                    break
            row['reload_complete_loads'] = further
            require(further >= 3 and state['money'] > restored['money'], 'Profitable reload continuation failed')
            require(sum(s['audit']['consumed'][cargo] for s in row['reload_samples']) > 0, 'No processor consumption after reload')
            gate_totals = {}
            for sample in row['samples'] + row['onward_samples'] + row['reload_samples']:
                for tile, (count, toll) in sample['audit']['gate_tolls']:
                    admissions, money = gate_totals.get(tile, (0, 0))
                    gate_totals[tile] = (admissions + count, money + toll)
            row['gate_totals'] = gate_totals
            used = next(g for g in initial['gates'] if g['id'] == gate)
            require(all(gate_totals.get(end['tile'], (0, 0))[0] > 0 for end in used['ends']), 'Both-direction public tolls not observed')
            row['save_sha256'] = hashlib.sha256(save.read_bytes()).hexdigest()
            require(not any(t[3] for t in state['research']), 'Starter service acquired unexplained research unlocks')
            row['passed'] = True
            row['status'] = 'passed'
        except Exception as exc:
            if engine is not None and engine.process.poll() is None:
                try:
                    row['failure_state'] = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
                    engine.save(folder / 'failure.sav')
                except Exception as snapshot_error:
                    row['failure_snapshot_error'] = str(snapshot_error)
            row['failure'] = str(exc)
            row['status'] = 'failed'
            print(f'Seed {seed}: FAIL: {exc}', flush=True)
        finally:
            if engine is not None:
                engine.close()
            (folder / 'evidence.json').write_text(json.dumps(row, indent=2) + '\n')
            (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
        require(row['passed'], f'First Sustainable IRON Freight stopped at seed {seed}; see retained failure evidence; remaining seeds NOT RUN')
    report['passed'] = all(row['passed'] for row in report['seeds'])
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    require(report['passed'], 'First Sustainable Freight remains unproven; see retained per-seed failures')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'build/openttd')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=500)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--first-freight', action='store_true', help='Ordinary generated IRON start; at most 240 advances/30 minutes per phase')
    modes.add_argument('--generation-contract', action='store_true', help='Twice-fresh generation, ordinary paid joins/Core station and cold reloads; no progression')
    parser.add_argument('--seeds', type=int, nargs='+', default=[11, 101, 2026])
    args = parser.parse_args()
    if args.generation_contract:
        return generation_contract(args)
    if args.first_freight:
        return first_freight(args)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    text = (ROOT/'demo/wp11_slice.cfg').read_text().split('[newgrf]')[0]
    for old, new in [('map_x = 9','map_x = 10'),('map_y = 7','map_y = 10'),('threaded_saves = true','threaded_saves = false'),('autosave_on_exit = true','autosave_on_exit = false')]:
        text = text.replace(old,new)
    text += '\n[newgrf]\n' + ''.join(f'{ROOT}/bin/newgrf/{name} =\n' for name in ('openspacettd_integrated_v1.grf','openspacettd_rail_v3.grf','openspacettd_equipment_v1.grf'))
    config=output/'integrated.cfg';config.write_text(text)
    engine=Engine(args.binary.resolve(),config,output,'integrated')
    engine.deadline=time.monotonic()+3600
    report={'passed':False,'samples':[], 'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest()}
    save=output/'integrated.sav'
    complete_month=None
    try:
        engine.command('connected_economy prepare-integrated','CONNECTED prepared')
        report['initial']=json.loads(engine.command('connected_economy status','CONNECTED state '))
        report['initial_audit']=audit(engine)
        initial_save=output/'initial.sav';engine.save(initial_save)
        english_config=output/'english.cfg';english_config.write_text(text+'\n[misc]\nlanguage = english.lng\n')
        english=Engine(args.binary.resolve(),english_config,output,'english-audit',save=initial_save)
        try:report['english_audit']=audit(english)
        finally:english.close()
        for step in range(args.steps):
            state=json.loads(engine.command('connected_economy advance','CONNECTED advance '))
            report['samples'].append(state)
            require(state['conserved'],f"Cargo mismatch: {state['errors']}")
            require(not any(t['crashed'] for t in state['trains']),'Train crashed')
            require(state['money']>0,'Company exhausted its funds')
            engine.command('connected_economy progress','CONNECTED state ')
            month=state['year']*12+state['month']
            complete=len(state['facilities'])==13 and all(f['batches']>0 for f in state['facilities']) and len(state['research'][0]['unlocked'])==12 and any(p[8]==2 for p in state['stellar']['projects'])
            if complete and complete_month is None:
                complete_month=month
                report['progression_complete']=state
            if complete_month is not None and month-complete_month>=24:
                report['soak_months']=month-complete_month
                break
            if step % 10 == 0:
                print(f"Step {step}: {state['year']}-{state['month']+1}; factories={len(state['facilities'])}; research={state['research']}",flush=True)
        require(complete_month is not None and report.get('soak_months',0)>=24,'Progression and 24-month soak did not complete')
        report['electric']=json.loads(engine.command('connected_economy electric','CONNECTED electric '))
        require(all(report['electric'].values()),'Electric construction bypassed or mutated its preview')
        report['soak_cash_delta']=state['money']-report['progression_complete']['money']
        require(report['soak_cash_delta']>0,'Operating soak was not profitable')
        engine.command('connected_economy stop-food','CONNECTED food stopped')
        for _ in range(8):
            state=json.loads(engine.command('connected_economy advance','CONNECTED advance '))
            require(state['conserved'],'Interruption cargo mismatch')
        report['food_interrupted']=state
        require(all(city['state']==0 for city in state['cities']),'Food interruption did not halt growth')
        engine.command('connected_economy start-food','CONNECTED food started')
        recovered=False
        for _ in range(24):
            state=json.loads(engine.command('connected_economy advance','CONNECTED advance '))
            require(state['conserved'],'Recovery cargo mismatch')
            recovered |= any(city['state']!=0 for city in state['cities'])
        require(recovered,'Food service failed to recover')
        report['food_recovered']=True
        report['audit']=audit(engine)
        engine.save(save)
        report['final']=json.loads(engine.command('connected_economy status','CONNECTED state '))
    finally:
        report['exit_before_close']=engine.process.poll()
        engine.close()
        (output/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')

    loaded=Engine(args.binary.resolve(),config,output,'reload',save=save)
    try:
        restored=json.loads(loaded.command('connected_economy status','CONNECTED state '))
        for key in ('economy','stellar','stocks','held','research','money'):
            require(restored[key]==report['final'][key],f'Cold reload changed {key}')
        state=json.loads(loaded.command('connected_economy advance','CONNECTED advance '))
        require(state['conserved'],'Cargo mismatch after reload')
        report['cold_reload']=True
        report['passed']=True
        report['save_sha256']=hashlib.sha256(save.read_bytes()).hexdigest()
    finally:
        loaded.close()
        (output/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Integrated progression, 24-month soak, electric construction and cold reload passed',flush=True)

if __name__=='__main__':main()
