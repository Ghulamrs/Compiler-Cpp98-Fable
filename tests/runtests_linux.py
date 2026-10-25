#!/usr/bin/env python3
# Linux port of the portable parts of sandbox/build-cppparse.ps1.
# Checks that need cl / ml64 / link / dumpbin are reported as SKIP.
import os, re, subprocess, sys, glob

EXE = os.path.abspath(sys.argv[1])
CPP = os.path.abspath(sys.argv[2])
SB = os.path.join(CPP, "sandbox")
os.chdir(SB)
DIRS = ["driver", "common", "frontend", "middle", "backend", "analyzer", "analyzer/report"]
results = {}  # test -> [ok, fail, skip]
cur = None
for _d in ("out/sample", "out/folder", "out/files", "out/bad", "out/opt", "out/par", "out/x64"): os.makedirs(_d, exist_ok=True)

def test(name):
    global cur
    cur = name; results[name] = [0, 0, 0]
    print("=== %s ===" % name)

def ok(c, msg):
    if c: results[cur][0] += 1; print("  ok   " + msg)
    else: results[cur][1] += 1; print("  FAIL " + msg)
    return c

def skip(msg):
    results[cur][2] += 1; print("  SKIP " + msg)

def run(*args, **kw):
    p = subprocess.run([EXE] + list(args), capture_output=True, text=True, errors="replace", **kw)
    return p.returncode, p.stdout + p.stderr

def rd(p):
    try:
        with open(p, encoding="latin-1") as f: return f.read()
    except IOError: return ""

def mk(d): os.makedirs(d, exist_ok=True)

def mapcheck(name, objname=None):
    # port of Test-ObjectMap; dumpbin /symbols is replaced by objdump -t
    path = "out/x64/%s.map" % name; probs = []
    if not os.path.exists(path): ok(False, "missing " + path); return
    lines = rd(path).splitlines()
    sections = {}
    for l in lines:
        m = re.match(r'^ ([0-9a-f]{4}):00000000 ([0-9a-f]{8})H  (\S+)', l)
        if m: sections[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3))
    syms = []; byname = []; inb = False
    for l in lines:
        if l.startswith(' Symbols by Name'): inb = True; continue
        if inb:
            m = re.match(r'^ (?P<sec>[0-9a-f]{4}):(?P<off>[0-9a-f]{8})   (?P<size>[0-9a-f]{8})H   (?P<flags>.{3})    (?P<type>\w+)\s+(?P<link>public|static)\s+(?P<sym>\S+)\s*(?P<name>.*)$', l)
            if m: byname.append((":".join([m['sec'], m['off'], m['size'], m['flags'], m['sym']]), m['name'] or m['sym']))
        else:
            m = re.match(r'^ (?P<sec>[0-9a-f]{4}):(?P<off>[0-9a-f]{8})   (?P<size>[0-9a-f]{8})H   (?P<flags>.{3})    (?P<type>\w+)\s+(?P<sym>\S+)', l)
            if m: syms.append(dict(sec=int(m['sec'], 16), off=int(m['off'], 16), size=int(m['size'], 16), flags=m['flags'], type=m['type'], sym=m['sym'],
                                   key=":".join([m['sec'], m['off'], m['size'], m['flags'], m['sym']])))
    if not syms: ok(False, path + ": no symbols"); return
    for x in syms:
        want = '   ' if x['type'] != 'function' else ('f  ' if x['sec'] in sections else 'f i')
        if x['flags'] != want: probs.append("%s flags '%s' expected '%s'" % (x['sym'], x['flags'], want)); break
    if sorted(x['key'] for x in syms) != sorted(k for k, _ in byname): probs.append("Symbols by Name differs from the address lists")
    for i in range(1, len(byname)):
        if byname[i - 1][1] > byname[i][1]: probs.append("Symbols by Name not in order at " + byname[i][1]); break
    for sec in set(x['sec'] for x in syms):
        if sec not in sections: continue
        lst = sorted([x for x in syms if x['sec'] == sec], key=lambda x: x['off'])
        for i, x in enumerate(lst):
            limit = lst[i + 1]['off'] if i + 1 < len(lst) else sections[sec][0]
            if x['off'] + x['size'] > limit: probs.append("%s ends beyond %d" % (x['sym'], limit))
        if sections[sec][1].startswith('.text'):
            tot = sum(x['size'] for x in lst)
            if tot != sections[sec][0]: probs.append("section %d: functions add up to %d, section has %d" % (sec, tot, sections[sec][0]))
    obj = "out/x64/%s.obj" % (objname or name)
    t = subprocess.run(["objdump", "-t", obj], capture_output=True, text=True).stdout
    real = {}
    for m in re.finditer(r'\(sec\s+(-?\d+)\)\(fl [^)]*\)\(ty\s+\w+\)\(scl\s+(\d+)\) \(nx \d+\) 0x([0-9a-f]+) (\S+)', t):
        if int(m.group(1)) > 0 and m.group(2) in ('2', '3'): real[m.group(4)] = (int(m.group(1)), int(m.group(3), 16))
    for x in syms:
        if x['sym'] not in real: probs.append(x['sym'] + " is not in the object"); continue
        if real[x['sym']] != (x['sec'], x['off']): probs.append("%s: map %d:%x, object %d:%x" % ((x['sym'], x['sec'], x['off']) + real[x['sym']]))
    ok(not probs, "%s.map: %d symbols; flags, by-name list, sizes fill sections, addresses match objdump -t" % (name, len(syms)))
    for p_ in probs[:8]: print("       " + p_)

