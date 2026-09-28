#!/usr/bin/env python3
"""Three real servers and joined clients: native coal delivery through an intermediate world, then cold reload."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time
from test_federation_multiplayer import ROOT, Session, free_port
from federation_checkpoint import validate

class StellarSession(Session):
    def setup(self, checkpoint=None):
        if checkpoint:
            validate(checkpoint,self.binary)
            shutil.copyfile(Path(checkpoint)/"authority.json",self.output/"authority.json")
        token="disposable-stellar-test-hosts"
        self.spawn("authority",["python3",str(ROOT/"scripts/universe_authority.py"),"--host","127.0.0.1","--port",self.url.rsplit(":",1)[1],"--state-file",str(self.output/"authority.json")],{**os.environ,"OPENTTD_UNIVERSE_HOST_TOKEN":token})
        deadline=time.monotonic()+10
        while True:
            try:
                self.api("/health"); break
            except OSError:
                if time.monotonic()>deadline: raise
                time.sleep(.1)
        base=(ROOT/"demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]
        base=base.replace("[construction]","[construction]\ncommand_pause_level = 3").replace("min_active_clients = 0","min_active_clients = 1").replace("autosave_on_exit = true","autosave_on_exit = false").replace("threaded_saves = true","threaded_saves = false")
        if getattr(self,"integrated",False):
            base += "\n[newgrf]\n" + "".join(f"{ROOT}/bin/newgrf/{name} =\n" for name in ("openspacettd_integrated_v1.grf","openspacettd_rail_v3.grf","openspacettd_equipment_v1.grf"))
        for world in (1,2,3):
            port=free_port(); config=self.output/f"server{world}.cfg"; config.write_text(base)
            env={**os.environ,"OPENSPACETTD_WORLD_COUNT":"1","OPENSPACETTD_AUTHORITY_URL":self.url,"OPENTTD_UNIVERSE_HOST_TOKEN":token,"OPENTTD_UNIVERSE_GAME_ADDRESS":f"127.0.0.1:{port}"}
            game=["-g",str(Path(checkpoint)/f"world{world}.sav")] if checkpoint else ["-g"]
            server=self.spawn(f"server{world}",[str(self.binary),"-D",f"127.0.0.1:{port}","-c",str(config),"-x","-I","OpenGFX",*game,"-G","11","-t","1950","-d","net=2,desync=2"],env,True)
            server["port"]=port; self.servers.append(server); self.wait(server,"paused (",timeout=90)
            if not checkpoint: self.command(server,f"federation_test_fixture {world} {'integrated' if getattr(self,'integrated',False) else 'multihop'}","Federation fixture ready:")
        for world,server in enumerate(self.servers,1):
            config=self.output/f"client{world}.cfg"; config.write_text(f"[network]\nclient_name = Native Observer {world}\n[video]\nfullscreen = false\n[gui]\nautosave_on_exit = false\n")
            env={**os.environ,"SDL_VIDEODRIVER":"dummy"}; env.pop("OPENSPACETTD_AUTHORITY_URL",None)
            args=[str(self.binary),"-n",f"127.0.0.1:{server['port']}#255","-c",str(config),"-x","-I","OpenGFX","-r","800x600","-s","null","-m","null","-v","sdl","-d","net=2,desync=2"]
            self.clients.append(self.spawn(f"client{world}",args,env))
            self.wait(server,f"Native Observer {world} has joined the game"); self.join_company(world)
        self.write_manifest()

    def deliveries(self,count,baseline=0,timeout=1800):
        deadline=time.monotonic()+timeout; identity=None; reported=baseline
        while time.monotonic()<deadline:
            self.healthy()
            if not (self.output/"authority.json").exists():
                time.sleep(.5); continue
            authority=json.loads((self.output/"authority.json").read_text())
            tx=[t for t in authority["transfers"].values() if t["state"]=="COMPLETED"]
            returns=[t for t in tx if t["source_world"]==2 and t["dest_world"]==1]
            outbound=[t for t in tx if t["source_world"]==2 and t["dest_world"]==3]
            assert all(t["total_cargo"]==0 for t in returns),returns
            assert all(t["total_cargo"]>0 for t in outbound),outbound
            states=self.state()
            trains=[t for s in states for t in s["trains"] if t["global_id"]]
            assert len(trains)<=1,states
            for train in trains:
                if identity is None: identity=train["global_id"]
                assert identity==train["global_id"],train
                assert len(train["units"])==3,train
                assert [o["world"] for o in train["orders"]]==[1,3],train
                assert train["orders"][0]["load"]==2,train
            if len(returns)>reported:
                reported=len(returns); print(f"Three-server round trips: {reported}",flush=True)
            if len(returns)>=baseline+count:
                return {"returns":len(returns),"delivered":sum(t["total_cargo"] for t in outbound),"identity":identity}
            time.sleep(.5)
        raise RuntimeError(f"Multihop deadline exceeded: {self.state()}")

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("--output",type=Path,required=True); parser.add_argument("--binary",type=Path,default=ROOT/"build/openttd"); parser.add_argument("--deliveries",type=int,default=5)
    args=parser.parse_args(); args.output.mkdir(parents=True,exist_ok=False)
    first=StellarSession(args.output,args.binary.resolve(),False,True)
    try:
        first.setup(); before=first.begin_accounting(); first.resume(); trips=first.deliveries(args.deliveries)
        checkpoint=first.checkpoint("multihop-verified","confirmed-return"); accounting=first.finish_accounting(before)
        assert accounting["consumed"]==trips["delivered"]>0,accounting
    finally:
        first.close()
    reload_dir=args.output/"cold-reload"; reload_dir.mkdir(); second=StellarSession(reload_dir,args.binary.resolve(),False,True)
    try:
        second.setup(checkpoint); before=second.begin_accounting(); second.resume(); resumed=second.deliveries(1,trips["returns"])
        second.checkpoint("reloaded-verified","confirmed-return"); recovery=second.finish_accounting(before)
        assert resumed["identity"]==trips["identity"]
        report={"passed":args.deliveries>=5,"three_joined_clients":True,"trips":trips,"cold_reload":resumed,"accounting":accounting,"recovery_accounting":recovery}
        (args.output/"result.json").write_text(json.dumps(report,indent=2)); print("Three-server freight and cold reload verified",flush=True)
    finally:
        second.close()

if __name__=="__main__": main()
