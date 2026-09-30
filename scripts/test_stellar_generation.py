#!/usr/bin/env python3
"""Generate the authored CST sector with its published content and cold-reload the complete native save."""
import argparse
import hashlib
import json
from pathlib import Path
from test_wp11_slice import Engine, ROOT, require

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/openttd')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--legacy-industry-toggle', action='store_true', help='Verify integrated rules override the old random-industry preference')
    parser.add_argument('--seed', type=int, default=11, help='Native generation seed')
    parser.add_argument('--integrated', action='store_true', help='Verify the versioned Integrated Commonwealth content and saved rules')
    args=parser.parse_args(); output=args.output.resolve(); output.mkdir(parents=True,exist_ok=False)
    text=(ROOT/'demo/wp11_slice.cfg').read_text().split('[newgrf]')[0]
    text=text.replace('generation_seed = 11', f'generation_seed = {args.seed}')
    text=text.replace('player_built_economy = false','player_built_economy = true\ncst_sector = true\nresource_density = 1')
    text=text.replace('map_x = 9','map_x = 10').replace('map_y = 7','map_y = 10').replace('autosave_on_exit = true','autosave_on_exit = false').replace('threaded_saves = true','threaded_saves = false')
    text+='\n[newgrf]\nopenspacettd_industry_v4.grf =\nopenspacettd_rail_v3.grf =\nopenspacettd_equipment_v1.grf =\n'
    if args.integrated: text=text.replace('openspacettd_industry_v4.grf', 'openspacettd_integrated_v1.grf')
    if args.legacy_industry_toggle: text=text.replace('player_built_economy = true', 'player_built_economy = false')
    config=output/'stellar.cfg'; config.write_text(text)
    binary=args.binary.resolve(); engine=Engine(binary,config,output,'generation',world_count=7,seed=args.seed)
    try:
        initial=json.loads(engine.command('resource_sites','RESOURCE state '))
        require(len(initial['stellar_worlds'])==7,'Wrong stellar catalogue')
        if args.integrated:
            economy = initial['integrated_economy']
            require(economy['version'] == 1, 'Integrated economy did not activate')
            require(initial['player_built'], 'Integrated economy allowed legacy random industry expansion')
            require([row[1] for row in economy['roles']] == [2,1,0,0,0,1,0], 'Incorrect stable economic roles')
            require(not economy['cities'], 'Cities must start without injected reserves')
            accessible = {row[1] for row in initial['resource_details'] if row[0] in (2, 3)}
            require({'GRAI', 'SILC', 'IRON', 'OIL_', 'COPR', 'SAND'} <= accessible, 'Bootstrap frontier resources are inaccessible')
            require(any(row[1] == 'RARE' and row[0] in (4, 6) for row in initial['resource_details']), 'Missing rare-mineral expansion')
        require(len(initial['landing_zones'])==21,'Three verified landing sites required per world')
        require(len(initial['gate_policies'])==6,'Three shared CST backbone links required')
        require([w[5] for w in initial['stellar_worlds']]==[True]*4+[False]*3,'Incorrect starting access')
        require(initial['invalid_height_edges']==0,'Invalid terrain edge')
        require(initial['unoccupied_sites']>0 and initial['machine_cargo']!=255,'Missing hidden resources or machine modules')
        require(0<initial['processing']<=14,'Starter processors must be bounded')
        save=output/'stellar-sector.sav'; engine.save(save)
    finally: engine.close()
    loaded=Engine(binary,config,output,'reload',save=save,world_count=7)
    try:
        restored=json.loads(loaded.command('resource_sites','RESOURCE state ')); require(restored==initial,'Stellar generation state changed during reload')
    finally: loaded.close()
    report={'passed':True,'seed':args.seed,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'state':initial,'save_sha256':hashlib.sha256(save.read_bytes()).hexdigest()}
    (output/'evidence.json').write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__': main()