# ---- 1
test("Test 1: sample/Sample.sln")
rc, out = run("-o", "out/sample", "../examples/sample/Sample.sln")
ok(rc == 0, "exit code 0 (%d)" % rc)
report = rd("out/sample/Sample-report.txt")
for e in ['Visual Studio 2010, ToolsVersion 4.0, toolset v100', 'class geom::Circle : public Shape, private Named',
          'class geom::Rect : public Shape, virtual public Named', 'template <typename T, int N = 16>',
          'class util::FixedStack<bool, 8>', 'class util::FixedStack<T*, 4>', 'class util::FixedStack::Iterator',
          'struct geom::Rect::Corner', 'include/Legacy.h   (included by main.cpp)', '1>src/main.cpp(9): error E100: ',
          'function-try-block', 'double sum = 0.0', 'const Shape* s = m_shapes[i]', '#if: defined(SAMPLE_WITH_3D)',
          'SAMPLE_MAX(a, b)', 'static int s_liveCount', 'defined src/Shape.cpp:', 'operator bool', 'operator<<',
          'virtual double area() const = 0', 'util::Matrix2::rows() const is declared but no definition was found',
          'void (*m_callback)(const Shape&)', 'extern int g_shapeVersion', 'typedef void (*ReportFn)(const Shape&)',
          'struct LegacyStatus', 'unsigned m_flags : 4', 'SAMPLE_API double geom::totalArea',
          'preprocessor definitions: WIN32; _DEBUG; _CONSOLE; SAMPLE_STATIC',
          'Files parsed: 6  (headers 4, sources 2, of which found only through #include: 1)']:
    ok(e in report, e)
for e in ['class geom::Obsolete', 'void broken']: ok(e not in report, "absent: " + e)
for f in ['Sample-report.html', 'Sample-report.pdf', 'Sample-diagram.svg', 'Sample-diagram.dot']:
    p = "out/sample/" + f; ok(os.path.exists(p) and os.path.getsize(p) > 500, f)
sym = rd("out/sample/Sample-symbols.json")
for e in ['"kind": "class", "name": "Circle", "qualified": "geom::Circle"', '"kind": "method", "name": "area"',
          '"kind": "local", "name": "sum"', '"kind": "macro", "name": "SAMPLE_MAX"', '"kind": "parameter", "name": "radius"']:
    ok(e in sym, "symbols: " + e)
smap = rd("out/sample/Sample-symbols.map")
for e in [' Symbols by module', ' Cross reference', 'geom::Circle::area', 'geom::Canvas::add  (', ' Entry point', 'main(int argc, char* argv[])']:
    ok(e in smap, "map: " + e)
