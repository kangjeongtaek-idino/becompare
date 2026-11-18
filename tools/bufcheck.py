# bufcheck.py - static check: a stack buffer handed to a length-limited call must fit in the frame of its function.
#   Run from the repository root: python tools/bufcheck.py   (prints 'overflows: 0' when clean)
#   PROLOG n: locals are [rsp+32 .. rsp+n+8). A call "invoke f, &rsp+OFF, COUNT" writes COUNT wide chars (+ NUL).
import glob, re, sys

ROOT = 'gui/'
# function: (index of buffer arg, index of count arg, bytes per unit, extra units for a terminator)
API = {
    '_snwprintf': (0, 1, 2, 0),        # writes at most count chars (no NUL when full)
    'bc_snwprintf': (0, 1, 2, 0),
    'wcsncpy': (0, 2, 2, 0),
    'GetWindowTextW': (1, 2, 2, 0),    # count includes the NUL
    'GetDlgItemTextW': (2, 3, 2, 0),
    'GetTempPathW': (1, 0, 2, 0),
    'GetModuleFileNameW': (1, 2, 2, 0),
    'GetFullPathNameW': (2, 1, 2, 0),
    'GetClassNameW': (1, 2, 2, 0),
    'GetCurrentDirectoryW': (1, 0, 2, 0),
    'LoadStringW': (2, 3, 2, 0),
    'memset': (0, 2, 1, 0),
    'memcpy': (0, 2, 1, 0),
    'browse_folder': (1, None, 2, 520), 'browse_open': (1, None, 2, 520), 'browse_save': (1, None, 2, 520),
    'browse_save_as': (1, None, 2, 520), 'browse_open_as': (1, None, 2, 520), 'tc_tmp': (0, None, 2, 520),
    'input_box': (2, None, 2, 260),
    'ex_menu_choose': (3, None, 2, 520),   # hands the buffer to browse_save_as
}


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '[(':
            depth += 1
        elif ch in '])':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    out.append(cur.strip())
    return out


def strip_comment(line):
    q = None
    for i, ch in enumerate(line):
        if q:
            if ch == q:
                q = None
        elif ch in '"\'':
            q = ch
        elif ch == ';':
            return line[:i]
    return line


def load(path, seen, defs, lines):
    if path in seen:
        return
    seen.add(path)
    try:
        text = open(path, encoding='utf-8').read().splitlines()
    except OSError:
        return
    for no, raw in enumerate(text, 1):
        line = strip_comment(raw).strip()
        m = re.match(r'%include\s+"([^"]+)"', line)
        if m:
            load(ROOT + m.group(1), seen, defs, lines)
            continue
        m = re.match(r'%define\s+(\w+)\s+(.+)$', line)
        if m and '(' not in m.group(1):
            defs[m.group(1)] = m.group(2).strip()
        lines.append((path, no, line))


def ev(expr, defs, depth=0):
    if depth > 20:
        return None
    expr = expr.strip()
    toks = re.findall(r'0x[0-9A-Fa-f]+|\d+|\w+|[-+*()|<>]+', expr)
    out = ''
    for t in toks:
        if re.fullmatch(r'0x[0-9A-Fa-f]+|\d+', t):
            out += str(int(t, 0))
        elif re.fullmatch(r'\w+', t):
            if t not in defs:
                return None
            v = ev(defs[t], defs, depth + 1)
            if v is None:
                return None
            out += '(%d)' % v
        else:
            out += t
    try:
        return int(eval(out, {'__builtins__': {}}))
    except Exception:
        return None


bad = 0
for asm in sorted(glob.glob(ROOT + '*.asm')):
    defs, lines = {}, []
    load(asm, set(), defs, lines)
    frame, fname, macro = None, '?', False
    for path, no, line in lines:
        if line.startswith('%macro'):
            macro = True                 # the body belongs to whoever expands it
        elif line.startswith('%endmacro'):
            macro = False
        if macro:
            continue
        m = re.match(r'(\w+):\s*$', line)
        if m and not line.startswith('.'):
            fname = m.group(1)
        m = re.match(r'PROLOG\s+(.+)$', line)
        if m:
            frame = ev(m.group(1), defs)
            continue
        m = re.match(r'invoke\s+(\w+)\s*,\s*(.*)$', line)
        if not m or frame is None:
            continue
        fn, args = m.group(1), split_args(m.group(2))
        spec = API.get(fn)
        if not spec:
            continue
        bi, ci, unit, extra = spec
        if bi >= len(args) or (ci is not None and ci >= len(args)):
            continue
        b = args[bi]
        mb = re.match(r'&\s*rsp\s*\+\s*(.+)$', b)
        if not mb:
            continue
        off = ev(mb.group(1), defs)
        cnt = 0 if ci is None else ev(args[ci].lstrip('#'), defs)
        if off is None or cnt is None:
            continue
        end = off + (cnt + extra) * unit
        limit = frame + 8
        if end > limit:
            bad += 1
            print('%s:%d %s: %s buffer rsp+%d, %d units -> ends at %d, frame ends at %d (over by %d)' % (
                path.replace(chr(92), '/'), no, fname, fn, off, cnt, end, limit, end - limit))
print('overflows:', bad)
sys.exit(1 if bad else 0)
