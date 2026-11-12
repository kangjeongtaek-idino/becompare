"""Build and exercise real NASM objects and unattended GUI CLI in isolated fixtures.
Requires Windows, Python 3, NASM and MinGW-w64 (gcc, windres, objcopy).
No desktop input automation, user files or installed settings are used.
"""
from pathlib import Path
import re
import os, re, subprocess, tempfile, json
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "regression"
OUT.mkdir(parents=True, exist_ok=True)
def _mods():
    # keep in step with build-gui.ps1: its $optional list plus the three mandatory modules
    text = (ROOT / "build-gui.ps1").read_text(encoding="utf-8")
    opt = re.search(r"\$optional = ([^\r\n]+)", text).group(1)
    names = re.findall(r"'([a-z0-9_]+)'", opt)
    return ["util", "main", "home"] + names + ["stubs"]


MODS = _mods()
LIBS = ["-l"+s for s in "user32 gdi32 comctl32 comdlg32 shell32 ole32 dwmapi wininet gdiplus uxtheme shlwapi version advapi32 normaliz".split()]
LOG = []
def run(args, cwd=ROOT, timeout=30, expected=0, env=None):
    r = subprocess.run([str(a) for a in args], cwd=cwd, timeout=timeout, capture_output=True, text=True, errors="replace", env=env, creationflags=subprocess.CREATE_NO_WINDOW)
    if r.stdout: print(r.stdout, end="")
    if r.stderr: print(r.stderr, end="")
    assert r.returncode == expected, (args, r.returncode, expected, r.stdout, r.stderr)
    return r
for m in MODS:
    run(["nasm","-f","win64","-I","./",m+".asm","-o",OUT/(m+".obj")],ROOT/"gui")
run(["windres","res/app.rc","-O","coff","-o",OUT/"app.res.o"],ROOT/"gui")
run(["gcc","-mwindows",*[OUT/(m+".obj") for m in MODS],OUT/"app.res.o","-o",OUT/"bcgui.exe",*LIBS])
r=run(["python","tools/hazard.py"])
assert "hazards: 0" in r.stdout
run(["gcc",ROOT/"tools/test_safe_save.c",OUT/"engine.obj",OUT/"textlib.obj",*["-Wl,--wrap="+x for x in ["WriteFile","FlushFileBuffers","ReplaceFileW"]],"-o",OUT/"test_safe_save.exe"])
defs={}
for name in ["inc/app.inc","text.asm","edit_ui.inc","hex.asm","merge.asm","folder.asm"]:
    for line in (ROOT/"gui"/name).read_text(encoding="utf-8").splitlines():
        m=re.match(r"^%define\s+((?:SS_|TV_|HX_|MG_|F_|BC_)[A-Z0-9_]+|E_HR|E_SIZE)\s+([^;]+)",line)
        if m: defs[m[1]]=m[2].strip()
(OUT/"test_layout.h").write_text("\n".join("#define "+k+" "+v for k,v in defs.items()),encoding="utf-8")
exports={"main":["view_can_close","frame_proc","g_nsess","g_sess"],"text":["text_compare","tv_proc","ed_proc"],"hex":["hx_proc"],"merge":["mg_proc"],"folder":["fold_proc","sync_thread"]}
for mod,names in exports.items():
    run(["objcopy",*["--globalize-symbol="+n for n in names],OUT/(mod+".obj"),OUT/(mod+".test.obj")])
run(["gcc","-I",OUT,ROOT/"tools/test_view_state.c",*[OUT/(m+(".test" if m in exports else "")+".obj") for m in MODS],*["-Wl,--wrap="+x for x in ["SendMessageW","GetWindowLongPtrW","GetDlgItem","GetWindowTextW","SetWindowTextW","InvalidateRect","msgbox","DestroyWindow","EnableWindow","SetTimer","KillTimer"]],*LIBS,"-o",OUT/"test_view_state.exe"])
# Keep the fixture directory and logs for inspection. Each run has independent paths.
fixtures=Path(tempfile.mkdtemp(prefix="fixtures-",dir=OUT))
print("Fixtures:",fixtures)
for name in ["test_safe_save","test_view_state"]:
    r=run([OUT/(name+".exe")],fixtures)
    (fixtures/(name+".log")).write_text(r.stdout,encoding="utf-8")
for d in ["left","right","emptyA","emptyB","dirLeft/only-left","dirRight","local-app-data","detail_files/0.html","syncLeft","syncRight"]:
    (fixtures/d).mkdir(parents=True,exist_ok=True)