pdf = rd("out/sample/Sample-report.pdf")
ok(pdf.startswith('%PDF-1.4') and '%%EOF' in pdf, "PDF structure")
svg = rd("out/sample/Sample-diagram.svg")
ok('class geom::Circle' in svg and '<polygon' in svg, "SVG contents")
pos = -1; ino = True
for s in ['1. PROJECT', '2. FILES', '3. CLASSES', '4. CLASS DIAGRAM', '5. GLOBAL DEFINITIONS', '6. EXCEPTION HANDLING', '7. HEALTH', '8. TOTALS']:
    at = report.find("\n" + s)
    if at <= pos: ino = False
    pos = at
ok(ino, "report sections 1 to 8 in order")
a = report.find("\n4. CLASS DIAGRAM"); tree = report[a:report.find("\n5. GLOBAL")]
for e in ['class geom::Shape  [abstract]', '+-- class geom::Circle  (private)  also derives from: Shape',
          '+-- class geom::Rect  (public virtual)  also derives from: Shape', '+-- class geom::Sphere  (public)',
          'struct geom::Rect::Corner  [nested in geom::Rect]', '+-- class util::Matrix2  (public)']:
    ok(e in tree, "tree: " + e)
html = rd("out/sample/Sample-report.html")
ok('<svg' in html and not re.search(r'(src|href)="(https?|file):', html), "HTML self-contained with the diagram")
pages = len(re.findall(r'/Type\s*/Page[^s]', pdf)); ok(pages >= 3, "PDF pages: %d" % pages)
dot = rd("out/sample/Sample-diagram.dot")
for e in ['"geom::Circle" -> "geom::Named" [arrowhead=empty, color="#b03030"]',
          '"geom::Rect" -> "geom::Named" [arrowhead=empty, color="#222222", label="virtual"]',
          '"geom::Canvas" -> "geom::Shape" [style=dashed, arrowhead=open, color="#1a6fb5"]',
          '"geom::Rect::Corner" -> "geom::Rect" [style=dotted, arrowhead=none, color="#888888", label="nested"]',
          'fillcolor="#fff6d5"']:
    ok(e in dot, "dot: " + e)
for e in [r'(?s)LegacyCounter::tick  \(1\)\s+src/main\.cpp:37\s+in main',
          r'(?s)\(anonymous\)::kPi  \(2\)\s+src/Shape\.cpp:74\s+in geom::Circle::area\s+src/Shape\.cpp:80\s+in geom::Circle::perimeter',
          ' Unreferenced symbols ']:
    ok(re.search(e, smap), "map: " + e)

# ---- 2
test("Test 2: folder input (cpp folder)")
rc, out = run("-o", "out/folder", "-n", "cpp-folder", "..")
ok(rc == 0, "exit code 0 (%d)" % rc)
r2 = rd("out/folder/cpp-folder-report.txt")
for e in ['class cppparse::Parser', 'class cppparse::CodeBase', 'class Vector', 'class Matrix', 'struct cppparse::ClassInfo', 'folder scan (recursive)']:
    ok(e in r2, e)
m2 = rd("out/folder/cpp-folder-symbols.map")
ok('mx::Matrix::operator()(int r, int c)' in m2 and 'operator()()(' not in m2, "map: operator()(int r, int c)")

# ---- 3
test("Test 3: plain files")
rc, out = run("-o", "out/files", "-n", "vector", "../examples/sample-vectrix/Vector.hpp", "../examples/sample-vectrix/VectorTest.cpp")
ok(rc == 0, "exit code 0")
ok('template <typename T, int N>' in rd("out/files/vector-report.txt"), "Vector template found")

