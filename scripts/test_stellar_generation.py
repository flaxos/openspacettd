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
    args=parser.parse_args(); output=args.output.resolve(); output.mkdir(parents=True,exist_ok=False)
    text=(ROOT/'demo/wp11_slice.cfg').read_text().split('[newgrf]')[0]
    text=text.replace('player_built_economy = false','player_built_economy = true\ncst_sector = true\nresource_density = 1')
    text=text.replace('map_x = 9','map_x = 10').replace('map_y = 7','map_y = 10').replace('autosave_on_exit = true','autosave_on_exit = false').replace('threaded_saves = true','threaded_saves = false')
    text+='\n[newgrf]\nopenspacettd_industry_v4.grf =\nopenspacettd_rail_v3.grf =\nopenspacettd_equipment_v1.grf =\n'
    config=output/'stellar.cfg'; config.write_text(text)
    binary=args.binary.resolve(); engine=Engine(binary,config,output,'generation',world_count=7)
    try:
        initial=json.loads(engine.command('resource_sites','RESOURCE state '))
        require(len(initial['stellar_worlds'])==7,'Wrong stellar catalogue')
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
    report={'passed':True,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'state':initial,'save_sha256':hashlib.sha256(save.read_bytes()).hexdigest()}
    (output/'evidence.json').write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__': main()
