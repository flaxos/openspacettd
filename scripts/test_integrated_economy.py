#!/usr/bin/env python3
"""Exercise the integrated rules with the existing native connected-economy authoring harness."""
import argparse
import json
import time
import hashlib
from pathlib import Path
from test_wp11_slice import Engine, ROOT, require


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
    packs = ('integrated_v1', 'rail_v3', 'equipment_v1')
    text = (ROOT / 'demo/integrated_economy.cfg').read_text().replace(
        'min_active_clients = 0', 'min_active_clients = 1')
    for name in packs:
        text = text.replace(f'openspacettd_{name}.grf =',
                            f'{ROOT}/bin/newgrf/openspacettd_{name}.grf =')
    report = {'passed': False, 'human_uat': 'Pending', 'mode': 'ordinary-first-freight',
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'pack_sha256': {name: hashlib.sha256((ROOT / 'bin/newgrf' /
                  f'openspacettd_{name}.grf').read_bytes()).hexdigest() for name in packs},
              'steps_per_phase_limit': args.steps, 'seeds': []}
    import subprocess
    report['source_commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    report['source_dirty'] = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    for seed in args.seeds:
        folder = output / str(seed)
        folder.mkdir()
        config = folder / 'newgame.cfg'
        config.write_text(text.replace('generation_seed = 11', f'generation_seed = {seed}'))
        row = {'requested_seed': seed, 'passed': False, 'plans': [], 'samples': []}
        report['seeds'].append(row)
        engine = None
        try:
            engine = Engine(binary, config, folder, 'generation', world_count=7, seed=seed)
            engine.deadline = time.monotonic() + 3600
            initial = json.loads(engine.command('connected_economy first-freight-start', 'FREIGHT state '))
            row['initial'] = initial
            require(initial['seed'] == seed, 'Resolved generator seed differs')
            require(initial['economy']['version'] == 1, 'Integrated rules inactive')
            require(initial['money'] == initial['loan'] == 100000, 'Nonordinary initial finances')
            require(not initial['trains'] and not initial['stations'] and not initial['research'] and not initial['stocks'], 'Unexpected starting player assets or grants')
            require(all(t[1] == 16 for t in initial['rail']), 'Player rail at start')
            engine.save(folder / 'ordinary-start.sav')
            candidates = []
            for label in ('IRON', 'GRAI'):
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
            require(legal, 'Bounded planner found no legal starter route; not proof that every route is impossible')
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
            for step in range(args.steps):
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
            engine.deadline = time.monotonic() + 3600
            restored = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
            row['restored'] = restored
            require(restored == final, 'Cold reload changed native route/orders/cargo/finances/economy state')
            row['cold_reload_equal'] = True
            further = 0
            further_delivered = 0
            row['reload_samples'] = []
            for _ in range(args.steps):
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
        except Exception as exc:
            if engine is not None and engine.process.poll() is None:
                try:
                    row['failure_state'] = json.loads(engine.command('connected_economy first-freight-status', 'FREIGHT state '))
                    engine.save(folder / 'failure.sav')
                except Exception as snapshot_error:
                    row['failure_snapshot_error'] = str(snapshot_error)
            row['failure'] = str(exc)
            print(f'Seed {seed}: FAIL: {exc}', flush=True)
        finally:
            if engine is not None:
                engine.close()
            (folder / 'evidence.json').write_text(json.dumps(row, indent=2) + '\n')
            (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    report['passed'] = all(row['passed'] for row in report['seeds'])
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    require(report['passed'], 'First Sustainable Freight remains unproven; see retained per-seed failures')


def first_food(args):
    """Continue the hash-verified A1 checkpoint; never prepare an authored world."""
    import shutil
    import subprocess
    import tarfile
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    binary = args.binary.resolve()
    limit = min(args.steps, 240)
    require(limit > 0, 'A positive bounded advance limit is required')
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    archive = ROOT / 'docs/audit/2026-09-30/first-freight/native-proof.tar.gz'
    manifest = json.loads((archive.parent / 'native-proof-manifest.json').read_text())
    source_dir = output / 'a1-source'
    source_dir.mkdir()
    with tarfile.open(archive) as saved:
        saved.extractall(source_dir, filter='data')
    source = source_dir / 'build/a1-full-consist-three-seed/11'
    for name in ('operation.sav', 'evidence.json', 'newgame.cfg'):
        entry = next(f for f in manifest['files'] if f['path'] == f'build/a1-full-consist-three-seed/11/{name}')
        require(sha(source / name) == entry['sha256'], f'Changed A1 source {name}')
    require(sha(source / 'operation.sav') == '51ce35b791d419b3b4106a8f63f715bca89ae54f88522997a6295171320b5ce0', 'Wrong reviewed start save')
    original = json.loads((source / 'evidence.json').read_text())['before_reload']
    config = output / 'continuation.cfg'
    text = (ROOT / 'demo/integrated_economy.cfg').read_text().replace('min_active_clients = 0', 'min_active_clients = 1')
    packs = ('integrated_v1', 'rail_v3', 'equipment_v1')
    for name in packs:
        text = text.replace(f'openspacettd_{name}.grf =', f'{ROOT}/bin/newgrf/openspacettd_{name}.grf =')
    config.write_text(text)
    report = {'passed': False, 'human_uat': 'Pending', 'mode': 'ordinary-a1-first-paid-core-food',
              'source_commit': subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip(),
              'source_dirty': bool(subprocess.check_output(['git','status','--porcelain'], cwd=ROOT, text=True).strip()),
              'binary_sha256': sha(binary), 'config_sha256': sha(config), 'start_save_sha256': sha(source/'operation.sav'),
              'pack_sha256': {name: sha(ROOT/'bin/newgrf'/f'openspacettd_{name}.grf') for name in packs},
              'advance_limit': limit, 'phase_wall_seconds_limit': 1800, 'plans': [], 'income_samples': [], 'samples': [], 'reload_samples': []}
    require(report['pack_sha256'] == manifest['pack_sha256'], 'Content differs from A1 proof')
    engine = None
    def state():
        return json.loads(engine.command('connected_economy first-food-status', 'FREIGHT food '))
    def advance(samples, debt):
        sample = json.loads(engine.command('connected_economy first-food-advance', 'FREIGHT food '))
        samples.append(sample)
        require(sample['cash_conserved'] and not sample['cargo_errors'], 'Native cash/cargo reconciliation failed')
        require(sample['money'] > 0 and sample['loan'] == debt, 'Sampled insolvency or changed operating debt')
        require(not any(t['lost'] or t['crashed'] for t in sample['trains']), 'Lost/crashed train')
        require(not any(t[3] for t in sample['research']), 'Unexplained research unlock')
        return sample
    def phase(samples, initial, deliveries_required, months_required):
        engine.deadline = time.monotonic() + 1800
        started = time.monotonic()
        arrivals = {}
        paid = {}
        months = {}
        service = report['built']['services'][1]
        for step in range(limit):
            sample = advance(samples, initial['loan'])
            for tick, vehicle, station in sample['audit']['arrivals']:
                arrivals[vehicle] = (tick, station)
            # Native events from each sample may interleave multiple arrivals/payments;
            # assign each payment to the latest preceding arrival across all samples.
            for tick, vehicle, station, cargo, units, revenue in sample['audit']['payments']:
                if vehicle != service['train'] or station != service['drop'] or cargo != initial['cargo_labels']['FOOD'] or units <= 0 or revenue <= 0:
                    continue
                prior = [a for s in samples for a in s['audit']['arrivals'] if a[1] == vehicle and a[2] == station and a[0] <= tick]
                if not prior:
                    # A cold checkpoint may resume unloading an already-started visit.
                    # Retain that payment in raw samples; require a new observed arrival.
                    require(deliveries_required == 1, 'Paid food has no observed native arrival')
                    continue
                key = max(a[0] for a in prior)
                units_before, cash_before = paid.get(key, (0, 0))
                paid[key] = (units_before + units, cash_before + revenue)
            for date, town, used, remaining in sample['audit']['city_months']:
                if town == report['selected']['town'] and used > 0:
                    require(date not in months, 'Double-counted consuming month')
                    months[date] = (used, remaining)
            print(f'Food {"reload" if deliveries_required == 1 else "initial"} advance {step+1}: arrivals={len(paid)}, months={len(months)}, cash={sample["money"]}', flush=True)
            if len(paid) >= deliveries_required and len(months) >= months_required and sample['money'] > initial['money']:
                break
        totals = {}
        for key in ('service_income', 'service_running', 'vehicle_tolls', 'expenses'):
            totals[key] = {}
            for sample in samples:
                for ident, amount in sample['audit'][key]:
                    totals[key][ident] = totals[key].get(ident, 0) + amount
        food_id = service['train']
        totals['grain_service_net_before_shared_interest'] = sum(totals['service_income'].get(s['train'],0) - totals['service_running'].get(s['train'],0) - totals['vehicle_tolls'].get(s['train'],0) for s in report['built']['services'][:1])
        require(sum(totals['service_income'].values()) == -totals['expenses'].get(7, 0), 'Native completed train income differs from company credits')
        require(sum(totals['service_running'].values()) == totals['expenses'].get(2, 0), 'Native train running charges differ from company debits')
        totals['food_service_net_before_shared_interest'] = totals['service_income'].get(food_id,0) - totals['service_running'].get(food_id,0) - totals['vehicle_tolls'].get(food_id,0)
        totals['added_chain_net_before_shared_interest'] = totals['grain_service_net_before_shared_interest'] + totals['food_service_net_before_shared_interest']
        totals['combined_net'] = sample['money'] - initial['money']
        totals['paid_arrivals'] = [[tick, units, revenue] for tick,(units,revenue) in sorted(paid.items())]
        totals['consuming_months'] = [[date, used, reserve] for date,(used,reserve) in sorted(months.items())]
        totals['advances'] = len(samples)
        totals['wall_seconds'] = time.monotonic()-started
        totals['native_ticks'] = sample['tick']-initial['tick']
        totals['normal_speed_minutes'] = totals['native_ticks']*0.027/60
        require(len(paid) >= deliveries_required and len(months) >= months_required, 'Bounded paid deliveries/consuming months did not complete')
        require(totals['combined_net'] > 0, 'Combined measured operation not profitable')
        return totals
    try:
        engine = Engine(binary, config, output, 'continuation', save=source/'operation.sav', world_count=7, seed=11)
        engine.deadline = time.monotonic()+1800
        initial = state()
        report['initial'] = initial
        require({k:initial[k] for k in original} == original, 'Captured cold A1 checkpoint differs from reviewed before_reload')
        require(initial['money'] == 78268 and initial['loan'] == 100000 and initial['tick'] == 85248, 'Wrong ordinary starting state')
        report['a1_cold_equal'] = True
        report['audit'] = audit(engine)
        towns = [t for t in initial['towns'] if t['role'] == 'Core' and t['houses']]
        require(towns, 'No generated Core town with actual houses')
        candidates = [(i['id'], t['id'], g['id']) for i in initial['industries']
                      if any(c[0] == initial['cargo_labels']['GRAI'] for c in i['outputs'])
                      for t in towns for g in initial['gates'] if {e['world'] for e in g['ends']} == {i['world'],1}]
        def preview():
            before = state()
            plans = []
            for producer,town,gate in candidates:
                plan = json.loads(engine.command(f'connected_economy first-food-plan {producer} {town} {gate}', 'FREIGHT food-plan '))
                require(plan['preview_unchanged'], 'Preview mutated captured native state')
                plans.append(plan)
            require(state() == before, 'Whole-chain discovery changed captured native state')
            report['plans'].append({'tick':before['tick'], 'money':before['money'], 'plans':plans})
            legal = [p for p in plans if p['legal']]
            require(legal, 'Bounded complete-chain search found no legal route; material replan trigger')
            return min(legal, key=lambda p:p['total_quote'])
        selected = preview()
        if not selected['affordable'] and not args.preflight_only:
            # Earn only through the preserved ordinary native services, with fixed debt.
            for step in range(limit):
                advance(report['income_samples'], initial['loan'])
                if step % 8 == 7:
                    selected = preview()
                    if selected['affordable']:
                        break
            require(selected['affordable'], 'Whole-chain cost plus reserve exceeds bounded ordinary earnings/credit; stop for owner decision')
        report['selected'] = selected
        if args.preflight_only:
            report['preflight_only'] = True
            print(json.dumps(selected, indent=2))
            return
        before_build = state()
        suffix = f'{selected["producer"]} {selected["town"]} {selected["grain_link"]}'
        built = json.loads(engine.command('connected_economy first-food-build '+suffix, 'FREIGHT food-built '))
        report['built'] = built
        current = built['state']
        report['build_cost'] = before_build['money'] + current['loan'] - before_build['loan'] - current['money']
        require(current['money'] >= selected['operating_reserve'], 'Documented operating reserve not retained')
        require(report['build_cost'] <= selected['total_quote'], 'Actual construction/purchase cost exceeded full-chain quote')
        require(len([t for t in current['trains'] if t['front']]) == 4, 'Preserved plus two added services not present')
        station = next(s for s in current['stations'] if s['id'] == built['services'][1]['drop'])
        require(station['town'] == selected['town'] and station['consumer'] and not station['warehouse'] and any(h[1] == selected['town'] for h in station['houses']), 'Incorrect Core consumer station/catchment')
        engine.save(output/'food-construction.sav')
        report['initial_phase'] = phase(report['samples'], current, 3, 3)
        before_reload = state()
        report['before_reload'] = before_reload
        save = output/'food-operation.sav'
        engine.save(save)
        report['operation_save_sha256'] = sha(save)
        engine.close()
        engine = Engine(binary, config, output, 'cold-reload', save=save, world_count=7, seed=11)
        engine.deadline = time.monotonic()+1800
        restored = state()
        report['restored'] = restored
        require(restored == before_reload, 'Cold continuation changed captured routes/orders/designation/custody/cash')
        report['cold_reload_equal'] = True
        report['reload_phase'] = phase(report['reload_samples'], restored, 1, 1)
        report['final'] = state()
        engine.save(output/'food-final.sav')
        report['final_save_sha256'] = sha(output/'food-final.sav')
        require(sum(s['audit']['consumed'][initial['cargo_labels']['GRAI']] for s in report['samples']) > 0, 'No real grain processing')
        require(sum(s['audit']['produced'][initial['cargo_labels']['FOOD']] for s in report['samples']) > 0, 'No real FOOD output')
        require(sum(report['initial_phase']['expenses'].values()) == -report['initial_phase']['combined_net'], 'Company expenses/net mismatch across year rollover')
        require(sum(report['reload_phase']['expenses'].values()) == -report['reload_phase']['combined_net'], 'Reload expenses/net mismatch')
        report['passed'] = True
    except Exception as exc:
        report['failure'] = str(exc)
        if engine is not None and engine.process.poll() is None:
            try:
                report['failure_state'] = state()
                engine.save(output/'failure.sav')
            except Exception as snapshot_error:
                report['failure_snapshot_error'] = str(snapshot_error)
        raise
    finally:
        if engine is not None:
            engine.close()
        (output/'evidence.json').write_text(json.dumps(report, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'build/openttd')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=500)
    parser.add_argument('--first-food', action='store_true', help='Continue preserved seed11 A1 save through paid Core consumption')
    parser.add_argument('--preflight-only', action='store_true', help='Retain full food-chain quotes without spending')
    parser.add_argument('--first-freight', action='store_true', help='Ordinary generated start; no showcase preparation')
    parser.add_argument('--seeds', type=int, nargs='+', default=[11, 101, 2026])
    args = parser.parse_args()
    require(not (args.first_food and args.first_freight), 'Choose exactly one ordinary proof mode')
    require(not args.preflight_only or args.first_food, 'Preflight-only requires first-food continuation')
    if args.first_food:
        return first_food(args)
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