# ---- 4
test("Test 4: health checker (sample-bad)")
mk("out/bad")
rc, out = run("-o", "out/bad", "-n", "bad", "--text-only", "../examples/sample-bad")
ok(rc == 0, "exit code 0 (%d)" % rc)
health = rd("out/bad/bad-health.txt")
codes = ['E100', 'E101', 'W102', 'I103', 'E110', 'E120', 'E121', 'W122', 'E123', 'E124', 'W125', 'W126', 'I127', 'W128',
         'E130', 'E131', 'W132', 'W133', 'E134', 'E135', 'E136', 'W137', 'W138', 'W140', 'W141',
         'W147', 'W150', 'W151', 'W152', 'W153', 'W154', 'W155', 'E156', 'W157', 'E160', 'W161', 'E162', 'E163', 'W164', 'W165',
         'T100', 'T101', 'T102', 'T104', 'T106', 'T107', 'T108', 'T109', 'T110', 'T111', 'T112', 'T115', 'T116', 'T117', 'T118', 'T119', 'T121', 'T122', 'T146']
miss = [c for c in codes if not re.search(r"(?m)^\d+>[^\r\n]*\(\d+\): (error|warning|info) %s: " % c, health)]
ok(not miss, "all %d health/type codes reported%s" % (len(codes), (" - missing " + " ".join(miss)) if miss else ""))
rc, out = run("--strict", "--health", "../examples/sample-bad")
ok(rc == 3, "--strict exit code 3 (%d)" % rc)
g = re.search(r"errors (\d+), warnings (\d+), info (\d+)", rd("out/sample/Sample-health.txt"))
ok(g and int(g.group(1)) <= 1, "good sample: errors %s, warnings %s, info %s" % (g.groups() if g else ("?",) * 3))
ok("goodCode" not in health, "goodCode has no findings")

# ---- 5
test("Test 5: version 3 optimiser and lowering")
mk("out/opt")
for lvl in range(4):
    rc, out = run("-o", "out/opt", "-n", "opt%d" % lvl, "-O%d" % lvl, "--text-only", "../examples/sample-opt")
    ok(rc == 0 and not re.search(r": (error|warning|info) T1\d\d: ", rd("out/opt/opt%d-health.txt" % lvl)), "-O%d runs, no type findings" % lvl)
ir = {l: rd("out/opt/opt%d-ir.txt" % l) for l in range(4)}
log = {l: rd("out/opt/opt%d-opt.log" % l) for l in range(4)}
def sec(t, n):
    i = t.find("function %s(" % n)
    if i < 0: return ""
    j = t.find("================", i); j = len(t) if j < 0 else j
    return t[i:j]
def after(s):
    i = s.find("--- after"); return s if i < 0 else s[i:]
pp = sec(ir[0], 'Point::Point')
for name, c in [
    ('constants folds to ret #14 at -O1', re.search('ret #14', after(sec(ir[1], 'constants')))),
    ('deadcode has no mul at -O1', not re.search('bin mul', after(sec(ir[1], 'deadcode')))),
    ('cse computes a*b once at -O1', len(re.findall('bin mul', after(sec(ir[1], 'cse')))) == 1),
    ('strength: mul 8 became shl 3 at -O1', re.search(r'shl t\d+, #3', after(sec(ir[1], 'strength')))),
    ('loop-invariant code motion ran at -O2', re.search('loop-invariant code motion: [1-9]', log[2])),
    ('square inlined into usePoint at -O2', not re.search('call square', after(sec(ir[2], 'usePoint')))),
    ('constructor initializer list stores x and y (-O0)', 'addrfield x' in pp and 'storemem' in pp),
    ('usePoint folds to ret #12 at -O3', 'ret #12' in after(sec(ir[3], 'usePoint'))),
    ('unroll folds to ret #6 at -O3', 'ret #6' in after(sec(ir[3], 'unroll'))),
    ('optimisation log reports instruction counts', re.search(r'result: \d+ -> \d+ instructions', log[1])),
    ('assembly has prologue and epilogue', re.search(r'ENTER #\d+', rd("out/opt/opt3-asm.txt")) and 'RET' in rd("out/opt/opt3-asm.txt")),
    ('assembly passes arguments in registers (-O0)', 'POP R0' in rd("out/opt/opt0-asm.txt") and 'CALL ' in rd("out/opt/opt0-asm.txt")),
    ('AST dump annotates types', re.search(r'binary .\*.\s+: int', rd("out/opt/opt0-ast.txt")))]:
    ok(c, name)
