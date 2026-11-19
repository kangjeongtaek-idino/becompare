"""Cross-process schedules for the production guards; called by test_regressions.py."""
from pathlib import Path
import os, subprocess, time

def process_checks(exe, fixture, env):
    fixture=Path(fixture);fixture.mkdir(parents=True,exist_ok=True)
    def pair(mode,target,expected):
        children=[]
        for name,want in zip('AB',expected):
            ready=fixture/f'{mode}-{name}.ready';go=fixture/f'{mode}-{name}.go'
            p=subprocess.Popen([str(exe),mode,str(target),name,str(ready),str(go),str(want)],cwd=fixture,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
            children.append((p,ready,go))
        try:
            deadline=time.monotonic()+10
            while not all(ready.exists() for p,ready,go in children):
                assert time.monotonic()<deadline,(mode,'children did not reach barrier')
                time.sleep(.01)
            for p,ready,go in children:
                go.touch();out,err=p.communicate(timeout=15)
                assert p.returncode==0,(mode,p.returncode,out,err)
        finally:
            for p,ready,go in children:
                if p.poll() is None:p.kill();p.wait()
    shared=fixture/'shared.bin';shared.write_bytes(b'BASE')
    pair('save',shared,[1,0]);assert shared.read_bytes()==b'A'
    print('PASS processes: later stale save cannot overwrite earlier save')
    ini=fixture/'recent.ini';ini.write_text('[Recent]\nR0=ORIGINAL\n',encoding='ascii')
    children=[]
    def wait_ready(p,ready):
        deadline=time.monotonic()+10
        while not ready.exists():
            assert p.poll() is None and time.monotonic()<deadline,('recent worker failed to reach barrier',p.poll())
            time.sleep(.01)
    try:
        for name in 'AB':
            ready=fixture/f'recent-{name}.ready';go=fixture/f'recent-{name}.go'
            p=subprocess.Popen([str(exe),'recent',str(ini),name,str(ready),str(go),'0'],cwd=fixture,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
            children.append((p,ready,go))
            if name=='A':wait_ready(p,ready)
        time.sleep(.2)
        assert not children[1][1].exists() and children[1][0].poll() is None,'second recent_add read inside the first transaction'
        for p,ready,go in children:
            wait_ready(p,ready);go.touch();out,err=p.communicate(timeout=15)
            assert p.returncode==0,(p.returncode,out,err)
    finally:
        for p,ready,go in children:
            if p.poll() is None:p.kill();p.wait()
    text=ini.read_text(encoding='ascii')
    assert all(x in text for x in ['R0=B','R1=A','R2=ORIGINAL']),text
    print('PASS processes: recent_add preserves both process updates')
    temps=[]
    ps=[subprocess.Popen([str(exe),'temp'],cwd=fixture,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW) for _ in range(8)]
    try:
        for p in ps:
            out,err=p.communicate(timeout=10);assert p.returncode==0,(p.returncode,err);temps.append(out.strip())
    finally:
        for p in ps:
            if p.poll() is None:p.kill();p.wait()
    assert len(set(temps))==8,temps
    assert all(Path(path).is_file() for path in temps),temps
    print('PASS processes: eight conversions reserve different files despite identical timer ticks')
    # A settings transaction and a save must never acquire each other's locks backwards.
    lock_ini=fixture/'lock-order.ini';lock_ini.write_text('[UI]\na=value\n',encoding='ascii')
    children=[]
    try:
        for mode in ['lock-settings','lock-save']:
            ready=fixture/f'{mode}.ready';go=fixture/f'{mode}.go'
            p=subprocess.Popen([str(exe),mode,str(lock_ini),str(shared),str(ready),str(go),'0'],cwd=fixture,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
            children.append((p,ready,go));wait_ready(p,ready)
        children[1][2].touch();time.sleep(.1);children[0][2].touch()
        for p,ready,go in children:
            out,err=p.communicate(timeout=3);assert p.returncode==0,(p.returncode,out,err)
    finally:
        for p,ready,go in children:
            if p.poll() is None:p.kill();p.wait()
    print('PASS processes: settings snapshot and text-save lock order cannot deadlock')
    return 4
