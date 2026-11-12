"""Minimal passive-mode FTP server for testing (read-only): python mini_ftpd.py ROOT PORT"""
import os, socket, sys, threading, time

ROOT = os.path.abspath(sys.argv[1]); PORT = int(sys.argv[2])

def safe(cwd, arg):
    p = os.path.normpath(os.path.join(ROOT, (arg if arg.startswith('/') else cwd + '/' + arg).lstrip('/')))
    return p if p.startswith(ROOT) else ROOT

def vpath(p):
    r = os.path.relpath(p, ROOT).replace('\\', '/')
    return '/' if r == '.' else '/' + r

def listing(path):
    lines = []
    for n in sorted(os.listdir(path)):
        fp = os.path.join(path, n); st = os.stat(fp)
        t = time.strftime('%b %d %H:%M', time.gmtime(st.st_mtime))
        d = 'd' if os.path.isdir(fp) else '-'
        lines.append(f'{d}rwxr-xr-x 1 owner group {st.st_size if d == "-" else 0:>10} {t} {n}')
    return ('\r\n'.join(lines) + '\r\n').encode('utf-8')

def handle(c):
    cwd = '/'; pasv = None; c.sendall(b'220 mini ftpd\r\n'); f = c.makefile('rb')
    while True:
        line = f.readline()
        if not line: break
        s = line.decode('utf-8', 'replace').strip(); cmd, _, arg = s.partition(' '); cmd = cmd.upper()
        if cmd == 'USER': c.sendall(b'331 need password\r\n')
        elif cmd == 'PASS': c.sendall(b'230 logged in\r\n')
        elif cmd == 'SYST': c.sendall(b'215 UNIX Type: L8\r\n')
        elif cmd == 'PWD': c.sendall(f'257 "{cwd}"\r\n'.encode())
        elif cmd == 'TYPE': c.sendall(b'200 ok\r\n')
        elif cmd == 'CWD':
            p = safe(cwd, arg)
            if os.path.isdir(p): cwd = vpath(p); c.sendall(b'250 ok\r\n')
            else: c.sendall(b'550 no such dir\r\n')
        elif cmd in ('PASV', 'EPSV'):
            pasv = socket.socket(); pasv.bind(('127.0.0.1', 0)); pasv.listen(1); port = pasv.getsockname()[1]
            if cmd == 'PASV': c.sendall(f'227 Entering Passive Mode (127,0,0,1,{port >> 8},{port & 255})\r\n'.encode())
            else: c.sendall(f'229 Entering Extended Passive Mode (|||{port}|)\r\n'.encode())
        elif cmd in ('LIST', 'NLST', 'RETR', 'SIZE', 'MDTM'):
            a = arg
            while a.startswith('-'): a = a.partition(' ')[2]
            p = safe(cwd, a) if a else safe(cwd, '.')
            if cmd == 'SIZE':
                c.sendall(f'213 {os.path.getsize(p)}\r\n'.encode() if os.path.isfile(p) else b'550 no\r\n'); continue
            if cmd == 'MDTM':
                c.sendall(time.strftime('213 %Y%m%d%H%M%S\r\n', time.gmtime(os.path.getmtime(p))).encode() if os.path.exists(p) else b'550 no\r\n'); continue
            if pasv is None: c.sendall(b'425 use PASV\r\n'); continue
            if cmd == 'RETR':
                if not os.path.isfile(p): c.sendall(b'550 no file\r\n'); continue
                data = open(p, 'rb').read()
            else:
                if '*' in a or '?' in a:
                    d = os.path.dirname(p); import fnmatch
                    names = [n for n in sorted(os.listdir(d)) if fnmatch.fnmatch(n, os.path.basename(p))]
                    data = b''.join(listing(d).splitlines(True)[i] for i, n in enumerate(sorted(os.listdir(d))) if n in names)
                elif os.path.isdir(p): data = listing(p)
                else: data = listing(os.path.dirname(p))
            c.sendall(b'150 opening\r\n'); dc, _ = pasv.accept(); dc.sendall(data); dc.close(); pasv.close(); pasv = None
            c.sendall(b'226 done\r\n')
        elif cmd == 'QUIT': c.sendall(b'221 bye\r\n'); break
        else: c.sendall(b'502 not implemented\r\n')
    c.close()

srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(('127.0.0.1', PORT)); srv.listen(5)
print('listening', PORT, ROOT, flush=True)
while True:
    conn, _ = srv.accept(); threading.Thread(target=handle, args=(conn,), daemon=True).start()
