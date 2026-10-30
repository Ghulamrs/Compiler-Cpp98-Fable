# folder comparison of out\maps\ref and out\maps\ours (written by mapsplit.py): per unit the symbols
# on both sides, on one side only, and the code bytes; and for each unit a side-by-side file
# out\maps\compare\<unit>.txt aligned by decorated name (left: reference, right: cppparse)
import io, os, re, sys

def load(path):
    rows = {}
    order = []
    sec = None
    for line in io.open(path, encoding='latin-1'):
        m = re.match(r'section (\d+):', line)
        if m: sec = int(m.group(1)); continue
        m = re.match(r'\s+\+([0-9a-f]+)\s+(\d+)\s+(\S+)', line)
        if m:
            rows[m.group(3)] = (sec, int(m.group(1), 16), int(m.group(2)))
            order.append(m.group(3))
    return rows, order

base = sys.argv[1]
left, right, out = os.path.join(base, 'ref'), os.path.join(base, 'ours'), os.path.join(base, 'compare')
if not os.path.isdir(out): os.makedirs(out)
names = sorted(set(os.listdir(left)) | set(os.listdir(right)))
print('%-16s %7s %7s %7s %7s %7s %10s %10s' % ('unit', 'ref', 'ours', 'both', 'refonly', 'oursonly', 'ref code', 'ours code'))
for fn in names:
    l = load(os.path.join(left, fn)) if os.path.exists(os.path.join(left, fn)) else ({}, [])
    r = load(os.path.join(right, fn)) if os.path.exists(os.path.join(right, fn)) else ({}, [])
    both = [n for n in l[1] if n in r[0]]
    lonly = [n for n in l[1] if n not in r[0]]
    ronly = [n for n in r[1] if n not in l[0]]
    lcode = sum(v[2] for v in l[0].values() if v[0] == 1)
    rcode = sum(v[2] for v in r[0].values() if v[0] == 1)
    print('%-16s %7d %7d %7d %7d %7d %10d %10d' % (fn[:-4], len(l[1]), len(r[1]), len(both), len(lonly), len(ronly), lcode, rcode))
    with io.open(os.path.join(out, fn), 'w', encoding='latin-1', newline='\n') as f:
        f.write(u'%-16s | %-16s | symbol\n' % ('ref +addr size', 'ours +addr size'))
        f.write(u'%s\n' % ('-' * 100))
        for n in both:
            a, b = l[0][n], r[0][n]
            mark = u' ' if a[2] == b[2] else u'~'
            f.write(u'+%06x %7d  | +%06x %7d %s| %s\n' % (a[1], a[2], b[1], b[2], mark, n))
        for n in lonly:
            a = l[0][n]
            f.write(u'+%06x %7d  | %-16s <| %s\n' % (a[1], a[2], '', n))
        for n in ronly:
            b = r[0][n]
            f.write(u'%-16s | +%06x %7d >| %s\n' % ('', b[1], b[2], n))
print('side-by-side files written to', out)
