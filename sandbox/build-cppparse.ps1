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

    # checks an object map (<name>.map) against itself and, given the object, against dumpbin's symbol table:
    # no definition overlaps the next one, functions start on 16-byte boundaries with under 16 bytes of padding,
    # and every address is the section and offset the object really has. Returns the problems found.
    function Test-ObjectMap([string]$mapPath, [string]$objPath = "") {
        $problems = @()
        if (-not (Test-Path $mapPath)) { return @("missing $mapPath") }
        $lines = Get-Content $mapPath
        if ($lines -match 'not available') { return @("map not available: " + (($lines | Select-String 'line \d+:' | Select-Object -First 1).Line)) }
        $sections = @{}
        foreach ($l in $lines) { if ($l -match '^ ([0-9a-f]{4}):00000000 ([0-9a-f]{8})H  (\S+)') { $sections[[Convert]::ToInt32($Matches[1], 16)] = @{ Size = [Convert]::ToInt64($Matches[2], 16); Name = $Matches[3] } } }
        $syms = @(); $byName = @(); $inByName = $false
        foreach ($l in $lines) {
            if ($l -match '^ Symbols by Name') { $inByName = $true; continue }
            # a row: address, size, link's flags (3 characters: "f i", "f  " or blank), type, [linkage,] symbol, name
            if ($inByName) {
                if ($l -match '^ (?<sec>[0-9a-f]{4}):(?<off>[0-9a-f]{8})   (?<size>[0-9a-f]{8})H   (?<flags>.{3})    (?<type>\w+)\s+(?<link>public|static)\s+(?<sym>\S+)\s*(?<name>.*)$') {
                    $byName += [pscustomobject]@{ Key = "$($Matches.sec):$($Matches.off):$($Matches.size):$($Matches.flags):$($Matches.sym)"; Sort = $(if ($Matches.name) { $Matches.name } else { $Matches.sym }) }
                }
            } elseif ($l -match '^ (?<sec>[0-9a-f]{4}):(?<off>[0-9a-f]{8})   (?<size>[0-9a-f]{8})H   (?<flags>.{3})    (?<type>\w+)\s+(?<sym>\S+)') {
                $syms += [pscustomobject]@{ Sec = [Convert]::ToInt32($Matches.sec, 16); Off = [Convert]::ToInt64($Matches.off, 16); Size = [Convert]::ToInt64($Matches.size, 16); Flags = $Matches.flags; Type = $Matches.type; Name = $Matches.sym; Key = "$($Matches.sec):$($Matches.off):$($Matches.size):$($Matches.flags):$($Matches.sym)" }
            }
        }
        if ($syms.Count -eq 0) { return @("no symbols in $mapPath") }
        # link's flags: a function is "f", and "f i" when it is inline (in a COMDAT section, absent from the table)
        foreach ($s in $syms) {
            $want = if ($s.Type -ne 'function') { '   ' } elseif ($sections.ContainsKey($s.Sec)) { 'f  ' } else { 'f i' }
            if ($s.Flags -ne $want) { $problems += "$($s.Name): flags '$($s.Flags)', expected '$want'"; break }
        }
        # the by-name list at the end: the same symbols, addresses and sizes, in ascending name order
        $diff = @(Compare-Object @($syms | ForEach-Object Key | Sort-Object) @($byName | ForEach-Object Key | Sort-Object))
        if ($diff.Count) { $problems += "Symbols by Name differs from the address lists: " + ($diff | Select-Object -First 3 | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }) -join '; ' }
        for ($i = 1; $i -lt $byName.Count; $i++) { if ([string]::CompareOrdinal($byName[$i - 1].Sort, $byName[$i].Sort) -gt 0) { $problems += "Symbols by Name not in order at '$($byName[$i].Sort)'"; break } }
        foreach ($g in ($syms | Group-Object Sec)) {
            if (-not $sections.ContainsKey([int]$g.Name)) { continue }   # a COMDAT section: listed only as part of its group
            $list = @($g.Group | Sort-Object Off)
            for ($i = 0; $i -lt $list.Count; $i++) {
                $end = $list[$i].Off + $list[$i].Size
                $limit = if ($i + 1 -lt $list.Count) { $list[$i + 1].Off } else { $sections[[int]$g.Name].Size }
                if ($end -gt $limit) { $problems += "$($list[$i].Name) ends at $end beyond $limit" }
            }
            if ($sections[[int]$g.Name].Name -like '.text*') {
                # functions start on 16-byte boundaries; the int 3 padding before the next one is under 16 bytes
                # and the last function ends the section
                for ($i = 0; $i -lt $list.Count; $i++) {
                    if ($list[$i].Off % 16 -ne 0) { $problems += "$($list[$i].Name) at $($list[$i].Off) is not 16-byte aligned"; break }
                    $limit = if ($i + 1 -lt $list.Count) { $list[$i + 1].Off } else { $sections[[int]$g.Name].Size }
                    $gap = $limit - ($list[$i].Off + $list[$i].Size)
                    if ($gap -lt 0 -or $gap -gt 15 -or ($i + 1 -eq $list.Count -and $gap -ne 0)) { $problems += "$($list[$i].Name): $gap byte(s) before the next function"; break }
                }
            }
        }
        if ($objPath -ne "") {
            $real = @{}
            # dumpbin follows a decorated name with " (undecorated)": the name is the first word after the bar
            foreach ($l in (& "$Msvc\bin\Hostx64\x64\dumpbin.exe" /nologo /symbols $objPath)) {
                if ($l -match '^[0-9A-F]{3,} ([0-9A-F]{8}) SECT([0-9A-F]+)\s.*\| (\S+)') { $real[$Matches[3]] = @([Convert]::ToInt32($Matches[2], 16), [Convert]::ToInt64($Matches[1], 16)) }
            }
            foreach ($s in $syms) {
                if (-not $real.ContainsKey($s.Name)) { $problems += "$($s.Name) is not in the object"; continue }
                if ($real[$s.Name][0] -ne $s.Sec -or $real[$s.Name][1] -ne $s.Off) { $problems += "$($s.Name): map $($s.Sec):$($s.Off), object $($real[$s.Name][0]):$($real[$s.Name][1])" }
            }
        }
        return $problems
    }

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
        # --eh=sjlj: test 6 assembles this text with ml64, which keeps the ;@ehrange lines of table regions as
        # comments (their range table would be missing); test 8 compares its objects with the same model
        & $exe -o "out\opt" -n "opt$lvl" "-O$lvl" --eh=sjlj --text-only "..\examples\sample-opt"
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
    # own<lvl>: setjmp regions, compared with ml64's objects of test 6 below; owntab<lvl>: the default table regions
    foreach ($variant in @(@("own", "--eh=sjlj"), @("owntab", "--eh=table"))) {
    foreach ($lvl in @(0, 3)) {
        $nm = "$($variant[0])$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $out = & $exe "-O$lvl" $variant[1] --obj -q -o out\x64 -n $nm "..\examples\sample-opt" 2>&1 | Out-String
        $ErrorActionPreference = $eap
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "out\x64\$nm.obj")) { Write-Host "  FAIL --obj at -O$lvl $($variant[1]):"; Write-Host $out; $failed++; continue }
        Write-Host "  ok   own COFF writer produced $nm.obj (-O$lvl $($variant[1]))"
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\${nm}driver.exe" "..\examples\sample-opt-driver\driver.cpp" "out\x64\$nm.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of driver with $nm.obj:"; Write-Host $clOut; $failed++; continue }
        Write-Host "  ok   linked $nm.obj with the MSVC-compiled driver"
        $run = & ".\out\x64\${nm}driver.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   ${nm}driver.exe: all results correct (-O$lvl $($variant[1]), cppparse .obj)" }
        else { Write-Host "  FAIL ${nm}driver.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    }
    # the encoder must agree with ml64: compare the instruction bytes of both objects
    $dumpbin = "$Msvc\bin\Hostx64\x64\dumpbin.exe"
    if (Test-Path $dumpbin) {
        # dumpbin prints a jump's absolute target; our functions start on 16-byte boundaries (int 3 between them)
        # and ml64's are packed, so a jump is compared by its distance from the instruction instead
        $disasm = {
            param($obj)
            & $dumpbin /nologo /disasm:nobytes $obj | Select-String '^\s+([0-9A-F]{16}):\s+(.*)$' | ForEach-Object {
                $at = [Convert]::ToInt64($_.Matches[0].Groups[1].Value, 16); $text = $_.Matches[0].Groups[2].Value -replace '\s+', ' '
                if ($text -match '^(j\w+) ([0-9A-F]{16})$') { $text = "$($Matches[1]) .{0:+#;-#;+0}" -f ([Convert]::ToInt64($Matches[2], 16) - $at) }
                $text
            }
        }
        $a = & $disasm "out\x64\opt3.obj"
        $b = & $disasm "out\x64\own3.obj"
        $a = $a | Where-Object { $_ -ne 'int 3' }; $b = $b | Where-Object { $_ -ne 'int 3' }
        $same = 0; $diff = 0
        for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($i -lt $a.Count -and $i -lt $b.Count -and $a[$i] -eq $b[$i]) { $same++ } else { $diff++ } }
        if ($diff -eq 0 -and $same -gt 100) { Write-Host "  ok   dumpbin disassembly of own3.obj matches ml64's opt3.obj ($same instructions)" }
        else { Write-Host "  FAIL disassembly differs: $same same, $diff different (first differences follow)"; $shown = 0; for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count) -and $shown -lt 8; $i++) { if ($i -ge $a.Count -or $i -ge $b.Count -or $a[$i] -ne $b[$i]) { Write-Host "    [$i] ml64: $($a[$i])   own: $($b[$i])"; $shown++ } }; $failed++ }
    }
    # the unwind tables (.pdata / .xdata) must agree with the ones ml64 builds from the same prologue directives
    . "$PSScriptRoot\scripts\unwind.ps1"
    foreach ($lvl in @(0, 3)) {
        $ua = Get-UnwindTable "out\x64\opt$lvl.obj"; $ub = Get-UnwindTable "out\x64\own$lvl.obj"
        $bad = @($ua.Keys | Where-Object { $ua[$_] -ne $ub[$_] }) + @($ub.Keys | Where-Object { -not $ua.ContainsKey($_) })
        if ($ua.Count -gt 10 -and $bad.Count -eq 0) { Write-Host "  ok   unwind info of own$lvl.obj matches ml64's for all $($ua.Count) functions with a frame" }
        else { Write-Host "  FAIL unwind info differs from ml64's (ml64 $($ua.Count), own $($ub.Count) entries):"; $bad | Select-Object -First 4 | ForEach-Object { Write-Host "    $_`n      ml64: $($ua[$_])`n      own:  $($ub[$_])" }; $failed++ }
    }
    # the object map: real sizes, consistent with the sections and with dumpbin's symbol table
    foreach ($lvl in @(0, 3)) {
        $p = @(Test-ObjectMap "out\x64\own$lvl.map" "out\x64\own$lvl.obj")
        if ($p.Count -eq 0) { Write-Host "  ok   own$lvl.map: functions 16-aligned, sizes consistent, addresses match dumpbin /symbols" }
        else { Write-Host "  FAIL own$lvl.map:"; $p | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }; $failed++ }
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
        $p = @(Test-ObjectMap "out\x64\v6o${lvl}6.map" $obj)
        if ($p.Count -eq 0) { Write-Host "  ok   v6o${lvl}6.map: functions 16-aligned, sizes consistent, addresses match dumpbin /symbols" }
        else { Write-Host "  FAIL v6o${lvl}6.map:"; $p | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }; $failed++ }
    }
    # a map with vtables and COMDAT sections: the matrix sample through the version 6 pipeline
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $null = & $exe --compile6 -O2 --obj --msvc-mangling -q -I ..\include -I .. -I ..\examples\sample-matrix -o out\x64 -n mtx "..\examples\sample-matrix\matrix.cpp" 2>&1
    $ErrorActionPreference = $eap
    $p = @(Test-ObjectMap "out\x64\mtx6.map" "out\x64\mtx6.obj")
    $mtx = if (Test-Path "out\x64\mtx6.map") { Get-Content "out\x64\mtx6.map" -Raw } else { "" }
    if ($mtx -match 'vtable\s+\S+\s+\S+::`vftable''' -and $mtx -notmatch '(?m)data\s+\S+\s+\S+::`vftable''') { Write-Host "  ok   mtx6.map: vtables typed as vtable" } else { $p += "vtables not typed as vtable" }
    if ($mtx -match '(?m)^ [0-9a-f]{4}-[0-9a-f]{4}\s+[0-9a-f]{8}H  \.text\$mn\s+CODE  COMDAT, \d+ section') { Write-Host "  ok   mtx6.map: COMDAT sections grouped" } else { $p += "COMDAT sections not grouped" }
    if ($p.Count -eq 0) { Write-Host "  ok   mtx6.map: functions 16-aligned, sizes consistent, addresses match dumpbin /symbols" }
    else { Write-Host "  FAIL mtx6.map:"; $p | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }; $failed++ }
    # #pragma pack and __declspec(align(N)): cppparse's layouts and globals against a cl-compiled driver that
    # checks every size and offset with cl's own, reads the globals, and checks their alignment
    foreach ($lvl in @(0, 2)) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n "align$lvl" "..\examples\sample-align\align.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\align${lvl}6.obj")) { Write-Host "  FAIL sample-align at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\aligndriver$lvl.exe" /I..\examples\sample-align "..\examples\sample-align-driver\driver-align.cpp" "out\x64\align${lvl}6.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the alignment driver (-O$lvl):"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\aligndriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   aligndriver$lvl.exe: pack and align layouts equal cl's, globals read and aligned (-O$lvl)" }
        else { Write-Host "  FAIL aligndriver$lvl.exe (-O$lvl):"; Write-Host ($run.Split("`n") | Where-Object { $_ -match 'FAIL' }); $failed++ }
        $p = @(Test-ObjectMap "out\x64\align${lvl}6.map" "out\x64\align${lvl}6.obj")
        $am = Get-Content "out\x64\align${lvl}6.map" -Raw
        if ($am -notmatch '(?m)^ 0002:[0-9a-f]{6}[048c]0   [0-9a-f]{8}H   .{3}    data\s+\?g_align64@@3NA') { $p += "g_align64 not at a 64-byte offset in the map" }
        if ($p.Count -eq 0) { Write-Host "  ok   align${lvl}6.map: functions 16-aligned, sizes consistent, addresses match dumpbin, g_align64 64-aligned" }
        else { Write-Host "  FAIL align${lvl}6.map:"; $p | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }; $failed++ }
    }
    # MSVC's class layout (empty bases, bit-fields, anonymous unions, polymorphic bases, secondary vftables): every size and offset cppparse computes for layout.hpp against a
    # cl-compiled driver that evaluates the same expressions, and objects shared between the two compilers
    foreach ($lvl in @(0, 1, 2, 3)) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n "layout$lvl" "..\examples\sample-layout\layout.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\layout${lvl}6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL sample-layout at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\layoutdriver$lvl.exe" /I..\examples\sample-layout "..\examples\sample-layout-driver\driver-layout.cpp" "out\x64\layout${lvl}6.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the layout driver (-O$lvl):"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\layoutdriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   layoutdriver$lvl.exe: class layouts equal cl's, shared objects read back (-O$lvl)" }
        else { Write-Host "  FAIL layoutdriver$lvl.exe (-O$lvl):"; Write-Host ($run.Split("`n") | Where-Object { $_ -match 'FAIL' }); $failed++ }
    }
    # MSVC's x64 convention for class arguments and results (1/2/4/8-byte classes in registers, others by
    # address and hidden result pointer): a cl-compiled driver calls cppparse's functions and the reverse
    foreach ($lvl in @(0, 2)) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n "abi$lvl" "..\examples\sample-abi\abi.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\abi${lvl}6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL sample-abi at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\abidriver$lvl.exe" /I..\examples\sample-abi "..\examples\sample-abi-driver\driver-abi.cpp" "out\x64\abi${lvl}6.obj" "out\x64\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the calling-convention driver (-O$lvl):"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\abidriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   abidriver$lvl.exe: class arguments and results cross cl and cppparse both ways (-O$lvl)" }
        else { Write-Host "  FAIL abidriver$lvl.exe (-O$lvl):"; Write-Host ($run.Split("`n") | Where-Object { $_ -match 'FAIL' }); $failed++ }
    }
    # static locals of inline functions shared with cl (cl's $TSS guards and _Init_thread_header/footer): a
    # cl /O2 driver (which inlines them) and cppparse's unit reach each object first in turn, linked in both
    # orders (so either compiler's copy of the functions is kept); every object is constructed once
    & $cl /nologo /c /O2 /EHsc /W3 "/Foout\x64\driver-statics.obj" /I..\examples\sample-statics "..\examples\sample-statics-driver\driver-statics.cpp" | Out-Null
    foreach ($lvl in @(0, 2)) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n "statics$lvl" "..\examples\sample-statics\statics.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\statics${lvl}6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL sample-statics at -O$lvl :"; Write-Host $c6; $failed++; continue }
        foreach ($order in @("cppparse-first", "cl-first")) {
            $objs = if ($order -eq "cl-first") { @("out\x64\driver-statics.obj", "out\x64\statics${lvl}6.obj") } else { @("out\x64\statics${lvl}6.obj", "out\x64\driver-statics.obj") }
            $sexe = "out\x64\staticsdriver$lvl-$order.exe"
            $lo = & $cl /nologo "/Fe$sexe" @objs "out\x64\cppparse_rt.obj" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the statics driver (-O$lvl, $order):"; Write-Host $lo; $failed++; continue }
            $sargs = @(); if ($order -eq "cl-first") { $sargs = @("cl-copies") }
            $run = & ".\$sexe" @sargs 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   staticsdriver$lvl-${order}: shared static locals constructed once from cl and cppparse (-O$lvl)" }
            else { Write-Host "  FAIL staticsdriver$lvl-$order (-O$lvl):"; Write-Host ($run.Split("`n") | Where-Object { $_ -match 'FAIL' }); $failed++ }
        }
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
        $clOut = & $cl /nologo /EHsc /W3 "/DLIFE_NRVO=$([int]($lvl -ge 1))" "/Foout\x64\" "/Feout\x64\lifedriver$lvl.exe" "..\examples\sample-life-driver\driver-life.cpp" $obj "out\x64\cppparse_rt.obj" 2>&1 | Out-String
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
    # spelled identically by cppparse (cl's own helpers are left out: deleting destructors, RTTI, sized delete, and its string literal names)
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /X /I..\include "/Foout\x64\mangle_cl.obj" "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-mangle\mangle.cpp" }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n mangle "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0 -or -not (Test-Path "out\x64\mangle6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL --compile6 of sample-mangle:"; Write-Host $c6; $failed++ }
    else {
        $clSyms = & $dumpbin /nologo /symbols "out\x64\mangle_cl.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Where-Object { $_ -notmatch '^\?\?_[GER7C]|^\?\?3@YAXPEAX_K@Z$' } | Sort-Object -Unique
        $ourSyms = & $dumpbin /nologo /symbols "out\x64\mangle6.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Sort-Object -Unique
        $missing = @($clSyms | Where-Object { $ourSyms -notcontains $_ })
        if ($missing.Count -eq 0 -and $clSyms.Count -gt 100) { Write-Host "  ok   sample-mangle: all $($clSyms.Count) decorated names cl uses are spelled identically by the version 6 mangler" }
        else { Write-Host "  FAIL sample-mangle: $($missing.Count) of $($clSyms.Count) decorated names differ from cl's:"; $missing | Select-Object -First 30 | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
        # static locals ("?n@?1??counterA@@YAHXZ@4HA"): the same name and the same linkage, Static unless the
        # function is inline (then External, one COMDAT for every unit); cl's unwind funclets ("?dtor$0@?0??...") are not
        # variables
        $localPattern = '(External|Static)\s+\| (\?[^?@ $]+@\?[0-9A-P]+@?\?\?\S+)'
        $clLocals = @(& $dumpbin /nologo /symbols "out\x64\mangle_cl.obj" | Select-String $localPattern | ForEach-Object { "$($_.Matches[0].Groups[1].Value) $($_.Matches[0].Groups[2].Value)" } | Sort-Object -Unique)
        $ourLocals = @(& $dumpbin /nologo /symbols "out\x64\mangle6.obj" | Select-String $localPattern | ForEach-Object { "$($_.Matches[0].Groups[1].Value) $($_.Matches[0].Groups[2].Value)" } | Sort-Object -Unique)
        $missing = @($clLocals | Where-Object { $ourLocals -notcontains $_ })
        if ($missing.Count -eq 0 -and $clLocals.Count -ge 6) { Write-Host "  ok   sample-mangle: all $($clLocals.Count) static locals have cl's names and linkage" }
        else { Write-Host "  FAIL sample-mangle: $($missing.Count) of $($clLocals.Count) static locals differ from cl's:"; $missing | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
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
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   selfdriver$lvl.exe: all 25 self-hosting checks correct (-O$lvl)" }
        else { Write-Host "  FAIL selfdriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
        # after main: a global and a static local with destructors, destroyed newest first through atexit
        if ($run -match 'destructors at exit: cba') { Write-Host "  ok   selfdriver$lvl.exe: globals and static locals destroyed at exit in reverse order (-O$lvl)" }
        else { Write-Host "  FAIL selfdriver$lvl.exe (-O$lvl): destructors at exit missing or out of order"; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed self-hosting check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 17: version 6 front end (sample-front and MatrixTest.cpp: rejected units; lookup, sizeof and templates against cl) ==="
    $failed = 0
    # units with errors: each must report its diagnostics, exit non-zero and leave no object, listing or map
    # (a stale object of an earlier run is put in place first: it must be removed, not taken for the result)
    $rejects = @(
        @{ unit = "errors"; expect = @("error S103: call to undeclared function 'undeclared_fn'", "error S101: undeclared identifier 'nosuchvar'") },
        @{ unit = "sizeof-incomplete"; expect = @("error S113: sizeof of incomplete class 'Inc'", "error S113: sizeof of incomplete class 'std::", "error S113: sizeof of incomplete class 'Self'", "error S113: sizeof of void") },
        @{ unit = "vbase"; expect = @("error S130: 'VL' has a virtual base class", "error S130: 'VR' has a virtual base class", "error S130: 'VD' has a virtual base class") },
        @{ unit = "vbase-refused"; expect = @("error S132: line 8: cannot convert from 'V' to 'D': 'V' is a virtual base", "error S132: line 9: catching 'V', a virtual base class", "error S132: line 10: throwing a pointer to 'D', a class with virtual bases") },
        @{ unit = "goto-refused"; expect = @("error S132: line 5: goto x jumps past the initialisation of an object with a destructor (ill-formed)", "error S132: line 6: goto y jumps into a try block or a handler (ill-formed)") },
        @{ unit = "covariant-refused"; expect = @("(8): error S131: 'CD::get' overrides 'SB::get' with a covariant return type, and its return conversion goes through a virtual base", "(14): error S131: 'D::g' overrides 'B::g' with a covariant return type, and the slot needs a this adjustment as well as the return adjustment") },
        @{ unit = "goto-handler"; expect = @("error S132: line 5: a goto out of a catch handler ('out')", "error S132: line 6: a goto out of a catch handler ('out')") },
        @{ unit = "new-refused"; expect = @("error S132: line 8: a placement new-expression of 'Q'", "error S132: line 9: new of a multidimensional array of 'S'", "error S132: line 9: delete[] of a multidimensional array of 'S'") },
        @{ unit = "memptr-refused"; expect = @("error S132: line 16: conversion of a pointer to member from 'int W::*' to 'int VW::*' is not supported yet", "error S132: line 17: conversion of a pointer to member from 'int VX::*' to 'int X::*' is not supported yet", "error S132: line 18: conversion of a pointer to member from 'int I1::*' to 'int I2::*', whose inheritance model is unspecified") },
        @{ unit = "ambiguous-base"; expect = @("(8): error S141: 'F': direct base 'V' is inaccessible; already a base of 'W'", "(9): error S141: 'H2': direct base 'V' is inaccessible; already a base of 'WV'", "(10): error S141: 'H5': direct base 'V' is inaccessible; already a base of 'X'", "(11): warning S142: 'G2': base-class 'V' is already a base-class of 'W'", "(12): warning S142: 'H3': base-class 'V' is already a base-class of 'WV'", "(16): warning S142: 'T7': base-class 'V' is already a base-class of 'XV'", "(17): error S141: 'T8': direct base 'V' is inaccessible; already a base of 'XV'") },
        @{ unit = "ambiguous"; expect = @("ambiguous.cpp(12): error S105: cannot initialise 'p1' of type A* from D*", "ambiguous.cpp(13): error S143: ambiguous conversions from 'D*' to 'A*'", "ambiguous.cpp(14): error S143:", "ambiguous.cpp(15): error S144: ambiguous access of 'a' in 'D'", "ambiguous.cpp(16): error S144: ambiguous access of 'f'", "ambiguous.cpp(17): error S105: cannot convert D to the return type A&") }
    )
    foreach ($r in $rejects) {
        $nm = "front-$($r.unit)"
        foreach ($sfx in @("6.obj", "6.asm", "6.map")) { "stale" | Out-File -Encoding ascii "out\x64\$nm$sfx" }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\$($r.unit).cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        $missing = @($r.expect | Where-Object { -not $c6.Contains($_) })
        $left = @(@("6.obj", "6.asm", "6.map") | Where-Object { Test-Path "out\x64\$nm$_" })
        if ($rc -ne 0 -and $missing.Count -eq 0 -and $left.Count -eq 0) { Write-Host "  ok   $($r.unit).cpp: errors reported, exit code $rc, no object, listing or map" }
        else { Write-Host "  FAIL $($r.unit).cpp (exit $rc, missing diagnostics: $($missing -join ' | '), left behind: $($left -join ', ')):"; Write-Host $c6; $failed++ }
    }
    # S130 is a refusal of code generation only: the analysis of the same unit accepts its virtual bases
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $s6 = & $exe --sema6 -I ..\include -o out\x64 -n front-vbase-sema "..\examples\sample-front\vbase.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -eq 0 -and $s6 -match 'sema: 0 error' -and -not $s6.Contains("S130")) { Write-Host "  ok   vbase.cpp: --sema6 accepts the virtual bases (exit 0, no S130)" }
    else { Write-Host "  FAIL vbase.cpp under --sema6 (exit $rc):"; Write-Host $s6; $failed++ }
    # vbase-layout.cpp: the layout of classes with virtual bases (sizes, vbtables and their names) against cl's
    # /d1reportAllClassLayout
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $null = & $exe --sema6 -I ..\include -o out\x64 -n front-vbase-layout "..\examples\sample-front\vbase-layout.cpp" 2>&1
    $rep = & $cl /nologo /c "/Foout\x64\vbase-layout_cl.obj" /d1reportAllClassLayout "..\examples\sample-front\vbase-layout.cpp" 2>&1 | Out-String
    $ErrorActionPreference = $eap
    $want = @([regex]::Matches($rep, '(?m)^class (V\w+)\s+size\((\d+)\)') | ForEach-Object { "$($_.Groups[1].Value) size $($_.Groups[2].Value)" })
    $want += @([regex]::Matches($rep, '(?m)^(V\w+)::\$vbtable@(\w*)@?:\s*\r?\n((?:\s*\d+\s*\|\s*-?\d+.*\r?\n)+)') | ForEach-Object { "$($_.Groups[1].Value) vbtable@$($_.Groups[2].Value):" + ((@([regex]::Matches($_.Groups[3].Value, '\|\s*(-?\d+)')) | ForEach-Object { $_.Groups[1].Value }) -join ',') })
    $got = @()
    $sym = if (Test-Path "out\x64\front-vbase-layout-sym6.txt") { Get-Content "out\x64\front-vbase-layout-sym6.txt" } else { @() }
    foreach ($ln in $sym) {
        if ($ln -notmatch '^struct (V\w+)\b.*?  size (\d+)') { continue }
        $c = $Matches[1]; $got += "$c size $($Matches[2])"
        foreach ($m in [regex]::Matches($ln, 'vbtable(@[\w/]+)?@-?\d+:(\S+)')) { $got += "$c vbtable@$($m.Groups[1].Value.TrimStart('@')):$($m.Groups[2].Value)" }
    }
    $diff = @(Compare-Object ($want | Sort-Object) ($got | Sort-Object))
    if ($want.Count -ge 20 -and $diff.Count -eq 0) { Write-Host "  ok   vbase-layout.cpp: sizes and vbtables of $(($want | Where-Object { $_ -match ' size ' }).Count) classes as cl lays them out" }
    else { Write-Host "  FAIL vbase-layout.cpp layout differs from cl's:"; $diff | ForEach-Object { Write-Host "    $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
    # front.cpp: built by cl (the reference) and by cppparse at -O0 and -O2; the programs must print the same lines
    $clOut = & $cl /nologo /EHsc /W3 "/Foout\x64\" "/Feout\x64\front_cl.exe" "..\examples\sample-front\front.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\front.cpp" }
    $want = & ".\out\x64\front_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "front$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\front.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\front.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo "/Feout\x64\${nm}.exe" "out\x64\${nm}6.obj" "out\x64\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   front.cpp (-O$lvl) prints what cl's build prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL front.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # copies.cpp: copies and temporaries, logged by the constructors and destructors. cl /Od copies a returned named
    # local and /O2 builds it in the result (NRVO): -O0 must print what cl /Od's build prints, -O2 what cl /O2's does
    foreach ($lvl in @(0, 2)) {
        $clOpt = if ($lvl -eq 0) { "/Od" } else { "/O2" }
        $clOut = & $cl /nologo /EHsc /W3 $clOpt "/Foout\x64\" "/Feout\x64\copies_cl$lvl.exe" "..\examples\sample-front\copies.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\copies.cpp" }
        $want = & ".\out\x64\copies_cl$lvl.exe" 2>&1 | Out-String
        $nm = "copies$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\copies.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\copies.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = & $cl /nologo "/Feout\x64\${nm}.exe" "out\x64\${nm}6.obj" "out\x64\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   copies.cpp (-O$lvl) logs the copies and destructions of cl $clOpt ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL copies.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl $clOpt's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # MatrixTest.cpp (Matrix.hpp, Vector.hpp; asserts and streams) against cl: member templates of class templates,
    # injected class names with arguments, a function template declared before its definition. std::cout comes
    # from the libcpp/iostream.cpp objects of test 15
    $clOut = & $cl /nologo /EHsc /W3 /I.. "/Foout\x64\" "/Feout\x64\matrixtest_cl.exe" "..\MatrixTest.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile MatrixTest.cpp" }
    $want = & ".\out\x64\matrixtest_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "matrixtest$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -I .. -o out\x64 -n $nm "..\MatrixTest.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of MatrixTest.cpp at -O$lvl :"; Write-Host ($c6 -split "`n" | Select-Object -First 20); $failed++; continue }
        $clOut = & $cl /nologo "/Feout\x64\${nm}.exe" "out\x64\${nm}6.obj" "out\x64\iostreamL${lvl}6.obj" "out\x64\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   MatrixTest.cpp (-O$lvl) runs and prints what cl's build prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL MatrixTest.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed front-end check(s) failed" }

    Write-Host ""
    Write-Host "=== cppparse: build and all tests succeeded ==="
    Write-Host "Reports: $Sandbox\out\sample\Sample-report.{txt,html,pdf}, Sample-diagram.{svg,dot}, Sample-health.txt"
    $global:LASTEXITCODE = 0
}
finally {
    Pop-Location
}
exit 0
