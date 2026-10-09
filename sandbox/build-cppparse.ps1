# build-cppparse.ps1 - compile cppparse with MSVC (Visual Studio 2026) and run its tests.
#
# Usage (from any directory):
#   powershell -ExecutionPolicy Bypass -File "cpp\sandbox\build-cppparse.ps1"
#
# cmd.exe is disabled by policy on this machine, so the MSVC environment is
# set by hand (see build.ps1). The sources are strict C++98; MSVC's oldest
# mode is C++14, so they compile unchanged. /W4 /permissive- is used and any
# warning fails the build.
#
# Tests:
#   1. examples\sample\Sample.sln  (Visual Studio 2010 solution)  -> sandbox\out\sample\
#      the text report is checked for expected findings
#   2. the cpp folder itself (folder input, includes cppparse's own sources)
#   3. Vector.hpp + VectorTest.cpp given as plain files

# Configurations:
#   -Config Release   (default) /O2, output sandbox\cppparse.exe, objects in obj\cppparse
#   -Config Debug     /Od /Zi /RTC1 /D_DEBUG /MTd, output sandbox\debug\cppparse.exe (+ .pdb), objects in obj\debug
#   -NoTests          build only
#   -Exe <path>       no build: run the tests with that compiler, e.g. the self-built
#                     sandbox\out\selfhost\cppparse-self.exe (see scripts\selfhost.ps1)
param(
    [ValidateSet("Release", "Debug")] [string] $Config = "Release",
    [switch] $NoTests,
    [string] $Exe = ""
)

$ErrorActionPreference = "Stop"

$Sandbox = $PSScriptRoot
$Src     = Split-Path $Sandbox -Parent
# cppparse's source folders (see scripts\toolset.ps1): each is an include folder of the build as well
$SourceDirs = @("driver", "common", "frontend", "middle", "backend", "analyzer", "analyzer\report")
$sourceIncludes = @(); foreach ($d in $SourceDirs) { $sourceIncludes += "/I..\cppparse\$d" }

# PowerShell names are case-insensitive, so $Exe is also the $exe the tests run
if ($Exe -ne "") {
    if (Test-Path $Exe) { $Exe = (Resolve-Path $Exe).Path }
    elseif (Test-Path "$Sandbox\$Exe") { $Exe = (Resolve-Path "$Sandbox\$Exe").Path }
    else { throw "compiler not found: $Exe" }
}

$VsRoot  = "D:\Microsoft Visual Studio 2026"
$MsvcVer = (Get-ChildItem "$VsRoot\VC\Tools\MSVC" | Sort-Object Name | Select-Object -Last 1).Name
$Msvc    = "$VsRoot\VC\Tools\MSVC\$MsvcVer"
$SdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$SdkVer  = (Get-ChildItem "$SdkRoot\Include" | Sort-Object Name | Select-Object -Last 1).Name

$env:PATH    = "$Msvc\bin\Hostx64\x64;$env:PATH"
$env:INCLUDE = "$Msvc\include;$SdkRoot\Include\$SdkVer\ucrt;$SdkRoot\Include\$SdkVer\um;$SdkRoot\Include\$SdkVer\shared"
$env:LIB     = "$Msvc\lib\x64;$SdkRoot\Lib\$SdkVer\ucrt\x64;$SdkRoot\Lib\$SdkVer\um\x64"

$cl = "$Msvc\bin\Hostx64\x64\cl.exe"

# the compiler and linker write temporary files to TMP: keep them on this drive (C: may be full)
New-Item -ItemType Directory -Force "$Sandbox\out\tmp" | Out-Null
$env:TMP  = "$Sandbox\out\tmp"
$env:TEMP = "$Sandbox\out\tmp"

Write-Host "=== Toolset: MSVC $MsvcVer, Windows SDK $SdkVer ==="