run("-o", "out/sample", "-O2", "../examples/sample/Sample.sln")
ok('vptr' in rd("out/sample/Sample-asm.txt"), "virtual call lowered through the vtable (Sample)")
ok('= phi ' in ir[2], "SSA form with phi nodes")
ok('function util::FixedStack<double, 32>::push' in rd("out/sample/Sample-ir.txt"), "FixedStack<double, 32>::push instantiated")
ok('function maxOf<int>' in ir[2] and 'Buffer<int, 4>::add' in ir[2], "maxOf<int>, Buffer<int, 4> instantiated")
ok(re.search('global value numbering|SSA construction', log[2]), "SSA passes logged")

# ---- 6
test("Test 6: version 4 x86-64 (ml64 + link + run)")
for l in (0, 2, 3): ok(os.path.exists("out/opt/opt%d.asm" % l) and os.path.getsize("out/opt/opt%d.asm" % l) > 1000, "opt%d.asm written" % l)
skip("ml64 / cl / link / run of driver.exe (Windows-only)")

# ---- 7
test("Test 7: parallel pipeline")
mk("out/par")
rsp = ["# response file written by the test"]
for d in DIRS:
    for x in ("cpp", "hpp"):
        if glob.glob("../cppparse/%s/*.%s" % (d, x)): rsp.append("../cppparse/%s/*.%s" % (d, x))
open("out/par/list.rsp", "w").write("\n".join(rsp + ["-O3", "-q"]) + "\n")
rc1, _ = run("-j", "1", "-o", "out/par", "-n", "j1", "@out/par/list.rsp"); ok(rc1 == 0, "-j 1 run (%d)" % rc1)
rc2, _ = run("-j", "12", "-o", "out/par", "-n", "j12", "@out/par/list.rsp"); ok(rc2 == 0, "-j 12 run (%d)" % rc2)
m = re.search(r'Files parsed: (\d+)', rd("out/par/j1-report.txt"))
ok(m and int(m.group(1)) >= 40, "@response file with wildcards: %s files" % (m.group(1) if m else "?"))
for s in ["-ir.txt", "-asm.txt", ".asm", "-health.txt", "-ast.txt"]:
    a = re.sub(r'\d+ thread', 'T thread', re.sub(r'j1([-.])', r'X\1', rd("out/par/j1" + s)))
    b = re.sub(r'\d+ thread', 'T thread', re.sub(r'j12([-.])', r'X\1', rd("out/par/j12" + s)))
    ok(a == b and a, s + " identical with -j 1 and -j 12")
ok(re.search(r'phase times \(ms, \d+ thread', rd("out/par/j12-opt.log")), "phase times logged (note: no worker pool off Windows)")
rc, w = run("-j", "4", "--no-compile", "-o", "out/par", "-n", "wild", "../examples/sample-opt/*.cpp", "../examples/sample-vectrix/Vect*.hpp")
ok(rc == 0 and re.search(r'2 files \(1 headers, 1 sources\)', w), "wildcards on the command line")
rc, _ = run("-q", "--no-compile", "-o", "out/par", "-n", "none", "../cppparse/nothing*.cpp")
ok(rc != 0, "unmatched pattern is an error")

# ---- 8
test("Test 8: COFF writer")
for l in (0, 3):
    rc, out = run("-O%d" % l, "--obj", "-q", "-o", "out/x64", "-n", "own%d" % l, "../examples/sample-opt")
    ok(rc == 0 and os.path.exists("out/x64/own%d.obj" % l), "own%d.obj written" % l)
    p = subprocess.run(["objdump", "-f", "out/x64/own%d.obj" % l], capture_output=True, text=True)
    ok("pe-x86-64" in p.stdout or "x86-64" in p.stdout, "objdump reads own%d.obj as %s" % (l, (re.search(r"file format (\S+)", p.stdout) or [None, "?"])[1]))
