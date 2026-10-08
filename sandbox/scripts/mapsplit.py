# splits two MSVC linker map files (reference build and self-built cppparse) into one text file per
# source unit on each side, out\maps\ref\<unit>.txt and out\maps\ours\<unit>.txt, so that the two
# directories can be compared file to file. Every line is one public symbol with its address relative
# to the unit's first symbol in that section ("incremental address"), its size (distance to the next
# public in the same section, whatever object owns it) and its decorated name, which is identical on
# both sides when the mangling agrees; absolute addresses are left out because each link lays the
# executable out differently.
import io, os, re, sys

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
        # objects from libraries ("libucrt:memset.obj") and linker-defined symbols ("<absolute>") are left out
        unit = None if (':' in obj or not obj.endswith(suffix)) else obj[:-len(suffix)]
        syms.append((int(m.group(1)), int(m.group(2), 16), m.group(3), unit))
    syms.sort()
    out = {}    # unit -> [(section, address, size, name)]
    for i, (sec, addr, name, unit) in enumerate(syms):
        nxt = syms[i + 1] if i + 1 < len(syms) else None
        size = nxt[1] - addr if nxt and nxt[0] == sec else 0
        if unit is not None: out.setdefault(unit, []).append((sec, addr, size, name))
    return out

def write(units, directory):
    if not os.path.isdir(directory): os.makedirs(directory)
    for unit, rows in units.items():
        with io.open(os.path.join(directory, unit + '.txt'), 'w', encoding='latin-1', newline='\n') as f:
            lastSec = None
            base = 0
            for sec, addr, size, name in rows:
                if sec != lastSec:
                    lastSec = sec
                    base = addr
                    total = sum(s for sc, a, s, n in rows if sc == sec)
                    f.write(u'section %04d: %d symbols, %d bytes\n' % (sec, len([1 for sc, a, s, n in rows if sc == sec]), total))
                f.write(u'  +%06x %6d  %s\n' % (addr - base, size, name))

ref = parse(sys.argv[1], '.obj')
ours = parse(sys.argv[2], '6.obj')
outDir = sys.argv[3]
write(ref, os.path.join(outDir, 'ref'))
write(ours, os.path.join(outDir, 'ours'))
print('per-unit map files: %d in %s, %d in %s' % (len(ref), os.path.join(outDir, 'ref'), len(ours), os.path.join(outDir, 'ours')))