Push-Location $Sandbox
try {
    New-Item -ItemType Directory -Force "obj\cppparse" | Out-Null
    New-Item -ItemType Directory -Force "out\sample"   | Out-Null
    New-Item -ItemType Directory -Force "out\folder"   | Out-Null
    New-Item -ItemType Directory -Force "out\files"    | Out-Null

    if ($Config -eq "Debug") { $objDir = "obj\debug"; $exeDir = "$Sandbox\debug"; $cflags = @("/Od", "/Zi", "/RTC1", "/D_DEBUG", "/MTd") }
    else { $objDir = "obj\cppparse"; $exeDir = $Sandbox; $cflags = @("/O2") }
    if ($Exe -ne "") { Write-Host "=== Not building: the tests run $exe ===" }
    else {
        New-Item -ItemType Directory -Force $objDir, $exeDir | Out-Null
        $exe = "$exeDir\cppparse.exe"
        Write-Host "=== Building cppparse.exe ($Config) ==="
        $sources = @(); foreach ($d in $SourceDirs) { $sources += @(Get-ChildItem "..\cppparse\$d\*.cpp" | ForEach-Object { "..\cppparse\$d\" + $_.Name }) }
        $linkFlags = @(); if ($Config -eq "Debug") { $linkFlags = @("/link", "/DEBUG") }
        $output = & $cl /nologo /EHsc /W4 /permissive- @cflags @sourceIncludes "/Fo$objDir\" "/Fe$exe" "/Fd$objDir\cppparse.pdb" @sources @linkFlags 2>&1 | Out-String
        Write-Host $output
        if ($LASTEXITCODE -ne 0) { throw "cppparse failed to compile (exit code $LASTEXITCODE)" }
        if ($output -match "warning C") { throw "cppparse compiled with warnings (W4 must be clean)" }
    }

    # ---- test 1: Visual Studio 2010 solution --------------------------------
    if ($NoTests) { Write-Host "built $exe"; $global:LASTEXITCODE = 0; return }

    Write-Host "=== Test 1: sample\Sample.sln ==="
    & $exe -o "out\sample" "..\examples\sample\Sample.sln"
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on Sample.sln" }

    $report = Get-Content "out\sample\Sample-report.txt" -Raw
    $expected = @(
        'Visual Studio 2010, ToolsVersion 4.0, toolset v100',
        'class geom::Circle : public Shape, private Named',
        'class geom::Rect : public Shape, virtual public Named',
        'template <typename T, int N = 16>',
        'class util::FixedStack<bool, 8>',
        'class util::FixedStack<T*, 4>',
        'class util::FixedStack::Iterator',
        'struct geom::Rect::Corner',
        'include/Legacy.h   (included by main.cpp)',
        '1>src/main.cpp(9): error E100: ',
        'function-try-block',
        'double sum = 0.0',
        'const Shape* s = m_shapes[i]',
        '#if: defined(SAMPLE_WITH_3D)',
        'SAMPLE_MAX(a, b)',
        'static int s_liveCount',
        'defined src/Shape.cpp:',
        'operator bool',
        'operator<<',
        'virtual double area() const = 0',
        'util::Matrix2::rows() const is declared but no definition was found',
        'void (*m_callback)(const Shape&)',
        'extern int g_shapeVersion',
        'typedef void (*ReportFn)(const Shape&)',
        'struct LegacyStatus',
        'unsigned m_flags : 4',
        'SAMPLE_API double geom::totalArea',
        'preprocessor definitions: WIN32; _DEBUG; _CONSOLE; SAMPLE_STATIC',
        'Files parsed: 6  (headers 4, sources 2, of which found only through #include: 1)'
    )
    $failed = 0
    foreach ($e in $expected) {
        if ($report.Contains($e)) { Write-Host "  ok   $e" } else { Write-Host "  FAIL missing: $e"; $failed++ }
    }
    $forbidden = @('class geom::Obsolete', 'void broken')
    foreach ($e in $forbidden) {
        if ($report.Contains($e)) { Write-Host "  FAIL must not appear: $e"; $failed++ } else { Write-Host "  ok   absent: $e" }
    }
    foreach ($f in @('Sample-report.html', 'Sample-report.pdf', 'Sample-diagram.svg', 'Sample-diagram.dot')) {
        $p = "out\sample\$f"
        if ((Test-Path $p) -and ((Get-Item $p).Length -gt 500)) { Write-Host "  ok   $f ($((Get-Item $p).Length) bytes)" } else { Write-Host "  FAIL missing or empty: $f"; $failed++ }
    }
    $symbols = Get-Content "out\sample\Sample-symbols.json" -Raw
    foreach ($e in @('"kind": "class", "name": "Circle", "qualified": "geom::Circle"', '"kind": "method", "name": "area"', '"kind": "local", "name": "sum"', '"kind": "macro", "name": "SAMPLE_MAX"', '"kind": "parameter", "name": "radius"')) {
        if ($symbols.Contains($e)) { Write-Host "  ok   symbols: $e" } else { Write-Host "  FAIL symbols missing: $e"; $failed++ }
    }
    $map = Get-Content "out\sample\Sample-symbols.map" -Raw
    foreach ($e in @(' Symbols by module', ' Cross reference', 'geom::Circle::area', 'geom::Canvas::add  (', ' Entry point', 'main(int argc, char* argv[])')) {
        if ($map.Contains($e)) { Write-Host "  ok   map: $e" } else { Write-Host "  FAIL map missing: $e"; $failed++ }
    }
    $pdf = Get-Content "out\sample\Sample-report.pdf" -Raw
    if ($pdf.StartsWith('%PDF-1.4') -and $pdf.Contains('%%EOF')) { Write-Host "  ok   PDF structure" } else { Write-Host "  FAIL PDF structure"; $failed++ }
    $svg = Get-Content "out\sample\Sample-diagram.svg" -Raw
    if ($svg.Contains('class geom::Circle') -and $svg.Contains('<polygon')) { Write-Host "  ok   SVG contents" } else { Write-Host "  FAIL SVG contents"; $failed++ }
    # the 8 report sections, in order
    $sections = @('1. PROJECT', '2. FILES', '3. CLASSES', '4. CLASS DIAGRAM', '5. GLOBAL DEFINITIONS', '6. EXCEPTION HANDLING', '7. HEALTH', '8. TOTALS')
    $pos = -1; $inOrder = $true
    foreach ($s in $sections) { $at = $report.IndexOf("`n$s"); if ($at -le $pos) { $inOrder = $false; Write-Host "  FAIL section missing or out of order: $s" }; $pos = $at }
    if ($inOrder) { Write-Host "  ok   report sections 1 to 8 in order" } else { $failed++ }
    # section 4: the inheritance tree (multiple, private and virtual inheritance, abstract and nested classes)
    $tree = $report.Substring($report.IndexOf("`n4. CLASS DIAGRAM"), $report.IndexOf("`n5. GLOBAL") - $report.IndexOf("`n4. CLASS DIAGRAM"))
    foreach ($e in @('class geom::Shape  [abstract]', '+-- class geom::Circle  (private)  also derives from: Shape',
                     '+-- class geom::Rect  (public virtual)  also derives from: Shape', '+-- class geom::Sphere  (public)',
                     'struct geom::Rect::Corner  [nested in geom::Rect]', '+-- class util::Matrix2  (public)')) {
        if ($tree.Contains($e)) { Write-Host "  ok   tree: $e" } else { Write-Host "  FAIL tree missing: $e"; $failed++ }
    }
    # the HTML report is one self-contained page with the diagram inside
    $html = Get-Content "out\sample\Sample-report.html" -Raw
    if ($html.Contains('<svg') -and -not ($html -match '(src|href)="(https?|file):')) { Write-Host "  ok   HTML self-contained with the diagram" } else { Write-Host "  FAIL HTML diagram missing or external references"; $failed++ }
    # the PDF has a title page, diagram pages and text pages
    $pages = ([regex]::Matches($pdf, '/Type\s*/Page[^s]')).Count
    if ($pages -ge 3) { Write-Host "  ok   PDF pages: $pages" } else { Write-Host "  FAIL PDF has $pages page(s)"; $failed++ }
    # the diagram legend: private red, virtual labelled, member dashed blue, nested dotted grey, abstract yellow
    $dot = Get-Content "out\sample\Sample-diagram.dot" -Raw
    foreach ($e in @('"geom::Circle" -> "geom::Named" [arrowhead=empty, color="#b03030"]',
                     '"geom::Rect" -> "geom::Named" [arrowhead=empty, color="#222222", label="virtual"]',
                     '"geom::Canvas" -> "geom::Shape" [style=dashed, arrowhead=open, color="#1a6fb5"]',
                     '"geom::Rect::Corner" -> "geom::Rect" [style=dotted, arrowhead=none, color="#888888", label="nested"]',
                     'fillcolor="#fff6d5"')) {
        if ($dot.Contains($e)) { Write-Host "  ok   dot: $e" } else { Write-Host "  FAIL dot missing: $e"; $failed++ }
    }
    # the map: callers in the cross reference and the unreferenced list
    foreach ($e in @('(?s)LegacyCounter::tick  \(1\)\s+src/main\.cpp:37\s+in main', '(?s)\(anonymous\)::kPi  \(2\)\s+src/Shape\.cpp:74\s+in geom::Circle::area\s+src/Shape\.cpp:80\s+in geom::Circle::perimeter',
                     ' Unreferenced symbols ')) {
        if ($map -match $e) { Write-Host "  ok   map: $e" } else { Write-Host "  FAIL map missing: $e"; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed check(s) failed on Sample.sln" }

    # ---- test 2: folder input (the cpp folder itself) ------------------------
    Write-Host "=== Test 2: folder input (cpp folder, includes cppparse's own sources) ==="
    & $exe -o "out\folder" -n "cpp-folder" ".."
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on the folder" }
    $report2 = Get-Content "out\folder\cpp-folder-report.txt" -Raw
    $failed = 0
    foreach ($e in @('class cppparse::Parser', 'class cppparse::CodeBase', 'class Vector', 'class Matrix', 'struct cppparse::ClassInfo', 'folder scan (recursive)')) {
        if ($report2.Contains($e)) { Write-Host "  ok   $e" } else { Write-Host "  FAIL missing: $e"; $failed++ }
    }
    # operator() keeps its own parentheses apart from the parameter list in the map
    $map2 = Get-Content "out\folder\cpp-folder-symbols.map" -Raw
    if ($map2.Contains('mx::Matrix::operator()(int r, int c)') -and -not $map2.Contains('operator()()(')) { Write-Host "  ok   map: operator()(int r, int c)" } else { Write-Host "  FAIL map: operator() shown wrongly"; $failed++ }
    if ($failed -ne 0) { throw "$failed check(s) failed on the folder input" }

    # ---- test 3: plain files -------------------------------------------------
    Write-Host "=== Test 3: plain files (Vector.hpp, VectorTest.cpp) ==="
    & $exe -o "out\files" -n "vector" "..\Vector.hpp" "..\VectorTest.cpp"
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on plain files" }
    $report3 = Get-Content "out\files\vector-report.txt" -Raw
    if (-not $report3.Contains('template <typename T, int N>')) { throw "Vector template not found in the plain-files report" }
    Write-Host "  ok   Vector template found"

    # ---- test 4: health checker on the deliberately broken folder ----------------
    Write-Host "=== Test 4: health checker (sample-bad folder) ==="
    New-Item -ItemType Directory -Force "out\bad" | Out-Null
    & $exe -o "out\bad" -n "bad" --text-only "..\examples\sample-bad"
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on sample-bad" }
    $health = Get-Content "out\bad\bad-health.txt" -Raw
    # E142-E145 and I146 are the count-based call checks; for type-checked bodies the typed T codes replace them
    $codes = @('E100', 'E101', 'W102', 'I103', 'E110', 'E120', 'E121', 'W122', 'E123', 'E124', 'W125', 'W126', 'I127', 'W128',
               'E130', 'E131', 'W132', 'W133', 'E134', 'E135', 'E136', 'W137', 'W138', 'W140', 'W141',
               'W147', 'W150', 'W151', 'W152', 'W153', 'W154', 'W155', 'E156', 'W157', 'E160', 'W161', 'E162',
               'E163', 'W164', 'W165')
    $failed = 0
    foreach ($code in $codes) {
        if ($health -match "(?m)^\d+>[^\r\n]*\(\d+\): (error|warning|info) ${code}: ") { Write-Host "  ok   $code" } else { Write-Host "  FAIL code not reported: $code"; $failed++ }
    }
    & $exe --strict --health "..\examples\sample-bad" | Out-Null
    if ($LASTEXITCODE -eq 3) { Write-Host "  ok   --strict exit code 3" } else { Write-Host "  FAIL --strict exit code was $LASTEXITCODE"; $failed++ }
    if ($failed -ne 0) { throw "$failed check(s) failed on sample-bad" }

    # the good sample must produce only the intended findings
    $goodHealth = Get-Content "out\sample\Sample-health.txt" -Raw
    if ($goodHealth -match "errors (\d+), warnings (\d+), info (\d+)") {
        Write-Host "  sample (good) project: errors $($Matches[1]), warnings $($Matches[2]), info $($Matches[3])"
        if ([int]$Matches[1] -gt 1) { throw "the good sample reports more than the one intended error (Missing.h)" }
    }
    $tcodes = @('T100', 'T101', 'T102', 'T104', 'T106', 'T107', 'T108', 'T109', 'T110', 'T111', 'T112', 'T115', 'T116', 'T117', 'T118', 'T119', 'T121', 'T122', 'T146')
    $failed = 0
    foreach ($code in $tcodes) {
        if ($health -match "(?m)^\d+>[^\r\n]*\(\d+\): (error|warning|info) ${code}: ") { Write-Host "  ok   $code" } else { Write-Host "  FAIL type code not reported: $code"; $failed++ }
    }
    if ($health -match "goodCode") { Write-Host "  FAIL goodCode must not produce findings"; $failed++ } else { Write-Host "  ok   goodCode has no findings" }
    if ($failed -ne 0) { throw "$failed type-check check(s) failed" }

    # ---- test 5: version 3 optimiser and lowering ---------------------------------
    Write-Host "=== Test 5: version 3 (AST, type check, IR, -O1/-O2/-O3, lowering) ==="
    New-Item -ItemType Directory -Force "out\opt" | Out-Null
    $failed = 0
    foreach ($lvl in 0..3) {
        & $exe -o "out\opt" -n "opt$lvl" "-O$lvl" --text-only "..\examples\sample-opt"
        if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on sample-opt at -O$lvl" }
        $h = Get-Content "out\opt\opt$lvl-health.txt" -Raw
        if ($h -match ": (error|warning|info) T1\d\d: ") { Write-Host "  FAIL sample-opt produced type findings at -O$lvl"; $failed++ }
    }
    $ir1 = Get-Content "out\opt\opt1-ir.txt" -Raw
    $ir2 = Get-Content "out\opt\opt2-ir.txt" -Raw
    $ir3 = Get-Content "out\opt\opt3-ir.txt" -Raw
    $log1 = Get-Content "out\opt\opt1-opt.log" -Raw
    $log2 = Get-Content "out\opt\opt2-opt.log" -Raw
    $log3 = Get-Content "out\opt\opt3-opt.log" -Raw
    $asm3 = Get-Content "out\opt\opt3-asm.txt" -Raw
    $asm0 = Get-Content "out\opt\opt0-asm.txt" -Raw
    $ast0 = Get-Content "out\opt\opt0-ast.txt" -Raw
    function Section($text, $name) { $i = $text.IndexOf("function $name("); if ($i -lt 0) { return "" }; $j = $text.IndexOf("================", $i); if ($j -lt 0) { $j = $text.Length }; return $text.Substring($i, $j - $i) }
    function After($sec) { $i = $sec.IndexOf("--- after"); if ($i -lt 0) { return $sec }; return $sec.Substring($i) }
    $checks = @(
        @{ name = 'constants folds to ret #14 at -O1';        ok = ((After (Section $ir1 'constants')) -match 'ret #14') },
        @{ name = 'deadcode has no mul at -O1';               ok = (-not ((After (Section $ir1 'deadcode')) -match 'bin mul')) },
        @{ name = 'cse computes a*b once at -O1';             ok = ((([regex]::Matches((After (Section $ir1 'cse')), 'bin mul')).Count) -eq 1) },
        @{ name = 'strength: mul 8 became shl 3 at -O1';      ok = ((After (Section $ir1 'strength')) -match 'shl t\d+, #3') },
        @{ name = 'loop-invariant code motion ran at -O2';    ok = ($log2 -match 'loop-invariant code motion: [1-9]') },
        @{ name = 'square inlined into usePoint at -O2';      ok = (-not ((After (Section $ir2 'usePoint')) -match 'call square')) },
        @{ name = 'constructor initializer list stores x and y (-O0)'; ok = (((Section (Get-Content "out\opt\opt0-ir.txt" -Raw) 'Point::Point') -match 'addrfield x') -and ((Section (Get-Content "out\opt\opt0-ir.txt" -Raw) 'Point::Point') -match 'storemem')) },
        @{ name = 'usePoint folds to ret #12 at -O3';          ok = ((After (Section $ir3 'usePoint')) -match 'ret #12') },
        @{ name = 'unroll folds to ret #6 at -O3';            ok = ((After (Section $ir3 'unroll')) -match 'ret #6') },
        @{ name = 'optimisation log reports instruction counts'; ok = ($log1 -match 'result: \d+ -> \d+ instructions') },
        @{ name = 'assembly has prologue and epilogue';         ok = (($asm3 -match 'ENTER #\d+') -and ($asm3 -match 'RET')) },
        @{ name = 'assembly passes arguments in registers (-O0)'; ok = (($asm0 -match 'POP R0') -and ($asm0 -match 'CALL ')) },
        @{ name = 'assembly uses the vtable for a virtual call (sample)'; ok = $true },
        @{ name = 'AST dump annotates types';                 ok = ($ast0 -match 'binary .\*.\s+: int') }
    )
    foreach ($c in $checks) { if ($c.ok) { Write-Host "  ok   $($c.name)" } else { Write-Host "  FAIL $($c.name)"; $failed++ } }
    & $exe -o "out\sample" -O2 "..\examples\sample\Sample.sln" | Out-Null
    $asmS = Get-Content "out\sample\Sample-asm.txt" -Raw
    if ($asmS -match 'vptr') { Write-Host "  ok   virtual call lowered through the vtable (Sample)" } else { Write-Host "  FAIL no vtable call in Sample-asm.txt"; $failed++ }
    if ($ir2 -match '= phi ') { Write-Host "  ok   SSA form with phi nodes in the IR dump" } else { Write-Host "  FAIL no phi nodes in the -O2 IR dump"; $failed++ }
    $irS = Get-Content "out\sample\Sample-ir.txt" -Raw
    if ($irS -match 'function util::FixedStack<double, 32>::push') { Write-Host "  ok   class template instantiated and compiled (FixedStack<double, 32>::push)" } else { Write-Host "  FAIL FixedStack<double, 32>::push not instantiated"; $failed++ }
    if ($ir2 -match 'function maxOf<int>' -and $ir2 -match 'Buffer<int, 4>::add') { Write-Host "  ok   function template deduced and instantiated (maxOf<int>, Buffer<int, 4>)" } else { Write-Host "  FAIL template instantiation in sample-opt"; $failed++ }
    if ($log2 -match 'global value numbering|SSA construction') { Write-Host "  ok   SSA passes logged" } else { Write-Host "  FAIL SSA passes missing from the log"; $failed++ }
    if ($failed -ne 0) { throw "$failed version 3 check(s) failed" }

    # ---- test 6: version 4 x86-64 code, assembled with ml64, linked with an MSVC driver, executed ----
    Write-Host "=== Test 6: version 4 x86-64 (ml64 + link + run) ==="
    $ml64 = "$Msvc\bin\Hostx64\x64\ml64.exe"
    New-Item -ItemType Directory -Force "out\x64" | Out-Null
    $failed = 0
    # version 5: the exception runtime, linked with every driver
    $rtOut = & $cl /nologo /c /EHsc /W3 "/Foout\x64\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $rtOut; throw "cppparse_rt.cpp failed to compile" }
    Write-Host "  ok   runtime cppparse_rt.cpp compiled"
    foreach ($lvl in @(0, 2, 3)) {
        $asmPath = "out\opt\opt$lvl.asm"
        if (-not (Test-Path $asmPath)) { Write-Host "  FAIL missing $asmPath"; $failed++; continue }
        $mlOut = & $ml64 /nologo /c "/Foout\x64\opt$lvl.obj" $asmPath 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL ml64 rejected opt$lvl.asm:"; Write-Host $mlOut; $failed++; continue }
        Write-Host "  ok   ml64 assembled opt$lvl.asm"
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\driver$lvl.exe" "..\examples\sample-opt-driver\driver.cpp" "out\x64\opt$lvl.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of driver with opt$lvl.obj:"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\driver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   driver$lvl.exe: all results correct (-O$lvl)" }
        else { Write-Host "  FAIL driver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed version 4 check(s) failed" }

    Write-Host "=== Test 7: version 5 parallel pipeline (wildcards, @response file, -j determinism) ==="
    New-Item -ItemType Directory -Force "out\par" | Out-Null
    $failed = 0
    # a pattern that matches nothing is an error, so only the folders that have headers get a *.hpp line
    $rsp = @("# response file written by the test")
    foreach ($d in $SourceDirs) { foreach ($x in @("cpp", "hpp")) { if (Test-Path "..\cppparse\$d\*.$x") { $rsp += "..\cppparse\$d\*.$x" } } }
    ($rsp + @("-O3", "-q")) | Out-File -Encoding ascii "out\par\list.rsp"
    $null = & $exe -j 1 -o out\par -n j1 "@out\par\list.rsp"
    if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL -j 1 run exited with $LASTEXITCODE"; $failed++ }
    $null = & $exe -j 12 -o out\par -n j12 "@out\par\list.rsp"
    if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL -j 12 run exited with $LASTEXITCODE"; $failed++ }
    $rep = Get-Content "out\par\j1-report.txt" -Raw
    if ($rep -match 'Files parsed: (\d+)' -and [int]$Matches[1] -ge 40) { Write-Host "  ok   @response file with *.cpp / *.hpp patterns: $($Matches[1]) files" }
    else { Write-Host "  FAIL response file / wildcard expansion"; $failed++ }
    foreach ($suffix in @("-ir.txt", "-asm.txt", ".asm", "-health.txt", "-ast.txt")) {
        $a = (Get-Content "out\par\j1$suffix" -Raw) -replace 'j1([-.])', 'X$1' -replace '\d+ thread', 'T thread'
        $b = (Get-Content "out\par\j12$suffix" -Raw) -replace 'j12([-.])', 'X$1' -replace '\d+ thread', 'T thread'
        if ($a -eq $b) { Write-Host "  ok   $suffix identical with -j 1 and -j 12" } else { Write-Host "  FAIL $suffix differs between -j 1 and -j 12"; $failed++ }
    }
    $log = Get-Content "out\par\j12-opt.log" -Raw
    if ($log -match 'phase times \(ms, 12 thread') { Write-Host "  ok   phase times logged" } else { Write-Host "  FAIL phase times missing from the log"; $failed++ }
    $wild = & $exe -j 4 --no-compile -o out\par -n wild "..\examples\sample-opt\*.cpp" "..\Vect*.hpp" | Out-String
    if ($LASTEXITCODE -eq 0 -and $wild -match '2 files \(1 headers, 1 sources\)') { Write-Host "  ok   wildcards on the command line" } else { Write-Host "  FAIL command-line wildcards:"; Write-Host $wild; $failed++ }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"   # the expected stderr line must not stop the script
    $null = & $exe -q --no-compile -o out\par -n none "..\cppparse\nothing*.cpp" 2>&1
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0) { Write-Host "  ok   unmatched pattern is an error" } else { Write-Host "  FAIL unmatched pattern accepted"; $failed++ }
    if ($failed -ne 0) { throw "$failed version 5 check(s) failed" }

    Write-Host "=== Test 8: version 5 COFF writer (--obj, link with the driver, run) ==="
    $failed = 0
    foreach ($lvl in @(0, 3)) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $out = & $exe "-O$lvl" --obj -q -o out\x64 -n "own$lvl" "..\examples\sample-opt" 2>&1 | Out-String
        $ErrorActionPreference = $eap
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "out\x64\own$lvl.obj")) { Write-Host "  FAIL --obj at -O$lvl :"; Write-Host $out; $failed++; continue }
        Write-Host "  ok   own COFF writer produced own$lvl.obj (-O$lvl)"
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\owndriver$lvl.exe" "..\examples\sample-opt-driver\driver.cpp" "out\x64\own$lvl.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of driver with own$lvl.obj:"; Write-Host $clOut; $failed++; continue }
        Write-Host "  ok   linked own$lvl.obj with the MSVC-compiled driver"
        $run = & ".\out\x64\owndriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   owndriver$lvl.exe: all results correct (-O$lvl, cppparse .obj)" }
        else { Write-Host "  FAIL owndriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    # the encoder must agree with ml64: compare the instruction bytes of both objects
    $dumpbin = "$Msvc\bin\Hostx64\x64\dumpbin.exe"
    if (Test-Path $dumpbin) {
        $a = (& $dumpbin /nologo /disasm:nobytes "out\x64\opt3.obj" | Select-String '^\s+[0-9A-F]{16}:\s' | ForEach-Object { ($_.Line -replace '^\s+[0-9A-F]{16}:\s+', '') -replace '\s+', ' ' })
        $b = (& $dumpbin /nologo /disasm:nobytes "out\x64\own3.obj" | Select-String '^\s+[0-9A-F]{16}:\s' | ForEach-Object { ($_.Line -replace '^\s+[0-9A-F]{16}:\s+', '') -replace '\s+', ' ' })
        $a = $a | Where-Object { $_ -ne 'int 3' }; $b = $b | Where-Object { $_ -ne 'int 3' }
        $same = 0; $diff = 0
        for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($i -lt $a.Count -and $i -lt $b.Count -and $a[$i] -eq $b[$i]) { $same++ } else { $diff++ } }
        if ($diff -eq 0 -and $same -gt 100) { Write-Host "  ok   dumpbin disassembly of own3.obj matches ml64's opt3.obj ($same instructions)" }
        else { Write-Host "  FAIL disassembly differs: $same same, $diff different (first differences follow)"; $shown = 0; for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count) -and $shown -lt 8; $i++) { if ($i -ge $a.Count -or $i -ge $b.Count -or $a[$i] -ne $b[$i]) { Write-Host "    [$i] ml64: $($a[$i])   own: $($b[$i])"; $shown++ } }; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed COFF check(s) failed" }

    Write-Host "=== Test 9: version 5 MSVC-compatible mangling (--msvc-mangling, native C++ driver) ==="
    $failed = 0
    $clObj = & $cl /nologo /c /EHsc "/Foout\x64\opt_cl.obj" "..\examples\sample-opt\opt.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clObj; throw "cl could not compile sample-opt\opt.cpp" }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $out = & $exe -O2 --obj --msvc-mangling -q -o out\x64 -n msvc "..\examples\sample-opt" 2>&1 | Out-String
    $ErrorActionPreference = $eap
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "out\x64\msvc.obj")) { Write-Host "  FAIL --msvc-mangling --obj:"; Write-Host $out; $failed++ }
    else {
        Write-Host "  ok   msvc.obj written with decorated names"
        # every function cl defined must exist, spelled identically, in cppparse's object
        $clSyms = & $dumpbin /nologo /symbols "out\x64\opt_cl.obj" | Select-String 'SECT.*\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Where-Object { $_ -notmatch '^\?catch\$|^\?\?_R|^\?\?_7type_info' } | Sort-Object -Unique
        $ourSyms = & $dumpbin /nologo /symbols "out\x64\msvc.obj" | Select-String '\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Sort-Object -Unique
        $missing = @($clSyms | Where-Object { $ourSyms -notcontains $_ })
        if ($missing.Count -eq 0) { Write-Host "  ok   all $($clSyms.Count) function names cl produced are spelled identically by cppparse" }
        else { Write-Host "  FAIL $($missing.Count) of $($clSyms.Count) decorated names differ:"; $missing | ForEach-Object { Write-Host "    cl: $_" }; $ourSyms | Select-Object -First 40 | ForEach-Object { Write-Host "    ours: $_" }; $failed++ }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\nativedriver.exe" "..\examples\sample-opt-driver\driver-native.cpp" "out\x64\msvc.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the native-declaration driver:"; Write-Host $clOut; $failed++ }
        else {
            Write-Host "  ok   linked driver-native.cpp (plain C++ declarations) with msvc.obj"
            $run = & ".\out\x64\nativedriver.exe" 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   nativedriver.exe: all results correct" } else { Write-Host "  FAIL nativedriver.exe:"; Write-Host $run; $failed++ }
        }
    }
    if ($failed -ne 0) { throw "$failed mangling check(s) failed" }

    Write-Host "=== Test 10: version 6 preprocessor (-E against cl /EP) ==="
    $failed = 0
    Push-Location "..\examples\sample-pp"
    try {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"   # cl prints the file name on stderr
        $clPp = & $cl /nologo /EP /Iinc pp.cpp 2>$null | Out-String
        $ErrorActionPreference = $eap
        # Start-Process captures stderr reliably (a plain 2> redirection loses it under SilentlyContinue)
        $p = Start-Process -FilePath $exe -ArgumentList '-E','-I','inc','pp.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\par\pp-out.txt" -RedirectStandardError "$Sandbox\out\par\pp-diag.txt"
        $rc = $p.ExitCode
    } finally { Pop-Location }
    $ourPp = Get-Content "$Sandbox\out\par\pp-out.txt" -Raw
    # cl /EP passes #pragma lines through; cppparse consumes them
    $norm = { param($s) ($s -split "`r?`n") | ForEach-Object { $_ -replace '\s', '' } | Where-Object { $_ -ne '' -and $_ -notmatch '^#pragma' } }
    $a = @(& $norm $clPp); $b = @(& $norm $ourPp)
    $same = 0; $diff = 0
    for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($i -lt $a.Count -and $i -lt $b.Count -and $a[$i] -eq $b[$i]) { $same++ } else { $diff++ } }
    if ($rc -eq 0 -and $diff -eq 0 -and $same -gt 20) { Write-Host "  ok   cppparse -E matches cl /EP on sample-pp ($same lines)" }
    else {
        Write-Host "  FAIL preprocessor output differs from cl /EP (exit $rc, $same same, $diff different):"
        $shown = 0
        for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count) -and $shown -lt 10; $i++) { if ($i -ge $a.Count -or $i -ge $b.Count -or $a[$i] -ne $b[$i]) { Write-Host "    [$i] cl: $($a[$i])"; Write-Host "    [$i] us: $($b[$i])"; $shown++ } }
        Get-Content "$Sandbox\out\par\pp-diag.txt" | Select-Object -First 10 | ForEach-Object { Write-Host "    diag: $_" }
        $failed++
    }
    $diagText = Get-Content "$Sandbox\out\par\pp-diag.txt" -Raw -ErrorAction SilentlyContinue
    if ($diagText -match 'error') { Write-Host "  FAIL preprocessor reported errors:"; Write-Host $diagText; $failed++ } else { Write-Host "  ok   no preprocessor errors" }
    # #error and a missing include are reported
    "#include ""nothere.h""`n#if 1`n#error stop here`n#endif`n" | Out-File -Encoding ascii "out\par\bad.cpp"
    $p = Start-Process -FilePath $exe -ArgumentList '-E',('"' + "$Sandbox\out\par\bad.cpp" + '"') -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\par\bad-out.txt" -RedirectStandardError "$Sandbox\out\par\bad-diag.txt"
    $rc = $p.ExitCode
    $bad = Get-Content "$Sandbox\out\par\bad-diag.txt" -Raw
    if ($rc -ne 0 -and $bad -match 'error: #error stop here' -and $bad -match 'include file not found: "nothere.h"') { Write-Host "  ok   #error and missing include diagnosed" }
    else { Write-Host "  FAIL diagnostics (exit $rc):"; Write-Host $bad; $failed++ }
    if ($failed -ne 0) { throw "$failed preprocessor check(s) failed" }

    Write-Host "=== Test 11: version 6 parser (--parse6 on the samples and on cppparse's own sources) ==="
    $failed = 0
    $p6 = & $exe --parse6 -o out\par -n opt6 "..\examples\sample-opt\opt.cpp" 2>&1 | Out-String
    if ($p6 -match ' 0 syntax error\(s\)' -and $p6 -match 'classes 4, enums 0, functions defined 26' -and $p6 -match 'templates 2') { Write-Host "  ok   sample-opt parses without errors (4 classes, 26 functions, 2 templates)" }
    else { Write-Host "  FAIL sample-opt:"; Write-Host $p6; $failed++ }
    $ast = Get-Content "out\par\opt6-ast6.txt" -Raw
    if ($ast -match 'function int square\(int x\)' -and $ast -match 'struct BadIndex : public Failure' -and $ast -match 'template <typename T, int N>' -and $ast -match 'catch \(decl\)' -and $ast -match 'return x \* x') { Write-Host "  ok   AST dump shows functions, bases, templates, handlers and expressions" }
    else { Write-Host "  FAIL AST dump contents"; $failed++ }
    $p6 = & $exe --parse6 -I ..\examples\sample\include "..\examples\sample\src\main.cpp" "..\examples\sample\src\Shape.cpp" 2>&1 | Out-String
    if (($p6 | Select-String ' 0 syntax error' -AllMatches).Matches.Count -eq 2) { Write-Host "  ok   the VS2010 sample (namespaces, templates, operators, virtual bases) parses without errors" }
    else { Write-Host "  FAIL sample:"; Write-Host $p6; $failed++ }
    $ownList = @(); $ownInc = @(); foreach ($d in $SourceDirs) { $ownList += @(Get-ChildItem "..\cppparse\$d\*.cpp" | ForEach-Object { $_.FullName }); $ownInc += @("-I", "..\cppparse\$d") }
    $own = & $exe --parse6 @ownInc $ownList 2>&1 | Out-String
    $ownErrors = ($own | Select-String '\): error' -AllMatches).Matches.Count
    $ownFiles = ($own | Select-String ' 0 syntax error' -AllMatches).Matches.Count
    if ($ownErrors -eq 0 -and $ownFiles -ge 30) { Write-Host "  ok   all $ownFiles of cppparse's own sources parse without syntax errors (self-hosting step 1)" }
    else { Write-Host "  FAIL own sources: $ownErrors error(s):"; ($own | Select-String '\): error') | Select-Object -First 10 | ForEach-Object { Write-Host "    $($_.Line.Trim())" }; $failed++ }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $bad = & $exe --parse6 (Get-ChildItem "..\examples\sample-bad\*.cpp" | ForEach-Object { $_.FullName }) 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0 -and $bad -match 'syntax error' -and $bad -match 'functions defined') { Write-Host "  ok   broken sample: errors reported, parsing continued" } else { Write-Host "  FAIL broken sample handling"; $failed++ }
    if ($failed -ne 0) { throw "$failed parser check(s) failed" }

    Write-Host "=== Test 12: version 6 semantic analysis (--sema6 on sample-opt and the samples) ==="
    $failed = 0
    $s6 = & $exe --sema6 -o out\par -n opt6 "..\examples\sample-opt\opt.cpp" 2>&1 | Out-String
    if ($s6 -match 'sema: 0 error\(s\)' -and $s6 -match 'functions analysed 26' -and $s6 -match 'unresolved 0' -and $s6 -match 'instantiations 1 classes / 1 functions') { Write-Host "  ok   sample-opt: no semantic errors, 26 functions, all names resolved, templates instantiated" }
    else { Write-Host "  FAIL sample-opt sema:"; Write-Host $s6; $failed++ }
    $sym = Get-Content "out\par\opt6-sym6.txt" -Raw
    $checks = @('struct Point  size 8 align 4', 'int y  @4', 'void\(int, int\) Point  \{body\}', 'int\(\) const sum', 'instantiation Buffer<int, 4>  size 20', 'int\[4\] items  @0', 'void\(const int&\) add', 'instantiation maxOf<int> int\(const int&, const int&\)', 'struct BadIndex : Failure@0  size 8', 'int index  @4')
    $missing = @($checks | Where-Object { $sym -notmatch $_ })
    if ($missing.Count -eq 0) { Write-Host "  ok   symbol dump: layouts, member functions, instantiated class and function template" }
    else { Write-Host "  FAIL symbol dump misses: $($missing -join ' | ')"; $failed++ }
    $s6 = & $exe --sema6 -I ..\examples\sample\include "..\examples\sample\src\Shape.cpp" 2>&1 | Out-String
    if ($s6 -match 'sema: \d+ error' -and $s6 -match 'calls resolved \d+' -and $s6 -notmatch 'ambiguous [1-9]') { Write-Host "  ok   the VS2010 sample analyses (errors only from missing std headers)" }
    else { Write-Host "  FAIL sample sema:"; Write-Host $s6; $failed++ }
    $bad = $s6 | Select-String '\): error S' | Where-Object { $_.Line -notmatch "std::|'size_t'|iterator" }
    if ($bad.Count -eq 0) { Write-Host "  ok   no semantic errors outside std:: uses" } else { Write-Host "  FAIL unexpected semantic errors:"; $bad | ForEach-Object { Write-Host "    $($_.Line.Trim())" }; $failed++ }
    if ($failed -ne 0) { throw "$failed sema check(s) failed" }

    Write-Host "=== Test 13: version 6 compile (--compile6 --obj --msvc-mangling, native driver) ==="
    $failed = 0
    foreach ($lvl in @(0, 2)) {
        $obj = "out\x64\v6o${lvl}6.obj"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -o out\x64 -n "v6o$lvl" "..\examples\sample-opt\opt.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path $obj)) { Write-Host "  FAIL --compile6 at -O$lvl :"; Write-Host $c6; $failed++; continue }
        Write-Host "  ok   version 6 pipeline produced $obj (-O$lvl)"
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\v6driver$lvl.exe" "..\examples\sample-opt-driver\driver-native.cpp" $obj "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the native driver with the version 6 object:"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\v6driver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   v6driver$lvl.exe: all results correct (-O$lvl, version 6 front end)" }
        else { Write-Host "  FAIL v6driver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed version 6 compile check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 14: version 6 object lifetimes (sample-life: destructors, copies, temporaries, unwinding) ==="
    $failed = 0
    foreach ($lvl in @(0, 2, 3)) {
        $obj = "out\x64\life${lvl}6.obj"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -o out\x64 -n "life$lvl" "..\examples\sample-life\life.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path $obj)) { Write-Host "  FAIL --compile6 of sample-life at -O$lvl :"; Write-Host $c6; $failed++; continue }
        if ($c6 -match 'sema: 0 error') { Write-Host "  ok   sample-life analysed without errors (-O$lvl)" } else { Write-Host "  FAIL sample-life has semantic errors:"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\lifedriver$lvl.exe" "..\examples\sample-life-driver\driver-life.cpp" $obj "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the lifetime driver:"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\lifedriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   lifedriver$lvl.exe: all 24 lifetime checks correct (-O$lvl)" }
        else { Write-Host "  FAIL lifedriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    $ir = Get-Content "out\x64\life0-ir6.txt" -Raw
    if ($ir -match 'call Tracker::~Tracker\(\)' -and $ir -match 'call Text::Text\(const Text&\)' -and $ir -match 'catchparam \.\.\.') { Write-Host "  ok   IR shows destructor calls, copy-constructor calls and the unwind handlers" }
    else { Write-Host "  FAIL IR lacks destructor / copy-constructor calls or unwind handlers"; $failed++ }
    if ($failed -ne 0) { throw "$failed object lifetime check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 15: version 6 standard library (include/: string, vector, map, set, algorithm, streams) ==="
    $failed = 0
    foreach ($lvl in @(0, 2)) {
        $objs = @()
        foreach ($unit in @("examples\sample-lib\lib1.cpp", "examples\sample-lib\lib2.cpp", "examples\sample-lib\lib3.cpp", "libcpp\iostream.cpp")) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($unit) + "L$lvl"
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $name "..\$unit" 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -ne 0 -or -not (Test-Path "out\x64\${name}6.obj")) { Write-Host "  FAIL --compile6 of $unit at -O$lvl :"; Write-Host $c6; $failed++; continue }
            if ($c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL $unit has semantic errors:"; Write-Host $c6; $failed++; continue }
            $objs += "out\x64\${name}6.obj"
        }
        if ($objs.Count -ne 4) { continue }
        Write-Host "  ok   the library samples and libcpp/iostream.cpp compile without errors (-O$lvl)"
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\libdriver$lvl.exe" "..\examples\sample-lib-driver\driver-lib.cpp" @objs "out\x64\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the library driver (COMDAT objects from four units):"; Write-Host $clOut; $failed++; continue }
        Write-Host "  ok   four objects link: inline functions and template instantiations are COMDAT"
        $run = & ".\out\x64\libdriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   libdriver$lvl.exe: all 13 library checks correct (-O$lvl)" }
        else { Write-Host "  FAIL libdriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
        if ($run -match 'cout from cppparse') { Write-Host "  ok   std::cout is a dynamically initialised global of libcpp/iostream.cpp (.CRT`$XCU)" }
        else { Write-Host "  FAIL std::cout produced no output"; $failed++ }
    }
    # the version 6 mangler against cl on library types: every decorated name cl defines or references must be
    # spelled identically by cppparse (cl's own helpers are left out: deleting destructors, RTTI, sized delete)
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /X /I..\include "/Foout\x64\mangle_cl.obj" "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-mangle\mangle.cpp" }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n mangle "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0 -or -not (Test-Path "out\x64\mangle6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL --compile6 of sample-mangle:"; Write-Host $c6; $failed++ }
    else {
        $clSyms = & $dumpbin /nologo /symbols "out\x64\mangle_cl.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Where-Object { $_ -notmatch '^\?\?_[GER7]|^\?\?3@YAXPEAX_K@Z$' } | Sort-Object -Unique
        $ourSyms = & $dumpbin /nologo /symbols "out\x64\mangle6.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Sort-Object -Unique
        $missing = @($clSyms | Where-Object { $ourSyms -notcontains $_ })
        if ($missing.Count -eq 0 -and $clSyms.Count -gt 100) { Write-Host "  ok   sample-mangle: all $($clSyms.Count) decorated names cl uses are spelled identically by the version 6 mangler" }
        else { Write-Host "  FAIL sample-mangle: $($missing.Count) of $($clSyms.Count) decorated names differ from cl's:"; $missing | Select-Object -First 30 | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed standard library check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 16: version 6 self-hosting constructs (sample-self: what cppparse got wrong compiling itself) ==="
    $failed = 0
    foreach ($lvl in @(0, 2, 3)) {
        $obj = "out\x64\self${lvl}6.obj"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n "self$lvl" "..\examples\sample-self\self.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path $obj)) { Write-Host "  FAIL --compile6 of sample-self at -O$lvl :"; Write-Host $c6; $failed++; continue }
        if ($c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL sample-self has semantic errors:"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\selfdriver$lvl.exe" "..\examples\sample-self-driver\driver-self.cpp" $obj "out\x64\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the self-hosting driver:"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\selfdriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   selfdriver$lvl.exe: all 18 self-hosting checks correct (-O$lvl)" }
        else { Write-Host "  FAIL selfdriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed self-hosting check(s) failed" }

    Write-Host ""
    Write-Host "=== cppparse: build and all tests succeeded ==="
    Write-Host "Reports: $Sandbox\out\sample\Sample-report.{txt,html,pdf}, Sample-diagram.{svg,dot}, Sample-health.txt"
    $global:LASTEXITCODE = 0
}
finally {
    Pop-Location
}
exit 0