(fixtures/"left/file.txt").write_text("left",encoding="utf-8")
(fixtures/"right/file.txt").write_text("different",encoding="utf-8")
env=dict(os.environ,LOCALAPPDATA=str(fixtures/"local-app-data"))
def cli(name,args,expected):
    r=run([OUT/"bcgui.exe",*args],fixtures,timeout=10,expected=expected,env=env)
    LOG.append(dict(name=name,exit=r.returncode,pass_=True))
    print("PASS",name)
for name,left,right,output,expected in [
    ("equal folders","emptyA","emptyB","equal.html",0),
    ("different files","left","right","diff.html",1),
    ("report output failure","emptyA","emptyB","absent/out.html",2),
    ("missing input folder","absent","emptyB","missing.html",2),
    ("empty directory difference","dirLeft","dirRight","dirs.html",1),
    ("detail report output failure","left","right","detail.html",2),
]:
    cli(name,["folder",str(fixtures/left),str(fixtures/right),"/report",str(fixtures/output)],expected)
assert not (fixtures/"absent/out.html").exists()
assert "only-left" in (fixtures/"dirs.html").read_text(encoding="utf-8-sig")
cli("snapshot output failure",["folder",str(fixtures/"emptyA"),str(fixtures/"emptyB"),"/snapshot",str(fixtures/"absent/snapshot.bcss")],2)
(fixtures/"syncLeft/new.txt").write_text("sync fixture",encoding="utf-8")
cli("update synchronization",["folder",str(fixtures/"syncLeft"),str(fixtures/"syncRight"),"/sync","update"],0)
assert (fixtures/"syncRight/new.txt").read_text(encoding="utf-8")=="sync fixture"
# ---- /xlsx and /docx: exit code 0 same / 1 different / 2 error, never a window left waiting, well-formed XML
import zipfile, xml.dom.minidom
def part(path,name):
    with zipfile.ZipFile(path) as z: return z.read(name).decode("utf-8")
ex=fixtures/"export"; ex.mkdir()
(ex/"a.txt").write_bytes(b"one\r\ntwo\r\nthree\r\n")
(ex/"same.txt").write_bytes(b"one\r\ntwo\r\nthree\r\n")
(ex/"b.txt").write_bytes(b"one\r\n2\r\nthree\r\nfour\r\n")
(ex/"a.csv").write_bytes(b"id,name\r\n1,x\r\n2,y\r\n")
(ex/"b.csv").write_bytes(b"id,name\r\n1,x\r\n2,z\r\n")
(ex/"a.bin").write_bytes(bytes(range(64)))
(ex/"b.bin").write_bytes(bytes(range(5))+b"\xc8"+bytes(range(6,64)))
# U+FFFE / U+FFFF / a lone surrogate are not XML characters
(ex/"odd.txt").write_bytes(b"\xff\xfe"+"a￾b￿c".encode("utf-16-le")+b"\x00\xd8"+'d <&> "q"\r\n'.encode("utf-16-le"))
# cell references get longer from row 100,000 on (the cell start used to be cut there)
(ex/"bigA.txt").write_bytes("".join(f"line {i}\r\n" for i in range(1,100011)).encode())
(ex/"bigB.txt").write_bytes("".join((f"LINE {i} changed\r\n" if i%50000==0 else f"line {i}\r\n") for i in range(1,100011)).encode())
def bmp(path,v):  # 2x2 24-bit bitmap whose last pixel is (v,v,v)
    head=bytes([0x42,0x4D,70,0,0,0,0,0,0,0,54,0,0,0,40,0,0,0,2,0,0,0,2,0,0,0,1,0,24,0,0,0,0,0,16,0,0,0,0x13,0x0B,0,0,0x13,0x0B,0,0,0,0,0,0,0,0,0,0])
    path.write_bytes(head+bytes([10,20,30,40,50,60,0,0,70,80,90,v,v,v,0,0]))
bmp(ex/"p1.bmp",100); bmp(ex/"p1same.bmp",100); bmp(ex/"p2.bmp",200)
for d,t in [("fa","left"),("fb","right"),("fc","base"),("fsame1","same"),("fsame2","same")]:
    (ex/d).mkdir(); (ex/d/"x.txt").write_text(t,encoding="utf-8")
