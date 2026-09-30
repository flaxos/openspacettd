#!/usr/bin/env python3
"""Exercise the integrated rules with the existing native connected-economy authoring harness."""
import argparse
import json
import time
import hashlib
import subprocess
from pathlib import Path
from test_wp11_slice import Engine, OfflineEngine, ROOT, require
from test_commonwealth_platform_art import environment, profile_environment


FIRST_FREIGHT_ADVANCES = 240
PHASE_SECONDS = 1800
PACKS = ('integrated_v1', 'rail_v3', 'equipment_v1')


def phase_deadline(engine):
    """Each bounded acceptance phase gets its own 30-minute ceiling."""
    engine.deadline = time.monotonic() + PHASE_SECONDS


def freight_advance_limit(requested):
    require(requested > 0, 'Advance limit must be positive')
    return min(requested, FIRST_FREIGHT_ADVANCES)


def functional_campaign(args):
    """One bounded ASSISTED FUNCTIONAL campaign; one replay only after success."""
    require(args.seeds == [11], 'ASSISTED FUNCTIONAL is authorized for seed11 only')
    output = args.output.resolve()
    require(not output.exists(), 'Preserve earlier evidence; choose a new output directory')
    output.mkdir(parents=True)
    whole_deadline = time.monotonic() + 4 * 3600
    report = {'label': 'ASSISTED FUNCTIONAL', 'status': 'NOT RUN', 'human_uat': 'Pending',
              'ordinary_start_economic_proof': 'NOT RUN', 'primary': {}, 'replay': {'status': 'NOT RUN'},
              'inputs': acceptance_inputs(args.binary.resolve())}
    changed = subprocess.check_output(['git', 'diff', '--name-only', 'HEAD', '--', 'src', 'scripts'], cwd=ROOT, text=True)
    report['inputs']['changed_source_sha256'] = {
        p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in changed.splitlines()}

    def persist():
        report['artifacts'] = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in output.rglob('*.sav')}
        (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')

    def attempt(folder, result):
        folder.mkdir()
        config = folder / 'functional.cfg'
        config.write_text(ordinary_config(11) + '\n[misc]\nlanguage = english.lng\n')
        result.update(status='PARTIAL', assistance=[], native_transactions=[], samples=[], trace=[],
                      phases={name: 'NOT RUN' for name in ('preflight', 'setup', 'initial', 'cold')}, negative_control={'status': 'NOT RUN'}, cold_before={'status': 'NOT RUN'},
                      cold_after={'status': 'NOT RUN'})
        result['config_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
        engine = None
        primary_debits = 0
        control_debits = 0
        receipts = 0
        phase_used = {'initial': 0, 'cold': 0}
        preflight_deadline = min(whole_deadline, time.monotonic() + PHASE_SECONDS)

        def start(name, save=None, target=folder):
            target.mkdir(exist_ok=True)
            target_config = target / 'functional.cfg'
            if target != folder:
                target_config.write_text(config.read_text())
            with environment(profile_environment(target)):
                native = OfflineEngine(args.binary.resolve(), target_config, target, name, save=save)
            return native

        def state(native):
            return json.loads(native.command('connected_economy functional-status', 'CONNECTED functional-state '))

        def mutate(native, operation, ledger=result, pre_hq=False):
            nonlocal primary_debits, control_debits, receipts
            require(time.monotonic() < whole_deadline, 'Whole native campaign exhausted four-hour wall limit')
            before = state(native)
            value = json.loads(native.command('connected_economy ' + operation, 'CONNECTED functional-result '))
            after = value['state']
            entries = value['audit']['cash_transactions']
            # Retain the native result and its ledger even when an invariant stops
            # this campaign; a failed observation must not erase paid transactions.
            ledger.setdefault('native_transactions', []).extend(entries)
            ledger.setdefault('trace', []).append({'operation': operation, 'result': value})
            if ledger is result:
                primary_debits += sum(max(row[2], 0) for row in entries)
                receipts += sum(max(-row[2], 0) for row in entries if operation != 'functional-grant')
            else:
                control_debits += sum(max(row[2], 0) for row in entries)
            require(value['cash_conserved'] and not value['cargo_errors'], f'{operation}: native cash/cargo mismatch')
            require(after['loan'] == 100000 and after['money'] > 0, f'{operation}: fixed debt/cash contract failed')
            require(not any(t['lost'] or t['crashed'] for t in after['trains']), f'{operation}: lost or crashed train')
            require(sum(row[2] for row in entries) == value['audit']['cash_debits'], 'Incomplete native transaction history')
            require(after['money'] == before['money'] - sum(row[2] for row in entries), 'Financial transaction reconciliation failed')
            if ledger is result:
                require(primary_debits + control_debits <= 4000000, 'All-phase GBP4m gross-debit cap exhausted')
                if pre_hq:
                    require(primary_debits + control_debits <= 1000000, 'Pre-HQ GBP1m gross-debit cap exhausted')
            else:
                require(sum(max(row[2], 0) for row in ledger['native_transactions']) + ledger['starting_debits'] <= 1000000,
                        'Negative-control pre-HQ gross-debit cap exhausted')
            return value

        def advance(native, phase, deadline, ledger=result):
            require(phase_used[phase] < 240 and time.monotonic() < deadline,
                    f'{phase} shared 240 advances/30-minute bound exhausted')
            native.deadline = deadline
            phase_used[phase] += 1
            value = mutate(native, 'functional-advance', ledger, pre_hq=(phase == 'initial'))
            require(value['state']['tick'] - ledger['previous_tick'] == 2048, 'Advance differs from 2048 native ticks')
            ledger['previous_tick'] = value['state']['tick']
            ledger.setdefault('samples', []).append(value)
            return value

        def paid_visits(samples, service):
            arrivals = sorted(row for sample in samples for row in sample['audit']['arrivals']
                              if row[1] == service['train'] and row[2] == service['drop'])
            delivered, booked = set(), set()
            for sample in samples:
                for row in sample['audit']['payments']:
                    prior = [a[0] for a in arrivals if a[0] <= row[0]]
                    if row[1] == service['train'] and row[2] == service['drop'] and row[3] == service['cargo'] and row[4] > 0 and row[5] > 0 and prior:
                        delivered.add(max(prior))
                for row in sample['audit']['cash_payments']:
                    prior = [a[0] for a in arrivals if a[0] <= row[0]]
                    if row[1] == service['train'] and row[2] == service['drop'] and row[3] > 0 and prior:
                        booked.add(max(prior))
            return sorted(delivered & booked)

        def consumed_months(samples, town):
            return sorted({row[0] for sample in samples for row in sample['audit']['city_months'] if row[1] == town and row[2] > 0})

        def town_state(snapshot, town):
            return next(t for t in snapshot['towns'] if t['id'] == town)

        def reject_grant(native, expected):
            try:
                native.command('connected_economy functional-grant', 'CONNECTED functional-result ')
            except RuntimeError as error:
                require('one grant requires fresh pristine offline setup, never reload' in str(error), 'Unexpected grant rejection')
            else:
                raise RuntimeError('Repeat or reload grant was accepted')
            require(state(native) == expected, 'Rejected grant changed saved semantics')

        try:
            engine = start('fresh')
            engine.deadline = preflight_deadline
            pristine = json.loads(engine.command('connected_economy functional-start', 'CONNECTED functional-state '))
            result['pristine'] = pristine
            result['pristine_audit'] = audit(engine)
            require(state(engine) == pristine, 'Pristine terrain/text audit changed state')
            require(pristine['money'] == pristine['loan'] == 100000 and pristine['max_loan'] == 300000 and
                    pristine['seed'] == 11 and pristine['core_town_valid'] and not pristine['stations'] and
                    not pristine['trains'] and not pristine['research'] and not pristine['hqs'], 'Noncanonical generated pristine state')
            engine.save(folder / 'pristine.sav')
            grain, food = pristine['cargo_labels']['GRAI'], pristine['cargo_labels']['FOOD']
            roles = {w['id']: w['role'] for w in pristine['worlds']}
            candidates = []
            for producer in pristine['industries']:
                if roles[producer['world']] != 'Frontier' or not any(c[0] == grain for c in producer['outputs']):
                    continue
                for processor in pristine['industries']:
                    if roles[processor['world']] != 'Industrial' or not any(c[0] == grain for c in processor['inputs']) or not any(c[0] == food for c in processor['outputs']):
                        continue
                    for town in pristine['towns']:
                        if roles[town['world']] != 'Core' or town['population'] == 0:
                            continue
                        for a in pristine['gates']:
                            if {e['world'] for e in a['ends']} != {producer['world'], processor['world']}:
                                continue
                            for b in pristine['gates']:
                                if {e['world'] for e in b['ends']} == {processor['world'], town['world']}:
                                    candidates.append((producer['id'], processor['id'], town['id'], a['id'], b['id']))
            result['preflight'] = []
            selected = None
            for ids in sorted(candidates)[:2]:
                require(time.monotonic() < preflight_deadline, 'Generation/preflight 30-minute wall bound exhausted')
                command = 'functional-plan ' + ' '.join(map(str, ids))
                plan = json.loads(engine.command('connected_economy ' + command, 'CONNECTED functional-plan '))
                result['preflight'].append(plan)
                require(plan['preview_unchanged'] and state(engine) == pristine, 'Preflight mutated native state')
                if plan['legal'] and plan['total_quote'] <= 1000000:
                    selected = ids
                    result['selected_plan'] = plan
                    result['selection_reason'] = 'First legal whole chain in stable producer/processor/town/gate ID order within two candidates'
                    break
            require(selected is not None, 'No legal complete chain within two candidates and GBP1m pre-HQ quote bound')
            result['ordinary_start'] = {'label': 'ordinary-start', 'status': 'PASS', 'scope': 'read-only feasibility quote only',
                'cash': 100000, 'loan': 100000, 'max_loan': 300000, 'total_quote': plan['total_quote'],
                'construction_with_max_native_loan_quote_feasible': plan['total_quote'] <= 300000,
                'hq_cash_eligibility': 5000000, 'hq_ordinary_feasible': False,
                'economics': 'Unproven; retained revised A1 financial failure remains the operating comparison'}
            result['phases']['preflight'] = 'PASS'
            setup_deadline = min(whole_deadline, time.monotonic() + PHASE_SECONDS)
            engine.deadline = setup_deadline
            grant = mutate(engine, 'functional-grant', pre_hq=True)
            require(grant['amount'] == 6000000 and grant['state']['money'] == 6100000 and
                    grant['audit']['cash_transactions'] == [[pristine['tick'], 12, -6000000]], 'Cash allowance/native Other booking differs')
            result['assistance'].append({'amount': 6000000, 'currency': 'GBP virtual in-game', 'source': grant['source'],
                'native_expense_type': 'Other', 'signed_native_debit': -6000000, 'tick': pristine['tick'],
                'before_cash': 100000, 'after_cash': 6100000, 'before_debt': 100000, 'after_debt': 100000})
            reject_grant(engine, grant['state'])
            engine.save(folder / 'cash-assisted.sav')
            built = mutate(engine, 'functional-build ' + ' '.join(map(str, selected)), pre_hq=True)
            require(time.monotonic() < setup_deadline, 'Setup/build 30-minute bound exhausted')
            require(built['state']['money'] >= 5100000, 'Construction failed to retain GBP5.1m before receipts')
            result['construction'] = built
            result['construction_audit'] = audit(engine)
            require(state(engine) == built['state'], 'Paid terrain/text audit changed state')
            result['phases']['setup'] = 'PASS'
            food_service = built['services'][1]
            town = selected[2]
            receiver = next(s for s in built['state']['stations'] if s['id'] == food_service['drop'])
            require(receiver['consumer'] and not receiver['warehouse'] and receiver['town'] == town and
                    any(h['town'] == town and h['tile'] == plan['receiver_house'] for h in receiver['catchment_houses']),
                    'Native own-house FOOD receiver differs')
            initial_deadline = min(whole_deadline, time.monotonic() + PHASE_SECONDS)
            result['previous_tick'] = built['state']['tick']
            while len(paid_visits(result['samples'], food_service)) < 3 or len(consumed_months(result['samples'], town)) < 3:
                advance(engine, 'initial', initial_deadline)
            result['paid_food_visits'] = paid_visits(result['samples'], food_service)
            result['consuming_months'] = consumed_months(result['samples'], town)
            result['paid_grain_visits'] = paid_visits(result['samples'], built['services'][0])
            conversion = [row for sample in result['samples'] for row in sample['audit']['processor_cargo'] if row[1] == selected[1]]
            used_grain = -sum(row[3] for row in conversion if row[2] == grain)
            made_food = sum(row[3] for row in conversion if row[2] == food)
            batches = next(row[5] for row in state(engine)['economy']['factories'] if row[0] == selected[1]) - next(row[5] for row in pristine['economy']['factories'] if row[0] == selected[1])
            require(result['paid_grain_visits'] and used_grain == made_food == 2 * batches and batches > 0,
                    'Selected processor did not convert paid grain at actual 2:2 batch ratio')
            result['conversion'] = {'grain_consumed': used_grain, 'food_produced': made_food, 'batches': batches}
            print(f'ASSISTED FUNCTIONAL checkpoint: three paid FOOD visits and consuming months; {folder}', flush=True)
            before_control = state(engine)
            engine.save(folder / 'food-control-source.sav')
            control = result['negative_control']
            control.update(status='PARTIAL', starting_debits=primary_debits, starting_state=before_control, previous_tick=before_control['tick'], samples=[])
            copy = folder / 'negative-control'
            controlled = start('control', folder / 'food-control-source.sav', copy)
            try:
                controlled.deadline = initial_deadline
                require(state(controlled) == before_control, 'Disposable control cold load changed semantics')
                mutate(controlled, 'functional-stop ' + str(food_service['train']), control)
                zero_month = None
                while True:
                    observation = advance(controlled, 'initial', initial_deadline, control)
                    now = observation['state']
                    train = next(t for t in now['trains'] if t['id'] == food_service['train'])
                    city = town_state(now, town)
                    reserve, demand = dict(city['reserves']).get(food, 0), dict(city['demand'])[food]
                    month = now['calendar_clock'][:2]
                    if zero_month is not None:
                        require(train['stopped'] and train['speed'] == 0 and reserve == zero_month[1] and
                                not observation['audit']['city_months'] and not dict(city['consumed']).get(food, 0),
                                'Stopped supply changed reserves or consumed FOOD between observations')
                    if train['stopped'] and train['speed'] == 0 and reserve < demand and not dict(city['consumed']).get(food, 0):
                        if zero_month is None:
                            zero_month = (month, reserve)
                        elif month != zero_month[0]:
                            require(reserve == zero_month[1] and not observation['audit']['city_months'], 'Stopped supply changed reserves or consumed FOOD')
                            control['below_basket_remainder'] = reserve
                            control['stopped_no_use'] = now
                            break
                mutate(controlled, 'functional-restart ' + str(food_service['train']), control)
                recovery_samples = []
                while not paid_visits(recovery_samples, food_service) or not consumed_months(recovery_samples, town):
                    recovery_samples.append(advance(controlled, 'initial', initial_deadline, control))
                control['status'] = 'PASS'
                controlled.save(copy / 'recovered.sav')
            finally:
                controlled.close()
            require(state(engine) == before_control, 'Disposable control changed the paused primary')
            result['phases']['initial'] = 'PASS'
            engine.deadline = initial_deadline
            unlock = mutate(engine, 'functional-unlock', pre_hq=False)
            require(unlock['hq_quote'] == 2500000 and unlock['no_hq_rejected'] and unlock['materials_ii_rejected'], 'HQ/prerequisite proof differs')
            require(unlock['state']['research'] == [[301, 0, 100000, []]], 'Materials I first checkpoint must be selected at 0 RP')
            before_cold = state(engine)
            engine.save(folder / 'materials-i-before.sav')
            engine.close()
            engine = None
            cold_deadline = min(whole_deadline, time.monotonic() + PHASE_SECONDS)
            engine = start('cold-before', folder / 'materials-i-before.sav')
            engine.deadline = cold_deadline
            require(state(engine) == before_cold, 'First cold load lost project/budget/RP or native semantics')
            reject_grant(engine, before_cold)
            result['cold_before'].update(status='PASS', equal=True, state=before_cold)
            while 301 not in state(engine)['research'][0][3]:
                advance(engine, 'cold', cold_deadline)
            eligible = mutate(engine, 'functional-eligibility')
            require(eligible['materials_i_unlocked'] and eligible['materials_ii_selectable'], 'Materials II not selectable after paid I')
            research_entries = [row for row in result['native_transactions'] if row[1] == 12 and row[2] == 100000]
            require(len(research_entries) == 1, 'Research did not book exactly GBP100k once through native Other')
            mutate(engine, 'functional-budget-off')
            completed = state(engine)
            engine.save(folder / 'materials-i-completed.sav')
            engine.close()
            engine = None
            engine = start('cold-after', folder / 'materials-i-completed.sav')
            engine.deadline = cold_deadline
            require(state(engine) == completed, 'Second cold load changed retained unlock/custody/cash/orders')
            reject_grant(engine, completed)
            result['cold_after'].update(status='PASS', equal=True, state=completed)
            continued = []
            while not paid_visits(continued, food_service) or not consumed_months(continued, town):
                continued.append(advance(engine, 'cold', cold_deadline))
            result['cold_continuation'] = continued
            result['phases']['cold'] = 'PASS'
            result['final'] = state(engine)
            result['final_audit'] = audit(engine)
            require(state(engine) == result['final'], 'Final terrain/text audit changed state')
            engine.save(folder / 'owner-observation.sav')
            result['advance_counts'] = phase_used
            result['financial_reconciliation'] = {'starting_cash': 100000, 'assistance': 6000000,
                'native_receipts_excluding_assistance': receipts, 'native_gross_debits': primary_debits,
                'disposable_control_gross_debits': control_debits, 'campaign_gross_debit_cap_usage': primary_debits + control_debits,
                'ending_cash': result['final']['money'], 'debt': 100000,
                'native_net_excluding_assistance': receipts - primary_debits}
            require(result['final']['money'] == 100000 + 6000000 + receipts - primary_debits, 'Complete campaign financial reconciliation failed')
            result['status'] = 'PASS'
        except BaseException as error:
            result['error'] = str(error)
            result['status'] = 'PARTIAL' if any(term in str(error) for term in ('bound exhausted', 'cap exhausted', 'wall limit', 'within two candidates')) else 'FAIL'
            result['advance_counts'] = phase_used
            if engine is not None and engine.process.poll() is None:
                try:
                    result['partial_state'] = state(engine)
                    engine.save(folder / 'stopped-partial.sav')
                except Exception as capture_error:
                    result['partial_capture_error'] = str(capture_error)
            assistance = sum(-row[2] for row in result['native_transactions'] if row[1] == 12 and row[2] == -6000000)
            observed = result.get('partial_state', (result.get('trace') or [{}])[-1].get('result', {}).get('state', {}))
            reconciled_cash = 100000 + assistance + receipts - primary_debits
            result['financial_reconciliation'] = {'starting_cash': 100000, 'assistance': assistance,
                'native_receipts_excluding_assistance': receipts, 'native_gross_debits': primary_debits,
                'disposable_control_gross_debits': control_debits, 'campaign_gross_debit_cap_usage': primary_debits + control_debits,
                'ending_cash': observed.get('money'), 'debt': observed.get('loan'),
                'native_net_excluding_assistance': receipts - primary_debits,
                'recorded_transactions_ending_cash': reconciled_cash,
                'unobserved_native_cash_delta': None if 'money' not in observed else observed['money'] - reconciled_cash}
            raise
        finally:
            if engine is not None:
                engine.close()
            persist()

    try:
        attempt(output / 'primary', report['primary'])
        attempt(output / 'replay', report['replay'])
        for key in ('pristine', 'preflight', 'construction', 'trace', 'negative_control', 'cold_before', 'cold_after', 'final', 'advance_counts', 'financial_reconciliation'):
            require(report['primary'][key] == report['replay'][key], f'Independent identical replay differs in {key}')
        report['repeatability'] = 'PASS'
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = str(error)
        report['status'] = 'PARTIAL' if report['primary'].get('status') == 'PARTIAL' or report['replay'].get('status') == 'PARTIAL' else 'FAIL'
        raise
    finally:
        persist()
    print(f'ASSISTED FUNCTIONAL PASS; economics unproven; human UAT Pending. Evidence: {output}', flush=True)


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
    """Execute command/ack synchronously from one stdin line; retain the native script."""
    sequence = getattr(engine, '_generation_ack_number', 0) + 1
    engine._generation_ack_number = sequence
    script = (Path(engine.log.name).parent / f'generation-ack-{sequence:03}.scr').resolve()
    require('"' not in str(script) and '\n' not in str(script), 'Proof acknowledgement script cannot be safely quoted')
    with script.open('x') as stream:
        stream.write(command + '\n' + f'echo {marker}\n')
    engine.command(f'exec "{script}"', marker)


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
    modes.add_argument('--functional-core', action='store_true', help='Reviewed seed11 ASSISTED FUNCTIONAL Core supply/Materials I; one GBP6m offline grant')
    parser.add_argument('--seeds', type=int, nargs='+', default=[11, 101, 2026])
    args = parser.parse_args()
    if args.functional_core:
        return functional_campaign(args)
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