for l in (0, 3): mapcheck("own%d" % l) if os.path.exists("out/x64/own%d.obj" % l) else ok(False, "own%d.map" % l)
skip("link with driver + run, dumpbin comparison against ml64 (Windows-only)")

# ---- 9
test("Test 9: MSVC mangling")
rc, out = run("-O2", "--obj", "--msvc-mangling", "-q", "-o", "out/x64", "-n", "msvc", "../examples/sample-opt")
ok(rc == 0 and os.path.exists("out/x64/msvc.obj"), "msvc.obj written")
p = subprocess.run(["objdump", "-t", "out/x64/msvc.obj"], capture_output=True, text=True)
n = len(re.findall(r" \?\w", p.stdout)); ok(n > 10, "%d decorated '?' symbols in the table" % n)
skip("comparison with cl's names and native driver run (Windows-only)")

# ---- 10
test("Test 10: preprocessor (-E)")
pp = subprocess.run([EXE, "-E", "-I", "inc", "pp.cpp"], cwd="../examples/sample-pp", capture_output=True, text=True, errors="replace")
ok(pp.returncode == 0, "-E exit 0 (%d)" % pp.returncode)
ok('error' not in pp.stderr, "no preprocessor errors" + ("" if 'error' not in pp.stderr else ": " + pp.stderr[:300]))
# compare with gcc -E (closest stand-in for cl /EP); report agreement only
g = subprocess.run(["g++", "-E", "-P", "-x", "c++", "-std=c++98", "-U__GNUC__", "-D_WIN32=1", "-D_WIN64=1", "-D_M_X64=100", "-D__cppparse__=6", "-Iinc", "pp.cpp"],
                   cwd="../examples/sample-pp", capture_output=True, text=True)
norm = lambda s: [re.sub(r'\s', '', x) for x in s.splitlines() if re.sub(r'\s', '', x) and not x.lstrip().startswith('#')]
A, B = norm(g.stdout), norm(pp.stdout)
same = sum(1 for i in range(min(len(A), len(B))) if A[i] == B[i])
print("  info gcc -E vs cppparse -E: %d of %d/%d lines equal (gcc is not the reference; cl /EP is)" % (same, len(A), len(B)))
open("out/par/bad.cpp", "w").write('#include "nothere.h"\n#if 1\n#error stop here\n#endif\n')
p = subprocess.run([EXE, "-E", os.path.abspath("out/par/bad.cpp")], capture_output=True, text=True)
ok(p.returncode != 0 and 'error: #error stop here' in p.stderr and 'include file not found: "nothere.h"' in p.stderr, "#error and missing include diagnosed")
skip("byte-for-byte comparison with cl /EP (Windows-only)")

# ---- 11
test("Test 11: parser (--parse6)")
rc, p6 = run("--parse6", "-o", "out/par", "-n", "opt6", "../examples/sample-opt/opt.cpp")
ok(' 0 syntax error(s)' in p6 and 'classes 4, enums 0, functions defined 26' in p6 and 'templates 2' in p6, "sample-opt parses (4 classes, 26 functions, 2 templates)")
ast = rd("out/par/opt6-ast6.txt")
ok(all(re.search(x, ast) for x in [r'function int square\(int x\)', 'struct BadIndex : public Failure', 'template <typename T, int N>', r'catch \(decl\)', r'return x \* x']), "AST dump contents")
rc, p6 = run("--parse6", "-I", "../examples/sample/include", "../examples/sample/src/main.cpp", "../examples/sample/src/Shape.cpp")
ok(len(re.findall(' 0 syntax error', p6)) == 2, "VS2010 sample parses without errors")
inc = []; lst = []
for d in DIRS: inc += ["-I", "../cppparse/" + d]; lst += sorted(glob.glob("../cppparse/%s/*.cpp" % d))
rc, own = run("--parse6", *(inc + lst))
errs = re.findall(r'.*\): error.*', own); nf = len(re.findall(' 0 syntax error', own))
ok(not errs and nf >= 30, "own sources: %d files parse, %d errors" % (nf, len(errs)))
for e in errs[:10]: print("       " + e.strip())
rc, bad = run("--parse6", *sorted(glob.glob("../examples/sample-bad/*.cpp")))
ok(rc != 0 and 'syntax error' in bad and 'functions defined' in bad, "broken sample: errors reported, parsing continued")

