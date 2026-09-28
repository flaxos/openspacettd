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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'build/openttd')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=500)
    args = parser.parse_args()
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
