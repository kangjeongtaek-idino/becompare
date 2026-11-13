"""Build and exercise real NASM objects and unattended GUI CLI in isolated fixtures.
Requires Windows, Python 3, NASM and MinGW-w64 (gcc, windres, objcopy).
No desktop input automation, user files or installed settings are used; the UI test keeps its window hidden.
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
run(["gcc",ROOT/"tools/test_safe_save.c",OUT/"engine.obj",OUT/"textlib.obj",*["-Wl,--wrap="+x for x in ["WriteFile","FlushFileBuffers","ReplaceFileW","ReadFile"]],"-o",OUT/"test_safe_save.exe"])
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
# UI behaviour: the whole application in one process with a hidden window (tools/test_ui.c)
run(["objcopy","--redefine-sym","WinMain=bc_winmain",OUT/"main.obj",OUT/"main.ui.obj"])
run(["gcc",ROOT/"tools/test_ui.c",*[OUT/(m+(".ui" if m=="main" else "")+".obj") for m in MODS],*["-Wl,--wrap="+x for x in ["GetCommandLineW","ShowWindow","GetKeyState","MessageBoxW","update_start"]],OUT/"app.res.o",*LIBS,"-o",OUT/"test_ui.exe"])   # with the manifest: the same common controls as the application
# Keep the fixture directory and logs for inspection. Each run has independent paths.
fixtures=Path(tempfile.mkdtemp(prefix="fixtures-",dir=OUT))
print("Fixtures:",fixtures)
for name in ["test_safe_save","test_view_state"]:
    r=run([OUT/(name+".exe")],fixtures)
    (fixtures/(name+".log")).write_text(r.stdout,encoding="utf-8")
for d in ["left","right","emptyA","emptyB","dirLeft/only-left","dirRight","local-app-data","detail_files/0.html","syncLeft","syncRight"]:
    (fixtures/d).mkdir(parents=True,exist_ok=True)
(fixtures/"ui/local-app-data").mkdir(parents=True)
# every run gets its own temporary folder too: the application cleans %TEMP%\\BeCompareAsm when it ends
tmp=fixtures/"tmp"; tmp.mkdir()
ui_env=dict(os.environ,LOCALAPPDATA=str(fixtures/"ui/local-app-data"),TEMP=str(tmp),TMP=str(tmp))
ui_checks=0
for mode in ["text","merge","fmerge","browse","find","findmft"]:
    r=run([OUT/"test_ui.exe",mode],fixtures/"ui",timeout=200 if mode=="findmft" else 90,env=ui_env)
    if "SKIP" in r.stdout: print(r.stdout.splitlines()[0])
    (fixtures/("test_ui_"+mode+".log")).write_text(r.stdout,encoding="utf-8")
    ui_checks+=int(re.search(r"(\d+) checks, 0 failures",r.stdout).group(1))
(fixtures/"left/file.txt").write_text("left",encoding="utf-8")
(fixtures/"right/file.txt").write_text("different",encoding="utf-8")
env=dict(os.environ,LOCALAPPDATA=str(fixtures/"local-app-data"),TEMP=str(tmp),TMP=str(tmp))
# leftovers under %TEMP%\\BeCompareAsm: of a process that is gone (removed by the next run that ends),
# of a process that is running (this script: kept), and files that are not process folders (kept)
bct=tmp/"BeCompareAsm"
gone=bct/"FFFFFFF0_1A2B"/"A"/"sub"; gone.mkdir(parents=True); (gone/"x.txt").write_text("stale")
alive=bct/f"{os.getpid():X}_1A2B"/"B"; alive.mkdir(parents=True); (alive/"y.txt").write_text("in use")
(bct/"clipboard_12AB.txt").write_text("old layout"); (bct/"notes").mkdir()
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
assert not (bct/"FFFFFFF0_1A2B").exists(), "temporary folder of a process that is gone was not removed"
assert (alive/"y.txt").exists() and (bct/"clipboard_12AB.txt").exists() and (bct/"notes").exists()
print("PASS temporary folders: stale removed, running process and other names kept")
# a folder that cannot be listed makes the scan incomplete: no synchronisation, exit code 2
lk=fixtures/"locked"
for d in ["A/ok","A/shut","B/ok","B/shut"]: (lk/d).mkdir(parents=True)
for side in "AB":
    (lk/side/"ok/f.txt").write_text("x"); (lk/side/"shut/g.txt").write_text("y")
(lk/"B/only.txt").write_text("extra")
subprocess.run(["icacls",str(lk/"A/shut"),"/deny","*S-1-1-0:(OI)(CI)(RX)"],capture_output=True,check=True)
try:
    try:
        os.listdir(lk/"A/shut"); print("SKIP unreadable folder: this account can list it anyway")
    except PermissionError:
        cli("mirror with an unreadable folder",["folder",str(lk/"A"),str(lk/"B"),"/sync","mirror"],2)
        assert (lk/"B/only.txt").exists() and (lk/"B/shut/g.txt").exists(), "an incomplete scan must not delete anything"
finally:
    subprocess.run(["icacls",str(lk/"A/shut"),"/remove:d","*S-1-1-0"],capture_output=True)
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
# many changed rows: 800 (colouring used to fill the message queue, the run never ended) and 2,000
# (4,000 edits: beyond one pass of the diff, which used to report the whole file as one changed block)
def many(n,word): return "\r\n".join(f"value {i} = compute({i}, {word if i%10==0 else 'base'}); // row {i}" for i in range(n)).encode()+b"\r\n"
for n in (8000,20000):
    (ex/f"many{n}a.txt").write_bytes(many(n,"left")); (ex/f"many{n}b.txt").write_bytes(many(n,"right"))
# workbooks with two sheets; the tab order (Summary, Data) is the reverse of the part numbers
def book(path,summary,data):
    def sheet(rows,strings):
        out=[]
        for r,row in enumerate(rows,1):
            cells=[]
            for c,v in enumerate(row):
                ref=chr(65+c)+str(r)
                if isinstance(v,int): cells.append(f'<c r="{ref}"><v>{v}</v></c>')
                else:
                    if v not in strings: strings.append(v)
                    cells.append(f'<c r="{ref}" t="s"><v>{strings.index(v)}</v></c>')
            out.append(f'<row r="{r}">{"".join(cells)}</row>')
        return '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'+"".join(out)+'</sheetData></worksheet>'
    strings=[]; s1=sheet(data,strings); s2=sheet(summary,strings)
    ns='xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
    with zipfile.ZipFile(path,"w",zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml",'<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"/>')
        z.writestr("xl/workbook.xml",f'<?xml version="1.0" encoding="UTF-8"?><workbook {ns}><bookViews><workbookView/></bookViews><sheets><sheet name="요약 Summary" sheetId="2" r:id="rId7"/><sheet name="Data" sheetId="1" r:id="rId3"/></sheets></workbook>')
        z.writestr("xl/_rels/workbook.xml.rels",'<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Target="/xl/worksheets/sheet2.xml" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Id="rId7"/></Relationships>')
        z.writestr("xl/sharedStrings.xml",'<?xml version="1.0" encoding="UTF-8"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'+"".join(f"<si><t>{t}</t></si>" for t in strings)+"</sst>")
        z.writestr("xl/worksheets/sheet1.xml",s1); z.writestr("xl/worksheets/sheet2.xml",s2)
book(ex/"book1.xlsx",[["항목","합계"],["전체",60]],[["id","name"],[1,"apple"],[2,"banana"]])
book(ex/"book2.xlsx",[["항목","합계"],["전체",60]],[["id","name"],[1,"apple"],[2,"cherry"]])
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
    # a sheet of a workbook: by name, by position among the tabs, and none (the first worksheet part)
    ("xlsx sheet by name, same",["table","book1.xlsx#요약 Summary","book2.xlsx#요약 Summary","/exit"],0),
    ("xlsx sheet by name, different",["table","book1.xlsx#data","book2.xlsx#Data","/exit"],1),
    ("xlsx sheet by position 1",["table","book1.xlsx#1","book2.xlsx#1","/exit"],0),
    ("xlsx sheet by position 2",["table","book1.xlsx#2","book2.xlsx#2","/exit"],1),
    ("xlsx without a sheet",["table","book1.xlsx","book2.xlsx","/exit"],1),
    ("xlsx sheet that does not exist",["table","book1.xlsx#없음","book2.xlsx#없음","/exit"],2),
    ("xlsx sheet position past the end",["table","book1.xlsx#3","book2.xlsx#3","/exit"],2),
    ("exit text with 800 changed rows",["text","many8000a.txt","many8000b.txt","/exit"],1),
    ("xlsx text with 2,000 changed rows",["text","many20000a.txt","many20000b.txt","/xlsx","many.xlsx"],1),
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
many_x=part(ex/"many.xlsx","xl/worksheets/sheet1.xml")
xml.dom.minidom.parseString(many_x)
kinds=re.findall(r'<c r="A\d+"[^>]*><is><t[^>]*>(.*?)</t>',many_x)
assert (len(kinds),kinds.count("변경"),kinds.count("같음"))==(20001,2000,18000), (len(kinds),kinds.count("변경"),kinds.count("같음"))
assert not any((ex/f).exists() for f in ["w1.xlsx","w2.docx","w3.xlsx","w4.xlsx","w5.xlsx","w6.html"])
# a compare that could not read its inputs leaves no report of an empty result
assert not any((ex/f).exists() for f in ["e4.docx","e7.docx"])
print("PASS export file contents")
(fixtures/"cli-results.json").write_text(json.dumps(LOG,indent=2),encoding="utf-8")
print("All regression checks passed: 44 save + 19 state/worker +",ui_checks,"UI +",len(LOG),"CLI cases + 1 temporary-folder check")