# ---- 12
test("Test 12: semantic analysis (--sema6)")
rc, s6 = run("--sema6", "-o", "out/par", "-n", "opt6", "../examples/sample-opt/opt.cpp")
ok('sema: 0 error(s)' in s6 and 'functions analysed 26' in s6 and 'unresolved 0' in s6 and 'instantiations 1 classes / 1 functions' in s6, "sample-opt sema")
sy = rd("out/par/opt6-sym6.txt")
miss = [c for c in ['struct Point  size 8 align 4', 'int y  @4', r'void\(int, int\) Point  \{body\}', r'int\(\) const sum', 'instantiation Buffer<int, 4>  size 20',
                    r'int\[4\] items  @0', r'void\(const int&\) add', r'instantiation maxOf<int> int\(const int&, const int&\)', 'struct BadIndex : Failure@0  size 8', 'int index  @4']
        if not re.search(c, sy)]
ok(not miss, "symbol dump" + (" misses " + " | ".join(miss) if miss else ""))
rc, s6 = run("--sema6", "-I", "../examples/sample/include", "../examples/sample/src/Shape.cpp")
ok(re.search(r'sema: \d+ error', s6) and re.search(r'calls resolved \d+', s6) and not re.search('ambiguous [1-9]', s6), "VS2010 sample analyses")
bad = [l for l in s6.splitlines() if re.search(r'\): error S', l) and not re.search(r"std::|'size_t'|iterator", l)]
ok(not bad, "no semantic errors outside std:: uses")
for l in bad[:10]: print("       " + l.strip())

# ---- 13-16: version 6 compile; only what can be checked without cl/link
def c6(name, src, lvl, extra=()):
    rc, out = run("--compile6", "-O%d" % lvl, "--obj", "--msvc-mangling", *(list(extra) + ["-o", "out/x64", "-n", name, src]))
    good = rc == 0 and os.path.exists("out/x64/%s6.obj" % name) and 'sema: 0 error' in out
    ok(good, "%s -O%d -> %s6.obj, sema 0 errors (exit %d)" % (src.split('/')[-1], lvl, name, rc))
    if not good: print("\n".join("       " + l for l in out.splitlines()[-8:]))
    return out

test("Test 13: version 6 compile")
for l in (0, 2): c6("v6o%d" % l, "../examples/sample-opt/opt.cpp", l); mapcheck("v6o%d6" % l)
run("--compile6", "-O2", "--obj", "--msvc-mangling", "-q", "-I", "../include", "-I", "..", "-I", "../examples/sample-matrix", "-o", "out/x64", "-n", "mtx", "../examples/sample-matrix/matrix.cpp")
mtx = rd("out/x64/mtx6.map")
ok(re.search(r"vtable\s+\S+\s+\S+::`vftable'", mtx) and not re.search(r"(?m)data\s+\S+\s+\S+::`vftable'", mtx), "mtx6.map: vtables typed as vtable")
ok(re.search(r'(?m)^ [0-9a-f]{4}-[0-9a-f]{4}\s+[0-9a-f]{8}H  \.text\$mn\s+CODE  COMDAT, \d+ section', mtx), "mtx6.map: COMDAT sections grouped")
mapcheck("mtx6")
for l in (0, 2):
    c6("align%d" % l, "../examples/sample-align/align.cpp", l, ["-I", "../include"])
    mapcheck("align%d6" % l)
    ok(re.search(r'(?m)^ 0002:[0-9a-f]{6}[048c]0   [0-9a-f]{8}H   .{3}    data\s+\?g_align64@@3NA', rd("out/x64/align%d6.map" % l)), "align%d6.map: g_align64 64-aligned" % l)
skip("link + run of the native, matrix and alignment drivers; dumpbin address check (Windows-only)")

