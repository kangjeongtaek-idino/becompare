"""Adversarial archives, extraction failure injection and a shared-parent stress case."""
from pathlib import Path
import io, json, os, struct, subprocess, tempfile, time, zipfile

ROOT = Path(__file__).resolve().parents[1]

def archive_checks(exe, directory, env=None, baseline=False):
    directory.mkdir(parents=True, exist_ok=True)
    checks = []
    def check(name, data, accepted, *, extract=False, fault=0, expected=b"content", mode="zip"):
        source = directory / f"case{len(checks)}.{'bcss' if mode == 'snapshot' else 'zip'}"
        source.write_bytes(data)
        dest = directory / "extracted.txt"
        dest.write_bytes(b"ORIGINAL")
        args = [str(exe), mode, str(source)]
        if extract: args += [str(dest), str(fault)]
        start = time.perf_counter()
        r = subprocess.run(args, cwd=directory, env=env, capture_output=True, text=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        ok = r.returncode == (0 if accepted else 2)
        if extract: ok &= dest.read_bytes() == (expected if accepted else b"ORIGINAL")
        checks.append(dict(name=name, passed=ok, output=r.stdout.strip(), seconds=time.perf_counter()-start))
        print(f"{'PASS' if ok else 'FAIL'} {name}: {r.stdout.strip()}", flush=True)
        if not baseline: assert ok, (name, r.returncode, r.stdout, r.stderr)
        return r.stdout
    def make(name="file.txt", data=b"content", method=zipfile.ZIP_STORED, comment=b""):
        b = io.BytesIO()
        with zipfile.ZipFile(b,"w",compression=method) as z:
            z.writestr(name,data); z.comment=comment
        return b.getvalue()
    def patch(data, offset, fmt, value):
        b=bytearray(data); struct.pack_into(fmt,b,offset,value); return bytes(b)
    normal=make(); cd=normal.index(b"PK\x01\x02"); end=normal.index(b"PK\x05\x06")
    for method in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
        for content in (b"", b"content", b"hello"*20000):
            check(f"valid method {method}, {len(content)} bytes",make(data=content,method=method),True,extract=True,expected=content)
    for path in ("sub/f.txt", ".git/config", "한글/자료.txt", "a\\b.txt", "COM10.txt", "console.txt"):
        check(f"valid name {path}",make(path),True,extract=True)
    b=io.BytesIO()
    with zipfile.ZipFile(b,"w"): pass
    check("empty ZIP",b.getvalue(),True)
    check("signature inside ZIP comment",make(comment=b"PK\x05\x06"+bytes(24)),True,extract=True)
    # A nonseekable output makes Python emit the optional data descriptor.
    class Stream(io.BytesIO):
        def seekable(self): return False
        def seek(self,*args): raise OSError("stream")
    stream=Stream()
    with zipfile.ZipFile(stream,"w",compression=zipfile.ZIP_DEFLATED) as z: z.writestr("f.txt",b"content")
    check("data descriptor",stream.getvalue(),True,extract=True)
    for path in ("../outside.txt","a/../../outside.txt","..\\outside.txt","/absolute.txt","C:/drive.txt","a:stream", "a//b", "a/./b", "a/.. /b", "a./b", "a /b", "a?b", "a*b", "a\x01b", "CON", "nul.txt", "a/AUX.log", "com1.txt", "LPT9", "COM¹.txt", "a"*256, "/".join(["part"]*110)):
        check(f"unsafe name {path!r}",make(path),False)
    check("embedded NUL in ZIP filename",normal.replace(b"file.txt",b"fi\0e.txt"),False)
    check("invalid UTF-8 filename",patch(normal.replace(b"file.txt",b"\xffile.txt"),cd+8,"<H",0x800),False)
    for name,offset,fmt,value in (
        ("central offset outside archive",end+16,"<I",0xffffffff),
        ("central size outside archive",end+12,"<I",0xffffffff),
        ("central directory count mismatch",end+8,"<I",0x00020002),
        ("multi-disk archive",end+4,"<H",1),
        ("record exceeds central directory",cd+28,"<H",65535),
        ("entry on another disk",cd+34,"<H",1),
        ("encrypted entry",cd+8,"<H",1),
        ("unsupported compression",cd+10,"<H",99),
        ("entry over extraction memory limit",cd+24,"<I",0x10000001),
        ("stored lengths differ",cd+24,"<I",8),
    ): check(name,patch(normal,offset,fmt,value),False)
    check("bad central signature",patch(normal,cd,"<I",0),False)
    check("truncated end record",normal[:-1],False)
    for name,offset,fmt,value in (
        ("corrupt payload CRC",30+len("file.txt"),"<B",0),
        ("wrong recorded CRC",cd+16,"<I",0),
        ("local header outside archive",cd+42,"<I",0xfffffffe),
        ("local header points at central record",cd+42,"<I",cd),
        ("local header compression mismatch",8,"<H",8),
        ("local encryption flag",6,"<H",1),
        ("local name length overflow",26,"<H",65535),
    ): check(name,patch(normal,offset,fmt,value),False,extract=True)
    compressed=make(method=zipfile.ZIP_DEFLATED); pos=compressed.index(b"PK\x01\x02")
    check("deflate output shorter than claimed",patch(compressed,pos+24,"<I",8),False,extract=True)
    for fault in (1,2,4): check(f"extraction I/O fault {fault} preserves target",normal,False,extract=True,fault=fault)
    check("short write preserves the existing target",normal,False,extract=True,fault=3)
    check("valid snapshot",b"BCSS1\r\nsub/file.txt\t0\t7\t0\t1234\r\n",True,mode="snapshot")
    for path in ("../outside", "a:stream", "NUL.txt", "a"*512):
        check(f"unsafe snapshot {path[:30]}",f"BCSS1\r\n{path}\t0\t7\t0\t1234\r\n".encode(),False,mode="snapshot")
    b=io.BytesIO()
    with zipfile.ZipFile(b,"w") as z:
        for i in range(5000): z.writestr("shared/"+"nested/"*24+f"f{i:05}.txt",b"")
    output=check("5000 files with 25 common parents",b.getvalue(),True)
    import re
    capacity=int(re.search(r"capacity=(\d+)",output)[1])
    efficient=capacity<=8192
    checks.append(dict(name="parent table memory stays proportional to unique paths",passed=efficient,capacity=capacity))
    print(f"{'PASS' if efficient else 'FAIL'} unique parent capacity: {capacity}",flush=True)
    if not baseline: assert efficient,capacity
    (directory/"results.json").write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding="utf-8")
    print(f"Archive checks: {len(checks)}, failures: {sum(not c['passed'] for c in checks)}",flush=True)
    return len(checks)

if __name__ == "__main__":
    import sys
    out=ROOT/"build"/"archive-checks"; out.mkdir(parents=True,exist_ok=True)
    objects=[]
    for mod in ("engine","textlib","mem","vtab","inflate"):
        obj=out/(mod+".obj");objects.append(obj)
        subprocess.run(["nasm","-f","win64","-I","./",mod+".asm","-o",str(obj)],cwd=ROOT/"gui",check=True)
    obj=out/"concurrency.obj";objects.append(obj)
    subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-c",str(ROOT/"gui/concurrency.c"),"-o",str(obj)],check=True)
    exe=out/"test_archives.exe"
    subprocess.run(["gcc","-municode",str(ROOT/"tools/test_archives.c"),*map(str,objects),"-Wl,--wrap=WriteFile","-Wl,--wrap=FlushFileBuffers","-luser32","-lshell32","-o",str(exe)],check=True)
    archive_checks(exe,Path(tempfile.mkdtemp(prefix="baseline-" if "--baseline" in sys.argv else "fixed-",dir=out)),baseline="--baseline" in sys.argv)
