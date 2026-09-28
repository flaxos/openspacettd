#!/usr/bin/env python3
"""Three native hosts: integrated physical freight, shared research, outage and cold reload."""
import argparse
import hashlib
import json
import time
from pathlib import Path
from test_stellar_multihop import StellarSession
from test_federation_multiplayer import ROOT


def research(session, completed=False, timeout=150):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        session.healthy()
        states=session.state()
        rows=[s['research'][0] for s in states]
        homes={r['home'] for r in rows}
        if len(homes)==1 and '' not in homes and [r['can_research'] for r in rows]==[False,True,False] and (not completed or all(r['materials_i'] for r in rows)):
            return states
        time.sleep(.5)
    raise RuntimeError(f'Research replication timeout: {rows}')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/openttd')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--deliveries',type=int,default=5)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    first=StellarSession(args.output,args.binary.resolve(),False,True);first.integrated=True
    try:
        first.setup();before=first.begin_accounting();first.resume()
        home=research(first)
        first.command(first.servers[0],'federation_fixture_block research')
        first.command(first.servers[1],'federation_fixture_block research')
        unlocked=research(first,True)
        assert unlocked[0]['research'][0]['active']==0
        authority=next(p for p in first.processes if p['name']=='authority')
        first.stop_record(authority)
        deadline=time.monotonic()+35
        while time.monotonic()<deadline:
            cached=research(first,True,5)
            assert all(s['research'][0]['home']==home[0]['research'][0]['home'] for s in cached)
            time.sleep(1)
        first.restart(authority)
        trips=first.deliveries(args.deliveries)
        checkpoint=first.checkpoint('integrated-verified','confirmed-return')
        accounting=first.finish_accounting(before)
        assert accounting['consumed']>0
        first_phase={'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),'trips':trips,'accounting':accounting,'research_home':home[0]['research'][0]['home'],'research_replicated':True,'cached_through_outage':True}
        (args.output/'first-phase.json').write_text(json.dumps(first_phase,indent=2)+'\n')
    finally:first.close()
    reload_dir=args.output/'cold-reload';reload_dir.mkdir()
    second=StellarSession(reload_dir,args.binary.resolve(),False,True);second.integrated=True
    try:
        second.setup(checkpoint);before=second.begin_accounting();second.resume()
        restored=research(second,True)
        resumed=second.deliveries(1,trips['returns'])
        second.checkpoint('integrated-reloaded','confirmed-return')
        recovery=second.finish_accounting(before)
        assert resumed['identity']==trips['identity']
        report={'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),'passed':args.deliveries>=5,'three_joined_clients':True,'research_home':home[0]['research'][0]['home'],'research_replicated':True,'cached_through_outage':True,'cold_reload':True,'trips':trips,'resumed':resumed,'accounting':accounting,'recovery':recovery}
        (args.output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print('Integrated three-host economy and research verified',flush=True)
    finally:second.close()

if __name__=='__main__':main()