for name,args,expected in [
    ("xlsx text same",["text","a.txt","same.txt","/xlsx","t0.xlsx"],0),
    ("xlsx text different",["text","a.txt","b.txt","/xlsx","t1.xlsx"],1),
    ("xlsx table same",["table","a.csv","a.csv","/xlsx","b0.xlsx"],0),
    ("xlsx table different",["table","a.csv","b.csv","/xlsx","b1.xlsx"],1),
    ("xlsx hex same",["hex","a.bin","a.bin","/xlsx","h0.xlsx"],0),
    ("xlsx hex different",["hex","a.bin","b.bin","/xlsx","h1.xlsx"],1),
    ("xlsx characters XML cannot hold",["text","odd.txt","a.txt","/xlsx","odd.xlsx"],1),
    ("xlsx beyond row 100,000",["text","bigA.txt","bigB.txt","/xlsx","big.xlsx"],1),
    ("docx merge with conflicts",["merge","a.txt","b.txt","/docx","m.docx"],1),
    ("docx merge without conflicts",["merge","a.txt","same.txt","/docx","m0.docx"],0),
    ("docx picture same",["picture","p1.bmp","p1same.bmp","/docx","p0.docx"],0),
    ("docx picture different",["picture","p1.bmp","p2.bmp","/docx","p1.docx"],1),
    ("docx folder merge with a conflict",["foldermerge","fa","fb","fc","/docx","f1.docx"],1),
    ("docx folder merge without conflicts",["foldermerge","fsame1","fsame2","/docx","f0.docx"],0),
    ("docx missing folder merge input",["foldermerge","nodir1","nodir2","/docx","e7.docx"],2),
    # /exit alone: the first run ends the app with the same exit codes
    ("exit text different",["text","a.txt","b.txt","/exit"],1),
    ("exit text same",["text","a.txt","same.txt","/exit"],0),
    ("exit text missing input",["text","nope1.txt","nope2.txt","/exit"],2),
    ("exit with nothing to run",["text","a.txt","/exit"],0),
    ("exit editor",["edit","a.txt","/exit"],0),
    ("exit merge leaves no save prompt",["merge","a.txt","b.txt","/exit"],1),
    ("exit picture different",["picture","p1.bmp","p2.bmp","/exit"],1),
    ("exit folder different",["folder","fa","fb","/exit"],1),
    ("exit folder same",["folder","fsame1","fsame2","/exit"],0),
    # a job that cannot start must not leave a window behind
    ("switch without a session",["/xlsx","w5.xlsx"],2),
    ("report without both folders",["folder","fa","/report","w6.html"],2),
    ("xlsx output failure",["text","a.txt","b.txt","/xlsx","absent/o.xlsx"],2),
    ("xlsx missing text input",["text","nope1.txt","nope2.txt","/xlsx","e1.xlsx"],2),
    ("xlsx missing table input",["table","nope1.csv","nope2.csv","/xlsx","e2.xlsx"],2),
    ("xlsx missing hex input",["hex","nope1.bin","nope2.bin","/xlsx","e3.xlsx"],2),
    ("docx missing merge input",["merge","nope1.txt","nope2.txt","/docx","e4.docx"],2),
    ("docx missing picture input",["picture","nope1.png","nope2.png","/docx","e5.docx"],2),
    ("docx missing version input",["version","nope1.exe","nope2.exe","/docx","e6.docx"],2),
    ("xlsx on a session without it",["merge","a.txt","b.txt","/xlsx","w1.xlsx"],2),
    ("docx on a session without it",["text","a.txt","b.txt","/docx","w2.docx"],2),
    ("xlsx on a folder session",["folder",".",".","/xlsx","w3.xlsx"],2),
    ("xlsx without both inputs",["text","a.txt","/xlsx","w4.xlsx"],2),
]:
    cli(name,[args[0],*[str(ex/a) if not a.startswith("/") else a for a in args[1:]]],expected)
for f,p in [("t1.xlsx","xl/worksheets/sheet1.xml"),("b1.xlsx","xl/worksheets/sheet1.xml"),("h1.xlsx","xl/worksheets/sheet1.xml"),("odd.xlsx","xl/worksheets/sheet1.xml"),("big.xlsx","xl/worksheets/sheet1.xml"),("m.docx","word/document.xml")]:
    xml.dom.minidom.parseString(part(ex/f,p))
big=part(ex/"big.xlsx","xl/worksheets/sheet1.xml")
assert big.count("<row ")==100011
row=re.search(r'<row r="100001">.*?</row>',big).group(0)
assert re.findall(r'<t xml:space="preserve">(.*?)</t>',row)==["변경","line 100000","LINE 100000 changed"], row
assert not any((ex/f).exists() for f in ["w1.xlsx","w2.docx","w3.xlsx","w4.xlsx","w5.xlsx","w6.html"])
# a compare that could not read its inputs leaves no report of an empty result
assert not any((ex/f).exists() for f in ["e4.docx","e7.docx"])
print("PASS export file contents")
(fixtures/"cli-results.json").write_text(json.dumps(LOG,indent=2),encoding="utf-8")
print("All regression checks passed: 30 save + 19 state/worker +",len(LOG),"CLI cases")
