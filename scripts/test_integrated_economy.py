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
              'seeds': []}
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'build/openttd')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=500)
    parser.add_argument('--first-freight', action='store_true', help='Ordinary generated start; no showcase preparation')
    parser.add_argument('--seeds', type=int, nargs='+', default=[11, 101, 2026])
    args = parser.parse_args()
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