test("Test 14: object lifetimes")
for l in (0, 2, 3): c6("life%d" % l, "../examples/sample-life/life.cpp", l)
ir = rd("out/x64/life0-ir6.txt")
ok('call Tracker::~Tracker()' in ir and 'call Text::Text(const Text&)' in ir and 'catchparam ...' in ir, "IR shows dtor calls, copy-ctor calls, unwind handlers")
skip("link + run of driver-life (Windows-only)")

test("Test 15: standard library")
for l in (0, 2):
    for u in ["examples/sample-lib/lib1.cpp", "examples/sample-lib/lib2.cpp", "examples/sample-lib/lib3.cpp", "libcpp/iostream.cpp"]:
        c6(os.path.splitext(os.path.basename(u))[0] + "L%d" % l, "../" + u, l, ["-I", "../include"])
c6("mangle", "../examples/sample-mangle/mangle.cpp", 0, ["-I", "../include"])
skip("link + run of driver-lib, cl name comparison for sample-mangle (Windows-only)")

test("Test 16: self-hosting constructs")
for l in (0, 2, 3): c6("self%d" % l, "../examples/sample-self/self.cpp", l, ["-I", "../include"])
skip("link + run of driver-self (Windows-only)")

# ---- 17: optimiser regressions; each case is read in the IR after optimisation (out/fix/<name>-ir6.txt)
def opt6(name, src, lvl):
    mk("out/fix")
    rc, out = run("--compile6", "-O%d" % lvl, "-o", "out/fix", "-n", name, src)
    good = 'sema: 0 error' in out and os.path.exists("out/fix/%s-ir6.txt" % name)
    ok(good, "%s -O%d compiles (exit %d)" % (src.split('/')[-1], lvl, rc))
    if not good: print("\n".join("       " + l for l in out.splitlines()[-8:]))
def optimised(name, fn):
    for part in rd("out/fix/%s-ir6.txt" % name).split("=" * 80):
        if ("function %s(" % fn) in part:
            i = part.find("--- after -O"); return part[i:] if i >= 0 else ""
    return ""

test("Test 17: optimiser regressions")
for l in (1, 2, 3):
    opt6("storeload%d" % l, "../examples/sample-optfix/storeload.cpp", l)
    f = optimised("storeload%d" % l, "storeThenLocal")
    ok(f and 'ret #1' not in f and ('ret #2' in f or 'loadmem' in f), "storeload -O%d: *p after x = 2 is not the 1 stored before" % l)
def blockOf(text, pattern):
    # the label of the block holding the first instruction that matches pattern ("" when none does)
    label = ""
    for line in text.splitlines():
        m = re.match(r'^B\d+:\s+;\s*(\S+)', line)
        if m: label = m.group(1)
        elif re.search(pattern, line): return label
    return ""
for l in (2, 3):
    opt6("licmdiv%d" % l, "../examples/sample-optfix/licmdiv.cpp", l)
    g = blockOf(optimised("licmdiv%d" % l, "guardedDivide"), r'bin sdiv')
    ok(g == "if.then", "licmdiv -O%d: the guarded 100 / d stays under its if (in %s)" % (l, g or "none"))
    z = blockOf(optimised("licmdiv%d" % l, "zeroTripDivide"), r'bin sdiv')
    ok(z == "for.body", "licmdiv -O%d: 100 / d stays in a body that may run zero times (in %s)" % (l, z or "none"))
    c = blockOf(optimised("licmdiv%d" % l, "constantDivide"), r'bin sdiv \S+, #4')
    ok(c == "entry", "licmdiv -O%d: k / 4 is still hoisted out of the loop (in %s)" % (l, c or "none"))

print("\n=== Summary ===")
T = [0, 0, 0]
for k, v in results.items():
    print("  %-45s ok %3d  fail %2d  skip %d" % (k, v[0], v[1], v[2]))
    T = [T[i] + v[i] for i in range(3)]
print("  %-45s ok %3d  fail %2d  skip %d" % ("TOTAL", T[0], T[1], T[2]))
sys.exit(1 if T[1] else 0)
