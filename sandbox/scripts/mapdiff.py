# compares two MSVC linker map files (reference build and self-built cppparse): per cppparse object, the
# code bytes and the public symbols, with the symbols that only one side has; sizes are derived from the
# symbol addresses in the "Publics by Value" section (the distance to the next public in the same section).
# Objects that come from libraries ("libucrt:memset.obj") are left out.
import io, re, sys

def parse(path, suffix):
    syms = []   # (section, address, name, unit)
    inPublics = False
    for line in io.open(path, encoding='latin-1'):
        if 'Publics by Value' in line: inPublics = True; continue
        if inPublics and line.startswith(' entry point'): break
        if not inPublics: continue
        m = re.match(r'\s*(\d{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)', line)
        if not m: continue
        obj = m.group(5)
        unit = None if ':' in obj else (obj[:-len(suffix)] if obj.endswith(suffix) else obj)
        syms.append((int(m.group(1)), int(m.group(2), 16), m.group(3), unit))
    syms.sort()
    sizes = {}
    for i, (sec, addr, name, unit) in enumerate(syms):
        nxt = syms[i + 1] if i + 1 < len(syms) else None
        size = nxt[1] - addr if nxt and nxt[0] == sec else 0
        sizes[name] = (size, unit, sec)
    return sizes

ref = parse(sys.argv[1], '.obj')
ours = parse(sys.argv[2], '6.obj')
perUnit = {}
for name, (size, unit, sec) in ref.items():
    if sec != 1 or unit is None: continue
    u = perUnit.setdefault(unit, [0, 0, 0, 0]); u[0] += size; u[1] += 1
for name, (size, unit, sec) in ours.items():
    if sec != 1 or unit is None: continue
    u = perUnit.setdefault(unit, [0, 0, 0, 0]); u[2] += size; u[3] += 1
print('%-16s %10s %6s %10s %6s %6s' % ('unit (code)', 'ref bytes', 'syms', 'our bytes', 'syms', 'ratio'))
rows = []
for u, (rb, rn, ob, on) in perUnit.items():
    rows.append(((ob * 100 // rb) if rb else 0, u, rb, rn, ob, on))
for ratio, u, rb, rn, ob, on in sorted(rows):
    print('%-16s %10d %6d %10d %6d %5d%%' % (u, rb, rn, ob, on, ratio))
refNames = set(n for n, (s, u, sec) in ref.items() if u is not None)
ourNames = set(n for n, (s, u, sec) in ours.items() if u is not None)
onlyRef = sorted(refNames - ourNames)
onlyOurs = sorted(ourNames - refNames)
print('\ncppparse symbols only in the reference: %d (first 12)' % len(onlyRef))
for n in onlyRef[:12]: print('  ' + n)
print('cppparse symbols only in ours: %d (first 12)' % len(onlyOurs))
for n in onlyOurs[:12]: print('  ' + n)
