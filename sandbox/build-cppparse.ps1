# build-cppparse.ps1 - compile cppparse with MSVC (Visual Studio 2026) and run its tests.
#
# Usage (from any directory):
#   powershell -ExecutionPolicy Bypass -File "cpp\sandbox\build-cppparse.ps1"
#
# cmd.exe is disabled by policy on this machine, so the MSVC environment is
# set by hand (see build.ps1). The sources are strict C++98 (long long apart,
# see scripts\strict98.ps1); MSVC's oldest mode is C++14, so they compile unchanged. /W4 /permissive- is used and any
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
#   -Linker ms|gride  the linker of every program the build and the tests link (scripts\toolset.ps1, Link-Program):
#                     Microsoft's link (the default, or the environment variable CPPPARSE_LINKER) or GRIDE's link.
#                     Under gride what is known to differ is printed "not under GRIDE's link: ..." and counted at
#                     the end (docs\design-selfhost.md 4.6); where GRIDE is not found the script exits with 77
param(
    [ValidateSet("Release", "Debug")] [string] $Config = "Release",
    [switch] $NoTests,
    [string] $Exe = "",
    [string] $Linker = ""
)

$ErrorActionPreference = "Stop"

$Sandbox = $PSScriptRoot
$Src     = Split-Path $Sandbox -Parent
# cppparse's source folders (see scripts\toolset.ps1): each is an include folder of the build as well
$SourceDirs = @("driver", "common", "frontend", "middle", "backend", "eh", "cv", "target", "analyzer", "analyzer\report")
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
# the linker setting, Link-Program and Build-Program, and where GRIDE is (toolset.ps1 sets $Exe to its default: the
# one given here is kept)
$exeGiven = $Exe
. "$Sandbox\scripts\toolset.ps1"
$Exe = $exeGiven
Assert-Linker

Write-Host "=== Toolset: MSVC $MsvcVer, Windows SDK $SdkVer ==="
if ($Linker -eq "gride") { Write-Host "=== Linker: GRIDE's link, $GrideLink ===" }

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
        # (Debug: /MTd's objects name the debug C runtime themselves, so the link adds no library of its own)
        $linkFlags = @(); $linkLibs = $null; if ($Config -eq "Debug") { $linkFlags = @("/DEBUG"); $linkLibs = @() }
        $objects = @($sources | ForEach-Object { "$objDir\" + [IO.Path]::GetFileNameWithoutExtension($_) + ".obj" })
        $output = Build-Program (@("/EHsc", "/W4", "/permissive-") + $cflags + $sourceIncludes + @("/Fo$objDir\", "/Fd$objDir\cppparse.pdb") + $sources) $objects $exe $linkFlags $linkLibs | Out-String
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
    # --exclude out: sandbox\out holds every test's outputs and whatever else is kept there (a copied code base
    # under review once made this scan run for hours); only the tracked sources are meant
    & $exe -o "out\folder" -n "cpp-folder" --exclude "out" ".."
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on the folder" }
    $report2 = Get-Content "out\folder\cpp-folder-report.txt" -Raw
    $failed = 0
    foreach ($e in @('class cppparse::Parser', 'class cppparse::CodeBase', 'class Vector', 'class Matrix', 'struct cppparse::ClassInfo', 'folder scan (recursive)', '/sandbox/out (--exclude)')) {
        if ($report2.Contains($e)) { Write-Host "  ok   $e" } else { Write-Host "  FAIL missing: $e"; $failed++ }
    }
    if ($report2.Contains('sandbox/out/')) { Write-Host "  FAIL a file under sandbox\out was scanned"; $failed++ } else { Write-Host "  ok   nothing under sandbox\out scanned" }
    # --exclude on a planted tree: the excluded folder (given in other case, with a trailing separator) and the
    # folders under it are skipped, its sibling is not; without --exclude the same scan finds it
    Remove-Item -Recurse -Force "out\exclude" -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force "out\exclude\src\gen\deep", "out\exclude\src\keep" | Out-Null
    Set-Content -Encoding ascii "out\exclude\src\keep\keep.hpp" "class KeepMe { public: int k; };"
    Set-Content -Encoding ascii "out\exclude\src\gen\drop.hpp" "class DropMe { public: int d; };"
    Set-Content -Encoding ascii "out\exclude\src\gen\deep\deeper.hpp" "class DropMeToo { public: int d; };"
    & $exe -o "out\exclude" -n "with" --text-only --exclude "out/exclude/src/GEN/" "out\exclude\src" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on the --exclude tree" }
    & $exe -o "out\exclude" -n "without" --text-only "out\exclude\src" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on the --exclude tree without --exclude" }
    $with = Get-Content "out\exclude\with-report.txt" -Raw
    $without = Get-Content "out\exclude\without-report.txt" -Raw
    if ($with.Contains('class KeepMe') -and -not $with.Contains('class DropMe') -and $with.Contains('/src/gen (--exclude)')) { Write-Host "  ok   --exclude skips the folder and its subfolders" }
    else { Write-Host "  FAIL --exclude: KeepMe found $($with.Contains('class KeepMe')), DropMe found $($with.Contains('class DropMe')), note $($with.Contains('/src/gen (--exclude)'))"; $failed++ }
    if ($without.Contains('class KeepMe') -and $without.Contains('class DropMe') -and $without.Contains('class DropMeToo')) { Write-Host "  ok   without --exclude the same scan finds all three" }
    else { Write-Host "  FAIL without --exclude a planted class is missing"; $failed++ }
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
    # cl's object of the runtime, or the compiler under test's with CPPPARSE_RT=cppparse (toolset.ps1 Build-Runtime, row P1 stage 3)
$rtOut = Build-Runtime "out\x64\cppparse_rt.obj"
    if ($LASTEXITCODE -ne 0) { Write-Host $rtOut; throw "cppparse_rt.cpp failed to compile" }
    Write-Host "  ok   runtime cppparse_rt.cpp compiled$(if ($env:CPPPARSE_RT -eq 'cppparse') { ' by cppparse' })"
    foreach ($lvl in @(0, 2, 3)) {
        $asmPath = "out\opt\opt$lvl.asm"
        if (-not (Test-Path $asmPath)) { Write-Host "  FAIL missing $asmPath"; $failed++; continue }
        $mlOut = & $ml64 /nologo /c "/Foout\x64\opt$lvl.obj" $asmPath 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL ml64 rejected opt$lvl.asm:"; Write-Host $mlOut; $failed++; continue }
        Write-Host "  ok   ml64 assembled opt$lvl.asm"
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-opt-driver\driver.cpp") @("out\x64\driver.obj", "out\x64\opt$lvl.obj", "out\x64\cppparse_rt.obj") "out\x64\driver$lvl.exe" | Out-String
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-opt-driver\driver.cpp") @("out\x64\driver.obj", "out\x64\$nm.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}driver.exe" | Out-String
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
        # (at least 8 functions with a frame: the comparison saw something; -O3 has 10 since B6 left the float temporaries' functions frameless)
        if ($ua.Count -ge 8 -and $bad.Count -eq 0) { Write-Host "  ok   unwind info of own$lvl.obj matches ml64's for all $($ua.Count) functions with a frame" }
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-opt-driver\driver-native.cpp") @("out\x64\driver-native.obj", "out\x64\msvc.obj", "out\x64\cppparse_rt.obj") "out\x64\nativedriver.exe" | Out-String
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
    # sample-front\alttok.cpp (test 17's program): -E keeps the spelling of the alternative tokens and digraphs and
    # replaces the trigraphs, as cl /EP /permissive- /Zc:trigraphs does (ALTTOK_NO_STDIO leaves <cstdio> and main out,
    # so that only the file's own lines are compared)
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $clPp = & $cl /nologo /EP /permissive- /Zc:trigraphs /DALTTOK_NO_STDIO "..\examples\sample-front\alttok.cpp" 2>$null | Out-String
    $ErrorActionPreference = $eap
    $p = Start-Process -FilePath $exe -ArgumentList '-E','-DALTTOK_NO_STDIO','..\examples\sample-front\alttok.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\par\alttok-out.txt" -RedirectStandardError "$Sandbox\out\par\alttok-diag.txt"
    $rc = $p.ExitCode
    $a = @(& $norm $clPp); $b = @(& $norm (Get-Content "$Sandbox\out\par\alttok-out.txt" -Raw))
    $same = 0; $diff = 0
    for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($i -lt $a.Count -and $i -lt $b.Count -and $a[$i] -eq $b[$i]) { $same++ } else { $diff++ } }
    if ($rc -eq 0 -and $diff -eq 0 -and $same -gt 10 -and ($b -join "`n") -match 'x<:1:>' -and ($b -join "`n") -match 'intarr\[2\]=\{2,3\}') { Write-Host "  ok   cppparse -E matches cl /EP on sample-front\alttok.cpp ($same lines): alternative tokens and digraphs keep their spelling, trigraphs are replaced" }
    else {
        Write-Host "  FAIL -E of sample-front\alttok.cpp differs from cl /EP (exit $rc, $same same, $diff different):"
        $shown = 0
        for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count) -and $shown -lt 10; $i++) { if ($i -ge $a.Count -or $i -ge $b.Count -or $a[$i] -ne $b[$i]) { Write-Host "    [$i] cl: $($a[$i])"; Write-Host "    [$i] us: $($b[$i])"; $shown++ } }
        $failed++
    }
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-opt-driver\driver-native.cpp") @("out\x64\driver-native.obj", $obj, "out\x64\cppparse_rt.obj") "out\x64\v6driver$lvl.exe" | Out-String
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "/I..\examples\sample-align", "..\examples\sample-align-driver\driver-align.cpp") @("out\x64\driver-align.obj", "out\x64\align${lvl}6.obj", "out\x64\cppparse_rt.obj") "out\x64\aligndriver$lvl.exe" | Out-String
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "/I..\examples\sample-layout", "..\examples\sample-layout-driver\driver-layout.cpp") @("out\x64\driver-layout.obj", "out\x64\layout${lvl}6.obj", "out\x64\cppparse_rt.obj") "out\x64\layoutdriver$lvl.exe" | Out-String
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "/I..\examples\sample-abi", "..\examples\sample-abi-driver\driver-abi.cpp") @("out\x64\driver-abi.obj", "out\x64\abi${lvl}6.obj", "out\x64\cppparse_rt.obj") "out\x64\abidriver$lvl.exe" | Out-String
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
            $lo = Link-Program ($objs + @("out\x64\cppparse_rt.obj")) $sexe | Out-String
            if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the statics driver (-O$lvl, $order):"; Write-Host $lo; $failed++; continue }
            $sargs = @(); if ($order -eq "cl-first") { $sargs = @("cl-copies") }
            $run = & ".\$sexe" @sargs 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   staticsdriver$lvl-${order}: shared static locals constructed once from cl and cppparse (-O$lvl)" }
            else { Write-Host "  FAIL staticsdriver$lvl-$order (-O$lvl):"; Write-Host ($run.Split("`n") | Where-Object { $_ -match 'FAIL' }); $failed++ }
        }
    }
    # the COFF writer's diagnostics (review m6): a listing the writer cannot encode names the line, the function and
    # the text of the line; a listing it can encode becomes an object (--compile6 --obj on a .asm input)
    ".code`nPUBLIC bad_f`nbad_f PROC`n        mov eax, 1`n        frob rax, 2`n        ret`nbad_f ENDP`nEND`n" | Out-File -Encoding ascii "out\x64\bad.asm"
    ".code`nPUBLIC good_f`ngood_f PROC`n        mov eax, 1`n        ret`ngood_f ENDP`nEND`n" | Out-File -Encoding ascii "out\x64\good.asm"
    Remove-Item "out\x64\bad6.obj", "out\x64\good6.obj" -ErrorAction SilentlyContinue
    # Start-Process captures stderr reliably (a plain 2> redirection loses it under SilentlyContinue)
    $p = Start-Process -FilePath $exe -ArgumentList '--compile6','-O0','--obj','--msvc-mangling','-q','-o','out\x64','out\x64\bad.asm' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\bad-out.txt" -RedirectStandardError "$Sandbox\out\x64\bad-err.txt"
    $rc1 = $p.ExitCode
    $d1 = "" + (Get-Content "$Sandbox\out\x64\bad-out.txt" -Raw) + (Get-Content "$Sandbox\out\x64\bad-err.txt" -Raw)
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $d2 = & $exe --compile6 -O0 --obj --msvc-mangling -q -o out\x64 out\x64\good.asm 2>&1 | Out-String
    $rc2 = $LASTEXITCODE
    $ErrorActionPreference = $eap
    if ($rc1 -ne 0 -and -not (Test-Path "out\x64\bad6.obj") -and $d1.Contains("line 5 in bad_f: unknown instruction frob [frob rax, 2]") -and $d1 -match "bad6.obj not written: 1 line\(s\) of") { Write-Host "  ok   COFF writer: a bad listing names the line, the function and quotes the line (exit $rc1)" }
    else { Write-Host "  FAIL COFF writer diagnostics (exit $rc1):"; Write-Host $d1; $failed++ }
    if ($rc2 -eq 0 -and (Test-Path "out\x64\good6.obj")) { Write-Host "  ok   COFF writer: a listing in cppparse's MASM subset is assembled into good6.obj" }
    else { Write-Host "  FAIL COFF writer on good.asm (exit $rc2):"; Write-Host $d2; $failed++ }
    # the driver (review m2): two inputs named after their stems, -q prints nothing but diagnostics, -o to a
    # missing directory fails with exit code 1, a response file holds several arguments on one line
    New-Item -ItemType Directory -Force "out\x64\two" | Out-Null
    "int twoA(int x) { return x + 1; }`n" | Out-File -Encoding ascii "out\x64\two_a.cpp"
    "int twoB(int x) { return x + 2; }`n" | Out-File -Encoding ascii "out\x64\two_b.cpp"
    "int twoC(int x) { int unused; return x + 3; }`n" | Out-File -Encoding ascii "out\x64\two_c.cpp"
    Remove-Item "out\x64\two\*" -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath $exe -ArgumentList '--compile6','-O0','--obj','--msvc-mangling','-q','-o','out\x64\two','out\x64\two_a.cpp','out\x64\two_b.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\two-out.txt" -RedirectStandardError "$Sandbox\out\x64\two-err.txt"
    $rc1 = $p.ExitCode; $d1 = "" + (Get-Content "$Sandbox\out\x64\two-out.txt" -Raw) + (Get-Content "$Sandbox\out\x64\two-err.txt" -Raw)
    $p = Start-Process -FilePath $exe -ArgumentList '--compile6','-O0','--obj','--msvc-mangling','-q','-o','out\x64\nodir\deeper','-n','x','out\x64\two_a.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\nodir-out.txt" -RedirectStandardError "$Sandbox\out\x64\nodir-err.txt"
    $rc2 = $p.ExitCode; $d2 = "" + (Get-Content "$Sandbox\out\x64\nodir-out.txt" -Raw) + (Get-Content "$Sandbox\out\x64\nodir-err.txt" -Raw)
    '# written by test 13' + "`n" + '-o out\x64\two -n fromresp "out\x64\two_a.cpp"' + "`n" + '  out\x64\two_b.cpp' | Out-File -Encoding ascii "out\x64\resp.txt"
    $p = Start-Process -FilePath $exe -ArgumentList '--compile6','-O0','--obj','--msvc-mangling','-q','@out\x64\resp.txt' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\resp-out.txt" -RedirectStandardError "$Sandbox\out\x64\resp-err.txt"
    $rc3 = $p.ExitCode; $d3 = "" + (Get-Content "$Sandbox\out\x64\resp-out.txt" -Raw) + (Get-Content "$Sandbox\out\x64\resp-err.txt" -Raw)
    # a warning is a diagnostic: printed under -q, and nothing else is
    $p = Start-Process -FilePath $exe -ArgumentList '--compile6','-O0','--obj','--msvc-mangling','-q','-o','out\x64\two','out\x64\two_c.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\twoc-out.txt" -RedirectStandardError "$Sandbox\out\x64\twoc-err.txt"
    $rc4 = $p.ExitCode; $d4 = "" + (Get-Content "$Sandbox\out\x64\twoc-out.txt" -Raw) + (Get-Content "$Sandbox\out\x64\twoc-err.txt" -Raw)
    if ($rc1 -eq 0 -and (Test-Path "out\x64\two\two_a6.obj") -and (Test-Path "out\x64\two\two_b6.obj") -and $d1.Trim() -eq "") { Write-Host "  ok   driver: two inputs named two_a6.obj and two_b6.obj, -q prints nothing" }
    else { Write-Host "  FAIL driver: two inputs (exit $rc1):"; Write-Host $d1; $failed++ }
    if ($rc2 -ne 0 -and $d2 -match "error: cannot write ") { Write-Host "  ok   driver: -o to a missing directory reports it and exits $rc2" }
    else { Write-Host "  FAIL driver: -o to a missing directory (exit $rc2):"; Write-Host $d2; $failed++ }
    if ($rc3 -eq 0 -and (Test-Path "out\x64\two\fromresp-06.obj") -and (Test-Path "out\x64\two\fromresp-16.obj")) { Write-Host "  ok   driver: a response file with -o, -n and a quoted input on one line, a comment, two inputs numbered" }
    else { Write-Host "  FAIL driver: response file (exit $rc3):"; Write-Host $d3; $failed++ }
    if ($rc4 -eq 0 -and $d4 -match "warning S154: 'unused'" -and ($d4 -split "`n" | Where-Object { $_.Trim() -ne "" }).Count -eq 1) { Write-Host "  ok   driver: -q prints the warning and nothing else" }
    else { Write-Host "  FAIL driver: -q with a warning (exit $rc4):"; Write-Host $d4; $failed++ }
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
        $clOut = Build-Program @("/EHsc", "/W3", "/DLIFE_NRVO=$([int]($lvl -ge 1))", "/Foout\x64\", "..\examples\sample-life-driver\driver-life.cpp") @("out\x64\driver-life.obj", $obj, "out\x64\cppparse_rt.obj") "out\x64\lifedriver$lvl.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the lifetime driver:"; Write-Host $clOut; $failed++; continue }
        $run = & ".\out\x64\lifedriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   lifedriver$lvl.exe: all 24 lifetime checks correct (-O$lvl)" }
        else { Write-Host "  FAIL lifedriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
    }
    $ir = Get-Content "out\x64\life0-ir6.txt" -Raw
    # (A3: every object with a destructor has a cleanup region whose landing pad destroys it between cppparse_unwind_enter and
    # leave; the catch-all unwind handler with the flags remains for objects built on one path only)
    $pads = ([regex]::Matches($ir, '(?m)^B\d+:\s+; cleanup\s')).Count
    # (cl's model, harness.ps1 -Eh cl: a pad is a funclet that returns to the runtime, "throw @cleanupret" in the IR, with no enter and leave)
    if ($ir -match 'call Tracker::~Tracker\(\)' -and $ir -match 'call Text::Text\(const Text&\)' -and $pads -gt 0 -and ($ir -match 'call cppparse_unwind_enter' -or $ir -match '@cleanupret')) { Write-Host "  ok   IR shows destructor calls, copy-constructor calls and the cleanup pads ($pads)" }
    else { Write-Host "  FAIL IR lacks destructor / copy-constructor calls or cleanup pads ($pads)"; $failed++ }
    if ($failed -ne 0) { throw "$failed object lifetime check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 15: version 6 standard library (include/: string, vector, map, set, algorithm, streams) ==="
    $failed = 0
    foreach ($lvl in @(0, 2)) {
        $objs = @()
        foreach ($unit in @("examples\sample-lib\lib1.cpp", "examples\sample-lib\lib2.cpp", "examples\sample-lib\lib3.cpp", "examples\sample-lib\lib4.cpp", "libcpp\iostream.cpp")) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($unit) + "L$lvl"
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $name "..\$unit" 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -ne 0 -or -not (Test-Path "out\x64\${name}6.obj")) { Write-Host "  FAIL --compile6 of $unit at -O$lvl :"; Write-Host $c6; $failed++; continue }
            if ($c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL $unit has semantic errors:"; Write-Host $c6; $failed++; continue }
            $objs += "out\x64\${name}6.obj"
        }
        if ($objs.Count -ne 5) { continue }
        Write-Host "  ok   the library samples and libcpp/iostream.cpp compile without errors (-O$lvl)"
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-lib-driver\driver-lib.cpp") (@("out\x64\driver-lib.obj") + $objs + @("out\x64\cppparse_rt.obj")) "out\x64\libdriver$lvl.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of the library driver (COMDAT objects from four units):"; Write-Host $clOut; $failed++; continue }
        Write-Host "  ok   four objects link: inline functions and template instantiations are COMDAT"
        $run = & ".\out\x64\libdriver$lvl.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') { Write-Host "  ok   libdriver$lvl.exe: all 27 library checks correct (-O$lvl)" }
        else { Write-Host "  FAIL libdriver$lvl.exe (-O$lvl):"; Write-Host $run; $failed++ }
        if ($run -match 'cout from cppparse') { Write-Host "  ok   std::cout is a dynamically initialised global of libcpp/iostream.cpp (.CRT`$XCU)" }
        else { Write-Host "  FAIL std::cout produced no output"; $failed++ }
    }
    # the explicit instantiation definitions of lib1.cpp (LinkInst<int>, two members of LinkInst<double>, linkTwice): the
    # functions they define in cppparse's object are the COMDATs cl's build of the unit defines, so a cl-built lib2 links too
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /X /I..\include "/Foout\x64\lib1_cl.obj" "..\examples\sample-lib\lib1.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-lib\lib1.cpp" }
    $definedInst = 'SECT\S+\s+notype \(\)\s+External\s+\| (\?\S*(LinkInst|linkTwice)\S*)'
    $clInst = @(& $dumpbin /nologo /symbols "out\x64\lib1_cl.obj" | Select-String $definedInst | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
    $ourInst = @(& $dumpbin /nologo /symbols "out\x64\lib1L06.obj" | Select-String $definedInst | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
    $missing = @($clInst | Where-Object { $ourInst -notcontains $_ })
    if ($missing.Count -eq 0 -and $clInst.Count -ge 7) { Write-Host "  ok   lib1.cpp: the $($clInst.Count) functions its explicit instantiations define are the COMDATs cl defines for them" }
    else { Write-Host "  FAIL lib1.cpp explicit instantiations: $($missing.Count) of $($clInst.Count) functions cl defines are missing:"; $missing | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
    # the version 6 mangler against cl on library types: every decorated name cl defines or references must be
    # spelled identically by cppparse (cl's own helpers are left out: deleting destructors, RTTI, sized delete, and its string literal names;
    # so is the placement operator new, which cl /Od calls and cppparse does not: it builds the object at the address)
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /X /I..\include "/Foout\x64\mangle_cl.obj" "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-mangle\mangle.cpp" }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n mangle "..\examples\sample-mangle\mangle.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0 -or -not (Test-Path "out\x64\mangle6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL --compile6 of sample-mangle:"; Write-Host $c6; $failed++ }
    else {
        $clSyms = & $dumpbin /nologo /symbols "out\x64\mangle_cl.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Where-Object { $_ -notmatch '^\?\?_[GER7C]|^\?\?3@YAXPEAX_K@Z$|^\?\?2@YAPEAX_KPEAX@Z$' } | Sort-Object -Unique
        $ourSyms = & $dumpbin /nologo /symbols "out\x64\mangle6.obj" | Select-String 'External\s+\| \?' | ForEach-Object { ($_.Line -replace '^.*\| ', '') -replace ' \(.*$', '' } | Sort-Object -Unique
        $missing = @($clSyms | Where-Object { $ourSyms -notcontains $_ })
        if ($missing.Count -eq 0 -and $clSyms.Count -gt 100) { Write-Host "  ok   sample-mangle: all $($clSyms.Count) decorated names cl uses are spelled identically by the version 6 mangler" }
        else { Write-Host "  FAIL sample-mangle: $($missing.Count) of $($clSyms.Count) decorated names differ from cl's:"; $missing | Select-Object -First 30 | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
        # the vftables' names (B11: for the base a table was inherited through where two would be named alike): every
        # vftable cl's object defines (COMDAT externals) has its name in cppparse's
        $vtPattern = '\| (\?\?_7\S+)'
        $clVt = @(& $dumpbin /nologo /symbols "out\x64\mangle_cl.obj" | Select-String "SECT\S+\s+notype\s+External\s+$vtPattern" | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
        $ourVt = @(& $dumpbin /nologo /symbols "out\x64\mangle6.obj" | Select-String "SECT\S+\s+notype\s+(External|Static)\s+$vtPattern" | ForEach-Object { $_.Matches[0].Groups[2].Value } | Sort-Object -Unique)
        $missing = @($clVt | Where-Object { $ourVt -notcontains $_ })
        if ($missing.Count -eq 0 -and $clVt.Count -ge 20) { Write-Host "  ok   sample-mangle: all $($clVt.Count) vftables cl defines are named alike" }
        else { Write-Host "  FAIL sample-mangle: $($missing.Count) of $($clVt.Count) vftable names differ from cl's:"; $missing | ForEach-Object { Write-Host "    cl: $_" }; $failed++ }
        # and none that cl does not: the vftables of the classes whose constructors or destructors the unit emits, not of
        # every polymorphic class it names (N3)
        $extra = @($ourVt | Where-Object { $clVt -notcontains $_ })
        if ($extra.Count -eq 0) { Write-Host "  ok   sample-mangle: cppparse defines no vftable cl does not define" }
        else { Write-Host "  FAIL sample-mangle: $($extra.Count) vftables cl does not define:"; $extra | ForEach-Object { Write-Host "    cppparse: $_" }; $failed++ }
        # and read-only, as cl keeps them (static symbols: the library's classes share names with the MSVC runtime's)
        $writable = @((& $dumpbin /nologo /headers "out\x64\mangle6.obj" | Out-String) -split 'SECTION HEADER' | Where-Object { $_ -match '\(\?\?_7' -and $_ -match '\bWrite\b' })
        $readOnly = @((& $dumpbin /nologo /headers "out\x64\mangle6.obj" | Out-String) -split 'SECTION HEADER' | Where-Object { $_ -match '\(\?\?_7' -and $_ -match 'Read Only' })
        if ($writable.Count -eq 0 -and $readOnly.Count -eq $ourVt.Count) { Write-Host "  ok   sample-mangle: the $($ourVt.Count) vftables are in read-only COMDAT sections, as cl's" }
        else { Write-Host "  FAIL sample-mangle: of $($ourVt.Count) vftables $($readOnly.Count) read-only, $($writable.Count) in writable sections"; $failed++ }
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
    # libnames.cpp (C3): what a unit defines with library class templates in its types (std::string, wstring,
    # char_traits, allocator) is named by cppparse with include/ as cl names it with its own headers
    $clOut = & $cl /nologo /c /EHsc /W3 /Od "/Foout\x64\libnames_cl.obj" "..\examples\sample-mangle\libnames.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-mangle\libnames.cpp" }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n libnames "..\examples\sample-mangle\libnames.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -ne 0 -or -not (Test-Path "out\x64\libnames6.obj") -or $c6 -notmatch 'sema: 0 error') { Write-Host "  FAIL --compile6 of sample-mangle\libnames.cpp:"; Write-Host $c6; $failed++ }
    else {
        $namesPattern = 'SECT\S+\s+\S+(?: \(\))?\s+External\s+\| (\?\S*names@@\S*)'
        $clNames = @(& $dumpbin /nologo /symbols "out\x64\libnames_cl.obj" | Select-String $namesPattern | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
        $ourNames = @(& $dumpbin /nologo /symbols "out\x64\libnames6.obj" | Select-String $namesPattern | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
        $diff = @(Compare-Object $clNames $ourNames)
        if ($diff.Count -eq 0 -and $clNames.Count -ge 23) { Write-Host "  ok   libnames.cpp: the $($clNames.Count) names it defines over the strings, the streams, complex, valarray, locale and strstream are cl's (its own headers)" }
        else { Write-Host "  FAIL libnames.cpp: names differ from cl's (its own headers):"; $diff | ForEach-Object { Write-Host "    $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
        # the standard stream objects it refers to (defined in libcpp/iostream.cpp) have cl's names too
        $streamPattern = 'UNDEF\s+notype\s+External\s+\| (\?w?c(in|out|err|log)@std@@\S*)'
        $clStreams = @(& $dumpbin /nologo /symbols "out\x64\libnames_cl.obj" | Select-String $streamPattern | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
        $ourStreams = @(& $dumpbin /nologo /symbols "out\x64\libnames6.obj" | Select-String $streamPattern | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
        $diff = @(Compare-Object $clStreams $ourStreams)
        if ($diff.Count -eq 0 -and $clStreams.Count -eq 8) { Write-Host "  ok   libnames.cpp: cin, cout, cerr, clog and their wide forms are named as cl names them" }
        else { Write-Host "  FAIL libnames.cpp: the standard streams' names differ from cl's:"; $diff | ForEach-Object { Write-Host "    $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
    }
    # the library programs (C3): each built by cl with its own headers, the reference, and by cppparse with include/ at
    # -O0 and -O2, linked with the libcpp/iostream.cpp objects above; the programs must print the same. threads.cpp runs the
    # streams, strings and locales on eight threads at once (the locales' shared reference counts); vector.cpp counts the
    # constructions, copies, assignments and destructions <vector> makes (row P9); newhandler.cpp installs new-handlers
    # and asks for storage no allocation function can give (row G11); replacenew.cpp, replacepart.cpp and replacearray.cpp
    # define allocation functions of their own, which replace libcpp's when the program is linked: operator new and
    # operator delete, operator new alone, the array forms alone (row G17); facets.cpp uses codecvt and the time, monetary
    # and messages facets of the classic locale (row G22); namedlocale.cpp the locales "de-DE", "German_Germany.1252",
    # "en-US" and "French_France.1252" facet by facet, the locales made of two, locale::global and eight threads making
    # locales at once (row G28; a name the C runtime of the machine lacks is reported and left out by both builds);
    # ranges.cpp gives the range members of the containers and of basic_string every kind of iterator, input iterators
    # among them, and counts the copies and the allocations a forward range costs (row G31)
    foreach ($prog in @("strings", "wstreams", "streambuf", "complex", "valarray", "strstream", "locale", "threads", "buffers", "complexf", "numfacets", "bigfile", "cheaders", "vector", "newhandler", "replacenew", "replacepart", "replacearray", "facets", "namedlocale", "ranges")) {
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-lib\$prog.cpp") @("out\x64\$prog.obj") "out\x64\${prog}_cl.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-lib\$prog.cpp" }
        $want = & ".\out\x64\${prog}_cl.exe" 2>&1 | Out-String
        foreach ($lvl in @(0, 2)) {
            $nm = "$prog$lvl"
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-lib\$prog.cpp" 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-lib\$prog.cpp at -O$lvl :"; Write-Host ($c6 -split "`n" | Select-Object -First 20); $failed++; continue }
            $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\iostreamL${lvl}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
            if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
            $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   $prog.cpp (-O$lvl) prints what cl's build with its own headers prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
            else { Write-Host "  FAIL $prog.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
        }
    }
    # namedlocale.cpp with the argument own prints what cppparse's library does on purpose where cl's does otherwise
    # (row G28), so these lines are checked for cppparse's builds alone: a German date read with its dots, a range
    # with a null character compared beyond it, the C runtime's classes of wide characters, a bad name refused by a
    # facet and whatever the categories, a locale made of one without a name, the C locale that global sets, and
    # the C locale of the other threads left alone while eight threads make locales
    $ownWant = @(
        "own: get_date of %x 98-2-3 fail 0, wide 98-2-3 fail 0",
        "own: collate with a null character -1 1, by the keys -1",
        "own: wide print of a tab 0, of U+00A0 0",
        "own: bad names throw 2",
        "own: (a locale without a name, de-DE, numeric) is named *",
        "own: global of (classic, de-DE, numeric) makes the C locale de-DE",
        "own: 800 locales made on 8 threads, the C locale of a thread disturbed 0 times")
    foreach ($lvl in @(0, 2)) {
        if (-not (Test-Path ".\out\x64\namedlocale$lvl.exe")) { continue }
        $own = @(& ".\out\x64\namedlocale$lvl.exe" own 2>&1 | ForEach-Object { "$_".TrimEnd() } | Where-Object { $_ })
        if ($LASTEXITCODE -eq 0 -and $own.Count -eq 1 -and $own[0] -match '^own: the C runtime has no locale ') { Write-Host "  ok   namedlocale.cpp (-O$lvl) own part left out: $($own[0])" }
        elseif ($LASTEXITCODE -eq 0 -and ($own -join "`n") -ceq ($ownWant -join "`n")) { Write-Host "  ok   namedlocale.cpp (-O$lvl) own: the $($ownWant.Count) lines where the library follows C++98 and not cl's library" }
        else { Write-Host "  FAIL namedlocale.cpp (-O$lvl, exit $LASTEXITCODE) own part:"; $own | ForEach-Object { Write-Host "    $_" }; $failed++ }
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
        $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-self-driver\driver-self.cpp") @("out\x64\driver-self.obj", $obj, "out\x64\cppparse_rt.obj") "out\x64\selfdriver$lvl.exe" | Out-String
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
        @{ unit = "vbase-refused"; expect = @("error S132: line 9: cannot convert from 'V' to 'D': 'V' is a virtual base") },
        @{ unit = "goto-refused"; expect = @("error S132: line 5: goto x jumps past the initialisation of an object with a destructor (ill-formed)", "error S132: line 6: goto y jumps into a try block or a handler (ill-formed)") },
        @{ unit = "new-refused"; expect = @("new-refused.cpp(11): error S107: no matching function for call to operator new(unsigned long long, char*) among 1 candidate(s)", "new-refused.cpp(12): error S107: no matching function for call to operator new[](unsigned long long, char*) among 1 candidate(s)", "new-refused.cpp(13): error S107: no matching function for call to operator new[](unsigned long long, Arena) among 3 candidate(s)", "new-refused.cpp(15): error S107: no matching function for call to operator new(unsigned long long) among 1 candidate(s)", "new-refused.cpp(16): error S107: no matching function for call to operator new[](unsigned long long) among 1 candidate(s)"); errors = 5 },
        @{ unit = "memptr-refused"; expect = @("error S132: line 16: conversion of a pointer to member from 'int V1::*' to 'int DV::*' through a virtual base (ill-formed in C++98, 4.11/2; cl's extension)", "error S132: line 17: conversion of a pointer to member from 'int() V1::*' to 'int() DV::*' through a virtual base", "error S132: line 18: conversion of a pointer to member from 'int I1::*' to 'int I2::*', whose inheritance model is unspecified, through a virtual base"); errors = 3 },
        @{ unit = "ambiguous-base"; expect = @("(8): error S141: 'F': direct base 'V' is inaccessible; already a base of 'W'", "(9): error S141: 'H2': direct base 'V' is inaccessible; already a base of 'WV'", "(10): error S141: 'H5': direct base 'V' is inaccessible; already a base of 'X'", "(11): warning S142: 'G2': base-class 'V' is already a base-class of 'W'", "(12): warning S142: 'H3': base-class 'V' is already a base-class of 'WV'", "(16): warning S142: 'T7': base-class 'V' is already a base-class of 'XV'", "(17): error S141: 'T8': direct base 'V' is inaccessible; already a base of 'XV'") },
        @{ unit = "cstrovl-refused"; expect = @("cstrovl-refused.cpp(9): error S105: cannot initialise 'bad' of type char* from const char*", "cstrovl-refused.cpp(10): error S105: cannot initialise 'bad2' of type wchar_t* from const wchar_t*", "cstrovl-refused.cpp(11): error S105: cannot initialise 'bad3' of type void* from const void*"); errors = 3 },
        @{ unit = "explspec-refused"; expect = @("explspec-refused.cpp(6): error S110: redefinition of class 'Box<char>'"); errors = 1 },
        @{ unit = "explspec-used"; expect = @("explspec-used.cpp(23): error S110: explicit specialisation of 'Obj<int>' after its use: it has already been instantiated (cl C2908)", "explspec-used.cpp(24): error S110: explicit specialisation of 'InBody<int>' after its use in 'inBody'", "explspec-used.cpp(25): error S110: explicit specialisation of 'Sized<int>' after its use", "explspec-used.cpp(26): error S110: explicit specialisation of 'Base<int>' after its use", "explspec-used.cpp(27): error S110: explicit specialisation of 'Mem<int>::f' after its use in 'callsMember'", "explspec-used.cpp(28): error S110: explicit specialisation of 'Mem<char>::s' after its use in 'readsStatic'", "explspec-used.cpp(29): error S110: explicit specialisation of 'fn<int>' after its use in 'callsFn'", "explspec-used.cpp(30): error S110: explicit specialisation of 'addr<long>' after its use"); errors = 8 },
        @{ unit = "access-refused"; expect = @("access-refused.cpp(17): error S150: 'P::secret': cannot access private member declared in class 'P' (cl C2248)", "access-refused.cpp(20): error S150: 'B::x': cannot access protected member declared in class 'B' (cl C2248)", "access-refused.cpp(21): error S150: 'B::x'", "access-refused.cpp(22): error S150: 'B::f'", "access-refused.cpp(23): error S150: 'B::x'", "access-refused.cpp(24): error S150: 'B::f'", "access-refused.cpp(25): error S150: 'B::B'", "access-refused.cpp(26): error S150: 'B::B'", "access-refused.cpp(28): error S150: 'B::x'", "access-refused.cpp(30): error S150: 'B::x'", "access-refused.cpp(31): error S150: 'B::x'", "access-refused.cpp(46): error S150: 'O::operator+': cannot access private member declared in class 'O' (cl C2248)", "access-refused.cpp(47): error S150: 'O::operator=='", "access-refused.cpp(48): error S150: 'O::operator[]'", "access-refused.cpp(49): error S150: 'O::operator++'", "access-refused.cpp(50): error S150: 'O::operator++'", "access-refused.cpp(51): error S150: 'O::operator->'", "access-refused.cpp(52): error S150: 'O::operator!'", "access-refused.cpp(53): error S150: 'O::operator int'", "access-refused.cpp(54): error S150: 'O::operator int'", "access-refused.cpp(55): error S150: 'O::operator int'", "access-refused.cpp(56): error S150: 'O::operator new'"); errors = 22 },
        @{ unit = "access-refused2"; expect = @("access-refused2.cpp(6): error S150: 'T4::T': cannot access protected member declared in class 'T4' (cl C2248)", "access-refused2.cpp(7): error S150: 'T4::In'", "access-refused2.cpp(8): error S150: 'T4::PT': cannot access private member", "access-refused2.cpp(9): error S150: 'T4::E'", "access-refused2.cpp(10): error S150: 'T4::PT'", "access-refused2.cpp(11): error S150: 'T4::In'", "access-refused2.cpp(12): error S150: 'T4::In'", "access-refused2.cpp(13): error S150: 'T4::T'", "access-refused2.cpp(14): error S150: 'T4::In'", "access-refused2.cpp(17): error S150: 'P2::~P2': cannot access private member declared in class 'P2' (cl C2248)", "access-refused2.cpp(18): error S150: 'P2::~P2'", "access-refused2.cpp(19): error S150: 'Q2::~Q2': cannot access protected member", "access-refused2.cpp(20): error S150: 'P2::~P2'", "access-refused2.cpp(21): error S150: 'P2::~P2'", "access-refused2.cpp(22): error S150: 'Q2::~Q2'", "access-refused2.cpp(23): error S150: 'P2::~P2'", "access-refused2.cpp(24): error S150: 'P2::~P2'", "access-refused2.cpp(26): error S150: 'F5::secret': cannot access private member declared in class 'F5' (cl C2248)"); errors = 18 },
        @{ unit = "access-refused3"; expect = @("access-refused3.cpp(9): error S150: 'B1::x': not accessible here: 'B1' is reached through a private or protected base class (cl C2247)", "access-refused3.cpp(10): error S150: 'B1::f': not accessible here", "access-refused3.cpp(11): error S150: conversion from 'D1' to its base class 'B1': the base class is not accessible here (cl C2243)", "access-refused3.cpp(12): error S150: 'B1::x': not accessible here", "access-refused3.cpp(13): error S150: 'B1::x': not accessible here", "access-refused3.cpp(14): error S150: 'B1::s': not accessible here", "access-refused3.cpp(15): error S150: conversion from 'D1' to its base class 'B1'", "access-refused3.cpp(16): error S150: 'B1::sf': not accessible here", "access-refused3.cpp(17): error S150: conversion from 'D1' to its base class 'B1'"); errors = 9 },
        @{ unit = "access-refused4"; expect = @("access-refused4.cpp(10): error S150: 'OD::operator delete': cannot access private member declared in class 'OD' (cl C2248)", "access-refused4.cpp(11): error S150: 'OD::operator delete[]': cannot access private member", "access-refused4.cpp(12): error S150: 'OD::operator delete': cannot access private member declared in class 'OD' (cl C2248)", "access-refused4.cpp(13): error S150: 'OD::operator delete[]': cannot access private member", "access-refused4.cpp(14): error S150: 'ODP::operator delete': cannot access protected member declared in class 'ODP' (cl C2248)", "access-refused4.cpp(17): error S150: static_cast between 'D2' and its base class 'B2': the base class is not accessible here (cl C2243)", "access-refused4.cpp(18): error S150: static_cast between 'D2' and its base class 'B2': the base class is not accessible here (cl C2243)", "access-refused4.cpp(19): error S150: static_cast between 'D2' and its base class 'B2': the base class is not accessible here (cl C2243)", "access-refused4.cpp(20): error S150: static_cast between 'D2' and its base class 'B2': the base class is not accessible here (cl C2243)", "access-refused4.cpp(24): error S150: 'B3::T': not accessible here: 'B3' is reached through a private or protected base class (cl C2247)", "access-refused4.cpp(25): error S150: 'B3::N': not accessible here", "access-refused4.cpp(26): error S150: 'B3::T': not accessible here: 'B3' is reached through a private or protected base class (cl C2247)", "access-refused4.cpp(27): error S150: 'B3::N': not accessible here", "access-refused4.cpp(28): error S150: 'B3::T': not accessible here: 'B3' is reached through a private or protected base class (cl C2247)", "access-refused4.cpp(30): error S150: 'A4::Priv': cannot access private member declared in class 'A4' (cl C2248)", "access-refused4.cpp(31): error S150: 'A4::Priv': cannot access private member declared in class 'A4' (cl C2248)", "access-refused4.cpp(32): error S150: 'A4::Priv': cannot access private member declared in class 'A4' (cl C2248)", "access-refused4.cpp(36): error S150: 'P6::~P6': cannot access private member declared in class 'P6' (cl C2248)", "access-refused4.cpp(37): error S150: 'P6::~P6': cannot access private member declared in class 'P6' (cl C2248)", "access-refused4.cpp(38): error S150: 'P6::~P6': cannot access private member declared in class 'P6' (cl C2248)", "access-refused4.cpp(44): error S150: 'H7': its implicit destructor cannot access the private destructor of 'M7' (cl C2248)", "access-refused4.cpp(45): error S150: 'HB7': its implicit destructor cannot access the private destructor of 'M7'", "access-refused4.cpp(46): error S150: 'H7': its implicit destructor cannot access the private destructor of 'M7'", "access-refused4.cpp(47): error S150: 'T7c::T7c': cannot access private member declared in class 'T7c' (cl C2248)", "access-refused4.cpp(48): error S150: 'CT8<int>::secret': cannot access private member declared in class 'CT8<int>' (cl C2248)", "access-refused4.cpp(50): error S103: call to undeclared function 'peek10'", "access-refused4.cpp(51): error S103: call to undeclared function 'plain10'"); diags = 27 },
        @{ unit = "friend-refused"; expect = @("friend-refused.cpp(7): error S116: friend 'N::nothing': no function of this name and signature is declared in 'N' (cl C2039)", "friend-refused.cpp(8): error S150: 'C1::s': cannot access private member declared in class 'C1' (cl C2248)", "friend-refused.cpp(9): error S116: friend 'N::f': no function of this name and signature is declared in 'N'", "friend-refused.cpp(10): error S150: 'C3::s'", "friend-refused.cpp(11): error S116: friend '::nothing8': no function of this name and signature is declared in the global namespace (cl C2039)", "friend-refused.cpp(12): error S150: 'C8::s'", "friend-refused.cpp(14): error S150: 'C5::s'", "friend-refused.cpp(16): error S150: 'C4::s'"); errors = 8 },
        @{ unit = "handler-refused"; expect = @("handler-refused.cpp(5): error S161: a handler after 'catch (...)' on line 4: '...' must be the last handler of its try block (C++98 15.3/6; cl C2311)", "handler-refused.cpp(8): error S161: a handler after 'catch (...)' on line 7", "handler-refused.cpp(11): error S161: a handler after 'catch (...)' on line 10",
            "handler-refused.cpp(17): error S163: 'Inc*': a handler cannot catch a pointer to the incomplete type 'Inc' (C++98 15.3/1; cl accepts it)", "handler-refused.cpp(18): error S163: 'const Inc* const': a handler cannot catch a pointer to the incomplete type 'const Inc'", "handler-refused.cpp(19): error S163: 'Inc*': a handler cannot catch a pointer to",
            "handler-refused.cpp(22): error S163: 'Inc&': a handler cannot catch a reference to the incomplete type 'Inc' (C++98 15.3/1; cl C2316)", "handler-refused.cpp(23): error S163: 'const IncU&': a handler cannot catch a reference to the incomplete type 'const IncU'",
            "handler-refused.cpp(26): error S163: 'Inc': a handler cannot catch the incomplete type 'Inc' (C++98 15.3/1; cl C2687)", "handler-refused.cpp(29): error S163: 'void': a handler cannot catch the incomplete type 'void' (C++98 15.3/1; cl C2687)",
            "handler-refused.cpp(32): error S163:", "handler-refused.cpp(33): error S163:", "handler-refused.cpp(36): error S163: 'Only<int>*': a handler cannot catch a pointer to the incomplete type 'Only<int>'", "handler-refused.cpp(39): error S163: 'Inc*': a handler cannot catch a pointer to the incomplete type 'Inc'"); errors = 14 },
        @{ unit = "typeid-refused"; expect = @("typeid-refused.cpp(4): error S146: typeid: this 'std::type_info' is the program's own, not the one <typeinfo> defines: use of undefined type 'type_info' (cl C2027)"); errors = 1 },
        @{ unit = "qualconv-refused"; expect = @("qualconv-refused.cpp(6): error S105: cannot initialise 'bad' of type const int** from int**", "qualconv-refused.cpp(7): error S105: cannot initialise 'bad2' of type int* const** from int***", "qualconv-refused.cpp(8): error S105: cannot initialise 'bad3'"); errors = 3 },
        @{ unit = "wide-refused"; expect = @("wide-refused.cpp(3): error S114: array bound 4294967296: an array must not exceed 0x7fffffff bytes (cl C2148)", "wide-refused.cpp(5): error S114: array bound 1099511627776: an array must not exceed 0x7fffffff bytes (cl C2148)"); errors = 2 },
        @{ unit = "inftempl"; expect = @("error S148: template instantiation depth exceeds 1000 instantiating 'Inf<int*") },
        @{ unit = "ucn-refused"; expect = @("ucn-refused.cpp(4): error: '\u0041': a universal-character-name cannot designate a character in the basic character set", "ucn-refused.cpp(5): error: '\u001f': a universal-character-name specifies a control character", "ucn-refused.cpp(6): error: '\u0085': a universal-character-name specifies a control character", "ucn-refused.cpp(7): error: '\u00d7': this character is not allowed in an identifier (C++98 Annex E; cl C3872)", "ucn-refused.cpp(8): error: '\u2030': this character is not allowed in an identifier", "ucn-refused.cpp(9): error: '\u0041': a universal-character-name cannot designate a character in the basic character set", "ucn-refused.cpp(10): error: '\u001f': a universal-character-name specifies a control character", "ucn-refused.cpp(11): error: '\ud800': a universal-character-name specifies an invalid character (cl C3850)", "ucn-refused.cpp(12): error: '\U00110000': a universal-character-name specifies an invalid character", "ucn-refused.cpp(13): error: incomplete universal-character-name: \u takes four hexadecimal digits", "ucn-refused.cpp(14): error: incomplete universal-character-name: \U takes eight hexadecimal digits", "ucn-refused.cpp(15): error: '\u0040': this character is not allowed in an identifier"); diags = 12 },
        @{ unit = "comment-unterminated"; expect = @("comment-unterminated.cpp(2): error: unterminated comment (cl C1071)") },
        @{ unit = "if-unterminated"; expect = @("if-unterminated.cpp(3): error: #if without #endif at end of file (cl C1070)") },
        @{ unit = "diagnostics"; expect = @("diagnostics.cpp(17): error S150: 'Priv::secret': cannot access private member declared in class 'Priv' (cl C2248)", "diagnostics.cpp(18): error S151: 'Ab': cannot instantiate abstract class ('f' is pure virtual) (cl C2259)", "diagnostics.cpp(19): error S151: 'Ab': cannot instantiate abstract class", "diagnostics.cpp(20): error S152: 'RefMem::operator=' cannot be generated: 'RefMem' has a data member 'r' of reference type (cl C2582)", "diagnostics.cpp(21): error S152: 'Holder::operator=' cannot be generated: 'Holder' has a data member 'm' whose class cannot be assigned", "diagnostics.cpp(9): error S150: 'Base::priv': cannot access private member declared in class 'Base'", "diagnostics.cpp(22): error S150: 'Base::prot': cannot access protected member declared in class 'Base'", "diagnostics.cpp(23): error S150: 'Hidden::Hidden': cannot access private member declared in class 'Hidden'", "diagnostics.cpp(14): error: expected ')' after the parameter list near '{'", "diagnostics.cpp(24): error: expected an expression near ';'") },
        @{ unit = "ambiguous"; expect = @("ambiguous.cpp(12): error S105: cannot initialise 'p1' of type A* from D*", "ambiguous.cpp(13): error S143: ambiguous conversions from 'D*' to 'A*'", "ambiguous.cpp(14): error S143:", "ambiguous.cpp(15): error S144: ambiguous access of 'a' in 'D'", "ambiguous.cpp(16): error S144: ambiguous access of 'f'", "ambiguous.cpp(17): error S105: cannot convert D to the return type A&", "ambiguous.cpp(26): error S145: 'XD': ambiguous inheritance of 'XV::f', overridden by 'XL::f' and 'XR::f' (cl C2250)", "ambiguous.cpp(34): error S144: ambiguous access of 'a' in 'YD'", "ambiguous.cpp(34): error S144: ambiguous access of 'f' in 'YD'", "ambiguous.cpp(34): error S144: ambiguous access of 's' in 'YD'", "ambiguous.cpp(35): error S144: ambiguous access of 'a' in 'YD'", "ambiguous.cpp(36): error S144: ambiguous access of 'f' in 'YD'", "ambiguous.cpp(37): error S144: ambiguous access of 'a' in 'YD'", "ambiguous.cpp(38): error S144: ambiguous access of 'a' in 'D2'", "ambiguous.cpp(38): error S144: ambiguous access of 'f' in 'D2'", "ambiguous.cpp(42): error S144: ambiguous access of 'v' in 'YY'"); errors = 17 }
    )
    foreach ($r in $rejects) {
        $nm = "front-$($r.unit)"
        foreach ($sfx in @("6.obj", "6.asm", "6.map")) { "stale" | Out-File -Encoding ascii "out\x64\$nm$sfx" }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\$($r.unit).cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        $missing = @($r.expect | Where-Object { -not $c6.Contains($_) })
        # errors: the number of errors the unit must report, no more (several on one line are not told apart above)
        if ($r.ContainsKey("errors")) { $n = ([regex]::Matches($c6, ': error S\d+')).Count; if ($n -ne $r["errors"]) { $missing += "$($r["errors"]) errors (reported: $n)" } }
        # diags: the same for a unit whose errors come from the preprocessor or the parser (no S number)
        if ($r.ContainsKey("diags")) { $n = ([regex]::Matches($c6, '\(\d+\): error')).Count; if ($n -ne $r["diags"]) { $missing += "$($r["diags"]) errors (reported: $n)" } }
        $left = @(@("6.obj", "6.asm", "6.map") | Where-Object { Test-Path "out\x64\$nm$_" })
        if ($rc -ne 0 -and $missing.Count -eq 0 -and $left.Count -eq 0) { Write-Host "  ok   $($r.unit).cpp: errors reported, exit code $rc, no object, listing or map" }
        else { Write-Host "  FAIL $($r.unit).cpp (exit $rc, missing diagnostics: $($missing -join ' | '), left behind: $($left -join ', ')):"; Write-Host $c6; $failed++ }
    }
    # warnings.cpp: the warnings cl gives at /W4 (unreferenced parameter and local, missing return, uninitialised
    # local, unreachable code, shadowing, assignment as a condition, narrowing) on the lines cl warns, and no other
    $wantW = @("warnings.cpp(5): warning S155: 'missingReturn': not all control paths return a value", "warnings.cpp(6): warning S153: 'y': unreferenced parameter", "warnings.cpp(7): warning S120: comparison of signed and unsigned values", "warnings.cpp(8): warning S160: 'initializing': conversion from 'double' to 'int', possible loss of data", "warnings.cpp(9): warning S159: assignment used as a condition", "warnings.cpp(10): warning S156: uninitialized local variable 'u' used", "warnings.cpp(11): warning S157: unreachable code", "warnings.cpp(12): warning S158: declaration of 'a' hides function parameter", "warnings.cpp(12): warning S153: 'a': unreferenced parameter", "warnings.cpp(13): warning S160: 'initializing': truncation of constant value", "warnings.cpp(14): warning S160: 'initializing': conversion from 'int' to 'short', possible loss of data", "warnings.cpp(15): warning S154: 'w': local variable is initialized but not referenced", "warnings.cpp(16): warning S160: 'return': conversion from 'long long' to 'int', possible loss of data", "warnings.cpp(17): warning S160: 'argument': conversion from 'int' to 'short', possible loss of data")
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n front-warnings "..\examples\sample-front\warnings.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    $missingW = @($wantW | Where-Object { -not $c6.Contains($_) })
    $countW = ([regex]::Matches($c6, "warning S1")).Count
    if ($rc -eq 0 -and $missingW.Count -eq 0 -and $countW -eq $wantW.Count) { Write-Host "  ok   warnings.cpp: the $countW warnings of cl /W4 and no other" }
    else { Write-Host "  FAIL warnings.cpp (exit $rc, $countW warnings, missing: $($missingW -join ' | ')):"; Write-Host $c6; $failed++ }
    # handler-order.cpp: a handler hidden by an earlier one of its try block is valid (15.3/5) and compiles with a
    # warning on the lines cl reports (its errors C2312, C2313, C2315 and its warning C4286) and no other (row G23)
    $wantH = @("handler-order.cpp(11): warning S162: 'int': is caught by 'int' on line 10: the handler is never entered (cl C2312)", "handler-order.cpp(14): warning S162: 'int': is caught by 'int' on line 13", "handler-order.cpp(17): warning S162: 'const int&': reference is caught by 'int' on line 16: the handler is never entered (cl C2315)", "handler-order.cpp(20): warning S162: 'int': is caught by reference ('const int&') on line 19: the handler is never entered (cl C2313)", "handler-order.cpp(23): warning S162: 'D&': is caught by 'const D&' on line 22", "handler-order.cpp(26): warning S162: 'int*': is caught by 'const int*' on line 25", "handler-order.cpp(29): warning S162: 'D&': is caught by base class ('B&') on line 28: the handler is never entered (cl C4286)", "handler-order.cpp(32): warning S162: 'E*': is caught by base class ('const B*') on line 31", "handler-order.cpp(35): warning S162: 'D': is caught by base class ('B') on line 34", "handler-order.cpp(38): warning S162: 'W&': is caught by base class ('V&') on line 37", "handler-order.cpp(41): warning S162: 'int': is caught by 'int' on line 40", "handler-order.cpp(46): warning S162: 'int*': is caught by 'int*' on line 45: the handler is never entered (cl C2312)", "handler-order.cpp(49): warning S162: 'void()*': is caught by 'void()*' on line 48")
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n front-handlers "..\examples\sample-front\handler-order.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    $missingH = @($wantH | Where-Object { -not $c6.Contains($_) })
    $countH = ([regex]::Matches($c6, "warning S1")).Count
    if ($rc -eq 0 -and $missingH.Count -eq 0 -and $countH -eq $wantH.Count -and (Test-Path "out\x64\front-handlers6.obj")) { Write-Host "  ok   handler-order.cpp: compiled, the $countH warnings for hidden handlers and no other" }
    else { Write-Host "  FAIL handler-order.cpp (exit $rc, $countH warnings, missing: $($missingH -join ' | ')):"; Write-Host $c6; $failed++ }
    # deep-nesting.cpp: 3000 nested blocks, 5000 nested parentheses and a 2000-term expression compile and run
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\x64 -n front-deep "..\examples\sample-front\deep-nesting.cpp" 2>&1 | Out-String
    $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
    if ($rc -eq 0 -and (Test-Path "out\x64\front-deep6.obj")) {
        $null = Link-Program @("out\x64\front-deep6.obj", "out\x64\cppparse_rt.obj") "out\x64\front-deep.exe"
        & "out\x64\front-deep.exe" | Out-Null
        if ($LASTEXITCODE -eq 0) { Write-Host "  ok   deep-nesting.cpp: 3000 nested blocks and 5000 nested parentheses compile and run" }
        else { Write-Host "  FAIL deep-nesting.cpp runs with exit code $LASTEXITCODE"; $failed++ }
    } else { Write-Host "  FAIL deep-nesting.cpp (exit $rc):"; Write-Host ($c6 -split "`n" | Select-Object -First 5); $failed++ }
    # vbase.cpp, refused by --compile6 (S130) until its vtordispex thunks (B11): the analysis accepts its virtual bases (the
    # polymorphic ones that --compile6 generates are run against cl in front.cpp, pvbase:: and vtordex::)
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
    # a table's path of bases: cl prints "VPR::$vbtable@VPB@VPP@:", the symbol dump "vbtable@VPB/VPP@0:"
    $want += @([regex]::Matches($rep, '(?m)^(V\w+)::\$vbtable@((?:\w+@)*):\s*\r?\n((?:\s*\d+\s*\|\s*-?\d+.*\r?\n)+)') | ForEach-Object { "$($_.Groups[1].Value) vbtable@$($_.Groups[2].Value.TrimEnd('@').Replace('@', '/')):" + ((@([regex]::Matches($_.Groups[3].Value, '\|\s*(-?\d+)')) | ForEach-Object { $_.Groups[1].Value }) -join ',') })
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
    $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-front\front.cpp") @("out\x64\front.obj") "out\x64\front_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\front.cpp" }
    $want = & ".\out\x64\front_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "front$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\front.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\front.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   front.cpp (-O$lvl) prints what cl's build prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL front.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # a pointer difference divides the distance by the element size as cl /Od does: not at all for one byte, by a shift
    # for a power of two, by idiv for the rest (front.cpp ptrdiff::bytes, ints, t12s at -O0)
    $asm0 = if (Test-Path "out\x64\front06.asm") { Get-Content "out\x64\front06.asm" -Raw } else { "" }
    $divs = @{}
    foreach ($f in @("bytes", "ints", "t12s", "bigs")) { $m = [regex]::Match($asm0, "(?s)\?$f@ptrdiff@@\S* PROC.*?ENDP"); $divs[$f] = if ($m.Success) { ([regex]::Matches($m.Value, '\bidiv\b')).Count } else { -1 } }
    if ($divs["bytes"] -eq 0 -and $divs["ints"] -eq 0 -and $divs["t12s"] -eq 1 -and $divs["bigs"] -eq 1) { Write-Host "  ok   front.cpp (-O0): no idiv in the pointer differences of 1- and 4-byte elements, one for 12 bytes and for 2^30 + 1, as cl /Od" }
    else { Write-Host "  FAIL front.cpp (-O0) pointer differences, idiv count: bytes $($divs['bytes']), ints $($divs['ints']), t12s $($divs['t12s']), bigs $($divs['bigs'])"; $failed++ }
    # the vftables of the classes the unit builds, not of one it only points to (N3): no ??_7Named@vtemit nor its virtual
    # functions, ??_7Built@vtemit defined
    $named = [regex]::IsMatch($asm0, '(?m)^\?\?_7Named@vtemit@@6B@ LABEL|^\?f@Named@vtemit@@UEBAHXZ PROC')
    $built = [regex]::IsMatch($asm0, '(?m)^\?\?_7Built@vtemit@@6B@ LABEL')
    if (-not $named -and $built) { Write-Host "  ok   front.cpp (-O0): the vftable of vtemit::Built, none of vtemit::Named, which the unit only points to" }
    else { Write-Host "  FAIL front.cpp (-O0) vftables: Named's table or f emitted $named, Built's table emitted $built"; $failed++ }
    # a call's arguments and result live in volatile registers (B1): callregs::swapped and callregs::rotated save no
    # callee-saved register, and swapped's cycle among the argument registers goes through rax
    $pushes = @{}
    foreach ($f in @("swapped", "rotated")) { $m = [regex]::Match($asm0, "(?s)\?$f@callregs@@\S* PROC.*?ENDP"); $pushes[$f] = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+push (rbx|rsi|rdi|r1[2-5])$')).Count } else { -1 } }
    $cycle = [regex]::IsMatch($asm0, '(?s)\?swapped@callregs@@\S* PROC.*?mov rax, r9\s+mov r9, r8\s+mov r8, rax.*?ENDP')
    if ($pushes["swapped"] -eq 0 -and $pushes["rotated"] -eq 0 -and $cycle) { Write-Host "  ok   front.cpp (-O0): callregs::swapped and rotated keep their call arguments in volatile registers, the cycle through rax" }
    else { Write-Host "  FAIL front.cpp (-O0) call registers: callee-saved pushes in swapped $($pushes['swapped']), rotated $($pushes['rotated']), cycle through rax $cycle"; $failed++ }
    # and an argument is computed in its argument register when nothing before the call writes it (B2): callregs::straight
    # loads a and b into rcx and rdx themselves, no copy before the call
    $m = [regex]::Match($asm0, "(?s)\?straight@callregs@@\S* PROC.*?ENDP")
    $copies = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+mov (rcx|rdx), r')).Count } else { -1 }
    if ($copies -eq 0) { Write-Host "  ok   front.cpp (-O0): callregs::straight computes its arguments in rcx and rdx, no copy before the call" }
    else { Write-Host "  FAIL front.cpp (-O0) call registers: $copies copies into rcx / rdx before the call in callregs::straight"; $failed++ }
    # and a call's result read once, soon after the call, stays in rax (B3): callregs::stored2 stores and compares its
    # results from rax (a constant loaded in between), no copy out of it
    $m = [regex]::Match($asm0, "(?s)\?stored2@callregs@@\S* PROC.*?ENDP")
    $outOfRax = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+mov r[a-z0-9]+, rax$')).Count } else { -1 }
    if ($outOfRax -eq 0) { Write-Host "  ok   front.cpp (-O0): callregs::stored2 reads its call results from rax, no copy out of it" }
    else { Write-Host "  FAIL front.cpp (-O0) call registers: $outOfRax copies out of rax in callregs::stored2"; $failed++ }
    # the inliner measures a callee as the lowering will leave it (F): inl::sameSize (26 IR instructions, 11 lowered) is
    # inlined into inl::countSame's loop at -O2
    $asm2 = if (Test-Path "out\x64\front26.asm") { Get-Content "out\x64\front26.asm" -Raw } else { "" }
    $m = [regex]::Match($asm2, "(?s)\?countSame@inl@@\S* PROC.*?ENDP")
    $calls = if ($m.Success) { ([regex]::Matches($m.Value, 'call \?sameSize@inl@@')).Count } else { -1 }
    if ($calls -eq 0) { Write-Host "  ok   front.cpp (-O2): inl::sameSize is inlined into inl::countSame's loop" }
    else { Write-Host "  FAIL front.cpp (-O2) inlining: $calls call(s) to inl::sameSize left in inl::countSame"; $failed++ }
    # and a function that only passes its arguments on to one call is inlined whatever their count (row G31): fwd::use
    # calls fwd::work, not fwd::pass (five arguments and a sixth: over the limit of the small functions by its size)
    $m = [regex]::Match($asm2, "(?s)\?use@fwd@@\S* PROC.*?ENDP")
    $calls = if ($m.Success) { ([regex]::Matches($m.Value, 'call \?pass@fwd@@')).Count } else { -1 }
    $direct = if ($m.Success) { ([regex]::Matches($m.Value, 'call \?work@fwd@@')).Count } else { -1 }
    if ($calls -eq 0 -and $direct -eq 1) { Write-Host "  ok   front.cpp (-O2): fwd::pass, which only passes its five arguments on, is inlined into fwd::use" }
    else { Write-Host "  FAIL front.cpp (-O2) inlining of a forwarding function: $calls call(s) to fwd::pass and $direct to fwd::work in fwd::use"; $failed++ }
    # a call to a function with C linkage cannot throw (A1): nothrowc::lenOf and before call only C functions while their
    # Buf lives, so they get no unwind handler and no range at -O0; cppBetween calls a C++ function meanwhile and keeps
    # them at -O0, and loses them at -O2, where that call is inlined away
    $eh = @{}
    # (a pad's block, or in cl's model its funclet "cleanup of region n"; a range, or in cl's model a state)
    foreach ($f in @("lenOf", "before", "cppBetween")) { $m = [regex]::Match($asm0, "(?s)\?$f@nothrowc@@\S* PROC.*?ENDP"); $eh[$f] = if ($m.Success) { [int]($m.Value -match '(?m); (unwind|cleanup)( of region \d+)?\s*$') + [int]($m.Value -match ';@ehrange|;@ehstate') } else { -1 } }
    $m = [regex]::Match($asm2, "(?s)\?cppBetween@nothrowc@@\S* PROC.*?ENDP"); $eh["cppBetween2"] = if ($m.Success) { [int]($m.Value -match '(?m); (unwind|cleanup)( of region \d+)?\s*$') + [int]($m.Value -match ';@ehrange|;@ehstate') } else { -1 }
    if ($eh["lenOf"] -eq 0 -and $eh["before"] -eq 0 -and $eh["cppBetween"] -eq 2 -and $eh["cppBetween2"] -eq 0) { Write-Host "  ok   front.cpp: nothrowc::lenOf and before have no unwind handler (C calls only while Buf lives), cppBetween has one at -O0 and none at -O2" }
    else { Write-Host "  FAIL front.cpp unwind handlers (handler + range found): lenOf $($eh['lenOf']), before $($eh['before']), cppBetween -O0 $($eh['cppBetween']), -O2 $($eh['cppBetween2']); expected 0, 0, 2, 0"; $failed++ }
    # a value takes over the register of an operand whose last use defines it (B4): coal::sum3's two-address forms copy no
    # register to another (every operand is loaded, used once and dies) but the result into eax at -O0 (one copy; the last
    # operation is fixed to rax before the takeover runs); at -O2 a's copy for the later "- a" is a lea since D4, so one too
    $m0 = [regex]::Match($asm0, "(?s)\?sum3@coal@@\S* PROC.*?ENDP"); $m2 = [regex]::Match($asm2, "(?s)\?sum3@coal@@\S* PROC.*?ENDP")
    $moves0 = if ($m0.Success) { ([regex]::Matches($m0.Value, '(?m)^\s+mov (?!rbp, rsp)(r[a-z0-9]+|e[a-z][a-z]), (r[a-z0-9]+|e[a-z][a-z])$')).Count } else { -1 }
    $moves2 = if ($m2.Success) { ([regex]::Matches($m2.Value, '(?m)^\s+mov (?!rbp, rsp)(r[a-z0-9]+|e[a-z][a-z]), (r[a-z0-9]+|e[a-z][a-z])$')).Count } else { -1 }
    if ($moves0 -eq 1 -and $moves2 -eq 1) { Write-Host "  ok   front.cpp: coal::sum3 computes each result in the register of its dying operand (register copies: $moves0 at -O0, $moves2 at -O2)" }
    else { Write-Host "  FAIL front.cpp register takeover: $moves0 register copies in coal::sum3 at -O0, $moves2 at -O2 (expected 1 and 1)"; $failed++ }
    # a floating-point value that no call interrupts lives in xmm3 (B6): fvol::blend saves no callee-saved xmm register
    $saves = @{}
    foreach ($lvl in @(0, 2)) { $m = [regex]::Match((&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } }), "(?s)\?blend@fvol@@\S* PROC.*?ENDP"); $saves[$lvl] = if ($m.Success) { ([regex]::Matches($m.Value, '\.savexmm128')).Count } else { -1 } }
    if ($saves[0] -eq 0 -and $saves[2] -eq 0) { Write-Host "  ok   front.cpp: fvol::blend keeps its temporaries in xmm3 and saves no xmm register" }
    else { Write-Host "  FAIL front.cpp volatile xmm3: fvol::blend saves $($saves[0]) xmm registers at -O0, $($saves[2]) at -O2 (expected none)"; $failed++ }
    # a 4-byte value lives in the low 32 bits of its register and is computed with the 32-bit forms (D1): coal::sum3 has no
    # movsxd at all (the loads are mov r32, dword ptr; nothing extends a result)
    $sx = @{}
    foreach ($lvl in @(0, 2)) { $m = [regex]::Match((&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } }), "(?s)\?sum3@coal@@\S* PROC.*?ENDP"); $sx[$lvl] = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+movsxd ')).Count } else { -1 } }
    if ($sx[0] -eq 0 -and $sx[2] -eq 0) { Write-Host "  ok   front.cpp: coal::sum3 computes in 32 bits, no sign extension at -O0 or -O2" }
    else { Write-Host "  FAIL front.cpp 32-bit widths: $($sx[0]) movsxd in coal::sum3 at -O0, $($sx[2]) at -O2 (expected none)"; $failed++ }
    # at -O0 a constant read once is its reader's immediate (D2): coal::show keeps no const instruction, and sum3's six
    # literal arguments are loaded straight into their registers
    $m = [regex]::Match($asm0, "(?s)\?show@coal@@\S* PROC.*?ENDP")
    $consts = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+; t\d+ = const ')).Count } else { -1 }
    $direct = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+mov (ecx|edx|r8d), -?\d+\s*$')).Count } else { -1 }
    if ($consts -eq 0 -and $direct -eq 6) { Write-Host "  ok   front.cpp: coal::show at -O0 keeps no const instruction, sum3's six literal arguments load straight into their registers" }
    else { Write-Host "  FAIL front.cpp immediates at -O0: $consts const instructions in coal::show, $direct literal argument loads (expected 0 and 6)"; $failed++ }
    # a floating-point argument is copied into its integer register only for a variadic callee (D3): fvol::show at -O0
    # has printf's two movq and none for blend's four arguments
    $m = [regex]::Match($asm0, "(?s)\?show@fvol@@\S* PROC.*?ENDP")
    $movq = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+movq r')).Count } else { -1 }
    if ($movq -eq 2) { Write-Host "  ok   front.cpp: fvol::show at -O0 copies only printf's two double arguments to integer registers (movq), none of blend's" }
    else { Write-Host "  FAIL front.cpp variadic-only movq: $movq movq in fvol::show at -O0 (expected 2)"; $failed++ }
    # a constant index is the instruction's offset and an add whose operands live on goes into another register with one
    # lea (D4): lea4::step is lea rax, [p+12] at both levels (no index register, no imul), lea4::sum at -O2 adds with a lea
    $lea = @{}
    foreach ($lvl in @(0, 2)) { $m = [regex]::Match((&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } }), "(?s)\?step@lea4@@\S* PROC.*?ENDP"); $lea[$lvl] = if ($m.Success) { [int]($m.Value -match '(?m)^\s+lea rax, \[r\w+\+12\]\s*$') - [int]($m.Value -match '(?m)^\s+(imul|movsxd|lea \w+, \[\w+\+\w+\*4\])') } else { -1 } }
    $m = [regex]::Match($asm2, "(?s)\?sum@lea4@@\S* PROC.*?ENDP"); $lea["sum"] = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+lea \w+, \[rcx\+rdx\]\s*$')).Count } else { -1 }
    if ($lea[0] -eq 1 -and $lea[2] -eq 1 -and $lea["sum"] -eq 1) { Write-Host "  ok   front.cpp: lea4::step is lea rax, [p+12] at -O0 and -O2, lea4::sum adds into another register with a lea" }
    else { Write-Host "  FAIL front.cpp lea forms: step -O0 $($lea[0]), -O2 $($lea[2]) (expected 1: the lea, no index register), sum lea [rcx+rdx] $($lea['sum']) (expected 1)"; $failed++ }
    # the frame in order of use (D5): hot5::many's scalars (n, s, i: every dword access to the frame) lie within 128 bytes of
    # rbp although its 160-byte array is declared first, so none of them takes a 4-byte displacement
    $m = [regex]::Match($asm0, "(?s)\?many@hot5@@\S* PROC.*?ENDP")
    $far = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)dword ptr \[rbp-(\d+)\]') | Where-Object { [int]$_.Groups[1].Value -gt 128 }).Count } else { -1 }
    $near = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)dword ptr \[rbp-(\d+)\]') | Where-Object { [int]$_.Groups[1].Value -le 128 }).Count } else { -1 }
    if ($far -eq 0 -and $near -ge 6) { Write-Host "  ok   front.cpp: hot5::many at -O0 keeps its scalars within 128 bytes of rbp ($near byte-displacement accesses, none with four bytes) although its array is declared first" }
    else { Write-Host "  FAIL front.cpp frame order: hot5::many at -O0 has $far scalar accesses beyond rbp-128 and $near within (expected 0 and at least 6)"; $failed++ }
    # +0.0 is made by clearing the register (D6): no load of the all-zero constant at either level, xorps instead
    $zero0 = ([regex]::Matches($asm0, 'real8 ptr \[fconst_0000000000000000\]')).Count; $zero2 = ([regex]::Matches($asm2, 'real8 ptr \[fconst_0000000000000000\]')).Count
    $xorps = ([regex]::Matches($asm2, '(?m)^\s+xorps (xmm\d+), \1\s*$')).Count
    if ($zero0 -eq 0 -and $zero2 -eq 0 -and $xorps -ge 10) { Write-Host "  ok   front.cpp: +0.0 clears its register (xorps, $xorps at -O2), the zero constant is never loaded" }
    else { Write-Host "  FAIL front.cpp zero constant: loads of fconst_0 $zero0 at -O0, $zero2 at -O2 (expected 0), xorps at -O2 $xorps (expected 10 or more)"; $failed++ }
    # a local assigned and read again in the block keeps its register at -O0 (D7): fwd7::chain never reloads t (its slot
    # from the frame comment), fwd7::vol reloads the volatile v after each of its three stores
    $fw = @{}
    foreach ($fn in @("chain", "vol")) {
        $m = [regex]::Match($asm0, "(?s)\?$fn@fwd7@@\S* PROC.*?ENDP")
        $var = if ($fn -eq "chain") { "t" } else { "v" }
        $slot = if ($m.Success) { [regex]::Match($m.Value, "(?m)\b$var@\[(rbp-\d+)\]").Groups[1].Value } else { "" }
        $fw[$fn] = if ($slot) { ([regex]::Matches($m.Value, '(?m)^\s+mov \w+, dword ptr \[' + [regex]::Escape($slot) + '\]')).Count } else { -1 }
    }
    if ($fw["chain"] -eq 0 -and $fw["vol"] -ge 3) { Write-Host "  ok   front.cpp: fwd7::chain at -O0 never reloads t after storing it, fwd7::vol reloads its volatile v $($fw['vol']) times" }
    else { Write-Host "  FAIL front.cpp store forwarding at -O0: reloads of t in fwd7::chain $($fw['chain']) (expected 0), of volatile v in fwd7::vol $($fw['vol']) (expected 3 or more)"; $failed++ }
    # the landing pads are laid out after the code of their region (D8): no jump hops over a cleanup pad to the block
    # right after it, at either level
    $lay = @{}
    foreach ($lvl in @(0, 2)) {
        $a = &{ if ($lvl -eq 0) { $asm0 } else { $asm2 } }
        $lay["hop$lvl"] = ([regex]::Matches($a, '(?m)^\s+jmp (\S+)\s*\r?\n\S+_B\d+:\s*; cleanup\s*$(?:\r?\n(?!\S+_B\d+:).*)*\r?\n\1:')).Count
        $lay["pads$lvl"] = ([regex]::Matches($a, '(?m)^\S+_B\d+:\s*; cleanup\s*$')).Count
    }
    if ($lay["hop0"] -eq 0 -and $lay["hop2"] -eq 0 -and $lay["pads0"] -gt 0) { Write-Host "  ok   front.cpp: no jump hops over a landing pad ($($lay['pads0']) pads at -O0, $($lay['pads2']) at -O2)" }
    else { Write-Host "  FAIL front.cpp block layout: jumps hopping over a pad $($lay['hop0']) at -O0, $($lay['hop2']) at -O2 (expected 0); pads at -O0 $($lay['pads0'])"; $failed++ }
    # the blocks in chains (D9): in chain9::both no conditional jump is followed by an unconditional one at either level
    $ch = @{}
    foreach ($lvl in @(0, 2)) {
        $m = [regex]::Match((&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } }), "(?s)\?both@chain9@@\S* PROC.*?ENDP")
        $ch["pair$lvl"] = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+j(?!mp)[a-z]+ \S+\s*\r?\n\s+jmp ')).Count } else { -1 }
        $ch["jcc$lvl"] = if ($m.Success) { ([regex]::Matches($m.Value, '(?m)^\s+j(?!mp)[a-z]+ ')).Count } else { -1 }
    }
    if ($ch["pair0"] -eq 0 -and $ch["pair2"] -eq 0 -and $ch["jcc0"] -ge 4) { Write-Host "  ok   front.cpp: chain9::both's conditional jumps ($($ch['jcc0']) at -O0, $($ch['jcc2']) at -O2) each fall through to one target, none is followed by a jmp" }
    else { Write-Host "  FAIL front.cpp chain layout: conditional jumps followed by a jmp in chain9::both $($ch['pair0']) at -O0, $($ch['pair2']) at -O2 (expected 0); conditional jumps at -O0 $($ch['jcc0']) (expected 4 or more)"; $failed++ }
    # the lean landing pad (A5): a plain object's pad is one call of cppparse_unwind_destroy (no enter, destructor, leave,
    # rethrow), and the range table names such a pad itself: no dispatch stub that is a lone jump
    $lp = @{}
    foreach ($lvl in @(0, 2)) {
        $a = (&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } })
        # (cl's model: the lean pad is the funclet that is one jump to the destructor, ";@funclet 0")
        $lp["destroy$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_destroy')).Count + ([regex]::Matches($a, '(?m)^;@funclet 0')).Count
        $lp["enter$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_enter')).Count
        $lp["stub$lvl"] = ([regex]::Matches($a, '(?m)^\S+_T\d+:[^\r\n]*\r?\n(?:\S+_R\d+[be]:\s*\r?\n)*\s+jmp ')).Count
    }
    if ($lp["destroy0"] -ge 100 -and $lp["destroy2"] -ge 50 -and $lp["stub0"] -eq 0 -and $lp["stub2"] -eq 0) { Write-Host "  ok   front.cpp: lean landing pads ($($lp['destroy0']) at -O0, $($lp['destroy2']) at -O2; $($lp['enter0']) pads with enter and leave at -O0), no dispatch stub that is one jump" }
    else { Write-Host "  FAIL front.cpp landing pads: cppparse_unwind_destroy calls $($lp['destroy0']) at -O0 (expected 100 or more), $($lp['destroy2']) at -O2 (expected 50 or more); dispatch stubs that are one jump $($lp['stub0']) at -O0, $($lp['stub2']) at -O2 (expected 0)"; $failed++ }
    # the pads of a frame follow one another by the generated code (row G30): a pad inside a region of its function
    # calls cppparse_unwind_destroy_next, which returns, and goes on at that region's pad by a jump or by falling into
    # the pad laid out next (a label line follows the call); the entry that does not return is followed by neither.
    # The runtime writes no return address: it reads its own in one place (cppparse_unwind_enter notes the frame)
    $pc = @{}
    foreach ($lvl in @(0, 2)) {
        $a = (&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } })
        $pc["plain$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_destroy')).Count
        $pc["next$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_destroy_next\s*$')).Count
        $pc["jump$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_destroy_next\s*\r?\n\s+jmp \S+')).Count
        $pc["fall$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_unwind_destroy_next\s*\r?\n(?:\s+;[^\r\n]*\r?\n)*\S+:')).Count
    }
    $rtText = Get-Content "$Sandbox\..\runtime\cppparse_rt.cpp" -Raw
    $rtReads = ([regex]::Matches($rtText, '_AddressOfReturnAddress\s*\(')).Count
    $rtWrites = ([regex]::Matches($rtText, '\*\s*\(?\s*\(\s*void\s*\*\s*\*\s*\)\s*_AddressOfReturnAddress|\*\s*back\s*=')).Count
    $padsOk = $true
    foreach ($lvl in @(0, 2)) { if ($pc["jump$lvl"] + $pc["fall$lvl"] -ne $pc["next$lvl"]) { $padsOk = $false } }
    # (cl's model has funclets in place of these pads: no such call in the listing)
    if ($pc["plain0"] -gt 0 -and $pc["next0"] -eq 0) { $padsOk = $false }
    if ($padsOk -and $rtReads -eq 1 -and $rtWrites -eq 0) { Write-Host "  ok   front.cpp: $($pc['next0']) pads at -O0 and $($pc['next2']) at -O2 go on at the frame's next pad themselves ($($pc['fall0']) and $($pc['fall2']) fall into it, the others jump); the runtime writes no return address" }
    else { Write-Host "  FAIL front.cpp pad chain: at -O0 $($pc['next0']) calls of cppparse_unwind_destroy_next, $($pc['jump0']) followed by a jmp, $($pc['fall0']) by a label; at -O2 $($pc['next2']), $($pc['jump2']), $($pc['fall2']) (each call is followed by one or the other); cppparse_rt.cpp reads its return address in $rtReads place(s) (expected 1) and writes one in $rtWrites (expected 0)"; $failed++ }
    # a byte that only steers the next branch (D10) is tested where it is: no call's bool result zero-extended and then
    # tested as 32 bits, bytes compared in memory or tested in their byte register
    $bt = @{}
    foreach ($lvl in @(0, 2)) {
        $a = (&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } })
        $bt["ext$lvl"] = ([regex]::Matches($a, '(?m)^\s+call [^\r\n]*(?:\r?\n\s*;[^\r\n]*)*\r?\n\s+movzx eax, al\b[^\r\n]*(?:\r?\n\s*;[^\r\n]*)*\r?\n\s+test eax, eax')).Count
        $bt["mem$lvl"] = ([regex]::Matches($a, '(?m)^\s+cmp byte ptr \[[^\]]+\], 0')).Count
        $bt["reg$lvl"] = ([regex]::Matches($a, '(?m)^\s+test (al|bl|cl|dl|sil|dil|r\d+b), \1\b')).Count
    }
    if ($bt["ext0"] -eq 0 -and $bt["ext2"] -eq 0 -and $bt["mem0"] -ge 40 -and $bt["reg0"] -ge 5) { Write-Host "  ok   front.cpp: bytes that steer a branch are tested where they are ($($bt['mem0']) in memory and $($bt['reg0']) in a byte register at -O0, $($bt['mem2']) and $($bt['reg2']) at -O2; no call result zero-extended for a test)" }
    else { Write-Host "  FAIL front.cpp byte tests: call results zero-extended and then tested $($bt['ext0']) at -O0, $($bt['ext2']) at -O2 (expected 0); bytes compared in memory at -O0 $($bt['mem0']) (expected 40 or more), tested in a byte register $($bt['reg0']) (expected 5 or more)"; $failed++ }
    # every function keeps the table model (A6): no setjmp frame in front.cpp at either level, nest6::pick (a ?: nested in an
    # arm of another, with std::string temporaries in the arms) included
    $sj = @{}
    foreach ($lvl in @(0, 2)) {
        $a = (&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } })
        $sj["frames$lvl"] = ([regex]::Matches($a, '(?m)^\s+call cppparse_push_frame')).Count
        $sj["pick$lvl"] = ([regex]::Matches($a, '(?m)^\?pick@nest6@@\S* PROC')).Count
    }
    if ($sj["frames0"] -eq 0 -and $sj["frames2"] -eq 0 -and $sj["pick0"] -eq 1) { Write-Host "  ok   front.cpp: no setjmp frame at either level (nest6::pick keeps the table model)" }
    else { Write-Host "  FAIL front.cpp setjmp frames: $($sj['frames0']) at -O0, $($sj['frames2']) at -O2 (expected 0); nest6::pick found $($sj['pick0']) (expected 1)"; $failed++ }
    # dead code (D11): a block no branch reaches is not emitted (no block labelled dead at either level, no switch.end in
    # unread8::g, whose cases all return), nor is a value nothing reads (unread8::f never stores or loads its parameter k,
    # which arrives in edx)
    $dc = @{}
    foreach ($lvl in @(0, 2)) {
        $a = (&{ if ($lvl -eq 0) { $asm0 } else { $asm2 } })
        $dc["dead$lvl"] = ([regex]::Matches($a, '(?m)^\S+_B\d+:\s*; dead\s*$')).Count
        $fm = [regex]::Match($a, "(?s)\?f@unread8@@\S* PROC.*?ENDP")
        $dc["edx$lvl"] = if ($fm.Success) { ([regex]::Matches($fm.Value, '(?m)^\s+\w+ [^;\r\n]*\bedx\b')).Count } else { -1 }
        $gm = [regex]::Match($a, "(?s)\?g@unread8@@\S* PROC.*?ENDP")
        $dc["swend$lvl"] = if ($gm.Success) { ([regex]::Matches($gm.Value, '; switch\.end')).Count } else { -1 }
    }
    if ($dc["dead0"] -eq 0 -and $dc["dead2"] -eq 0 -and $dc["edx0"] -eq 0 -and $dc["edx2"] -eq 0 -and $dc["swend0"] -eq 0 -and $dc["swend2"] -eq 0) { Write-Host "  ok   front.cpp: no block nothing reaches is emitted and no value nothing reads (unread8::f leaves its parameter k alone, unread8::g has no switch.end)" }
    else { Write-Host "  FAIL front.cpp dead code: blocks labelled dead $($dc['dead0']) at -O0, $($dc['dead2']) at -O2 (expected 0); instructions of unread8::f on edx $($dc['edx0']) / $($dc['edx2']) (expected 0); switch.end blocks in unread8::g $($dc['swend0']) / $($dc['swend2']) (expected 0)"; $failed++ }
    # copies.cpp: copies and temporaries, logged by the constructors and destructors. cl /Od copies a returned named
    # local and /O2 builds it in the result (NRVO): -O0 must print what cl /Od's build prints, -O2 what cl /O2's does
    foreach ($lvl in @(0, 2)) {
        $clOpt = if ($lvl -eq 0) { "/Od" } else { "/O2" }
        $clOut = Build-Program @("/EHsc", "/W3", $clOpt, "/Foout\x64\", "..\examples\sample-front\copies.cpp") @("out\x64\copies.obj") "out\x64\copies_cl$lvl.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\copies.cpp" }
        $want = & ".\out\x64\copies_cl$lvl.exe" 2>&1 | Out-String
        $nm = "copies$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\copies.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\copies.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   copies.cpp (-O$lvl) logs the copies and destructions of cl $clOpt ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL copies.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl $clOpt's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # alttok.cpp: the alternative tokens, digraphs and trigraphs of C++98 (2.5, 2.3) mean their primary tokens, and
    # # keeps the spelling written; cl reads them with /permissive- and /Zc:trigraphs
    $clOut = Build-Program @("/EHsc", "/W3", "/permissive-", "/Zc:trigraphs", "/Foout\x64\", "..\examples\sample-front\alttok.cpp") @("out\x64\alttok.obj") "out\x64\alttok_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\alttok.cpp" }
    $want = & ".\out\x64\alttok_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "alttok$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\alttok.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\alttok.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want -and $want -match 'str a and b\|x<:1:>\|%: y' -and $want -match 'strlit 1') { Write-Host "  ok   alttok.cpp (-O$lvl): alternative tokens, digraphs and trigraphs as cl /permissive- /Zc:trigraphs reads them, # keeps their spelling" }
        else { Write-Host "  FAIL alttok.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # ucn.cpp: universal character names (2.2/2, Annex E). In identifiers: the program built by cl against cppparse's
    # object linked with cl's object of the file's other half (/DUCN_OTHER: the names must decorate alike, UTF-8 in
    # the symbol); in literals: the code units cl gives (code page 1252 in narrow literals, UTF-16 in wide ones)
    $clOut = & $cl /nologo /c /EHsc /W3 /permissive- /DUCN_OTHER "/Foout\x64\ucn_other.obj" "..\examples\sample-front\ucn.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\ucn.cpp (UCN_OTHER)" }
    $clOut = Build-Program @("/EHsc", "/W3", "/permissive-", "/Foout\x64\ucn_cl.obj", "..\examples\sample-front\ucn.cpp") @("out\x64\ucn_cl.obj", "out\x64\ucn_other.obj") "out\x64\ucn_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\ucn.cpp" }
    $want = & ".\out\x64\ucn_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "ucn$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\ucn.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\ucn.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\ucn_other.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj with cl's ucn_other.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want -and $want -match 'names 343374' -and $want -match 'n2 2: 80 00' -and $want -match '#if: a narrow') { Write-Host "  ok   ucn.cpp (-O$lvl): universal character names in identifiers (linked with cl's object) and in literals as cl reads them" }
        else { Write-Host "  FAIL ucn.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # -E keeps the names as written (cl /EP prints them in the code page, which does not read back: no comparison)
    $p = Start-Process -FilePath $exe -ArgumentList '-E','-DUCN_NO_STDIO','..\examples\sample-front\ucn.cpp' -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$Sandbox\out\x64\ucn-pp.txt" -RedirectStandardError "$Sandbox\out\x64\ucn-pp-diag.txt"
    $ppText = Get-Content "$Sandbox\out\x64\ucn-pp.txt" -Raw
    $wantPp = @('int local\u00e9 = caf\u00E9 + \u0434\u0430;', 'int x\u00e9y = 5;', 'n\u03b1::S\U000000e9::s\u4e2d * 10 + x\u00e9y + 7 + b.get\u00e9();', 'const char s2[] = "caf\u00E9 \U000000e9 x\u00e9y";')
    $missingPp = @($wantPp | Where-Object { -not $ppText.Contains($_) })
    if ($p.ExitCode -eq 0 -and $missingPp.Count -eq 0) { Write-Host "  ok   ucn.cpp: -E and # keep the universal character names as written" }
    else { Write-Host "  FAIL -E of sample-front\ucn.cpp (exit $($p.ExitCode)), missing: $($missingPp -join ' | ')"; $failed++ }
    # front2.cpp: built by cl (the reference) and by cppparse at -O0 and -O2; the programs must print the same lines
    $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-front\front2.cpp") @("out\x64\front2.obj") "out\x64\front2_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\front2.cpp" }
    $want = & ".\out\x64\front2_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "front2_$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\front2.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\front2.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want -and $want -match 'explorder named 28 32 2 2 20 10 2 4' -and $want -match 'friends 7 7 8 9 14 21 2 8 6 5 5' -and $want -match 'protected 15 14 26 5 5 5 4' -and $want -match 'tiarray \.\$\$BY02H\|\.\$\$BY114H\|\.\$\$BY01\$\$CBH\|' -and $want -match 'tiarray \.PEAY02\$\$CBH\|\.PEAY112H\|' -and $want -match 'explorder 24 9 8 31 192151 322 4' -and $want -match 'newarr V new\[\] 104' -and $want -match 'newarr C delete\[\] placement 124 9' -and $want -match 'newarr live 0' -and $want -match 'mpdown VW2 fw\[16\] 0 0 fs\[16\] 0 8 fs2\[16\] 0 4' -and $want -match 'mpdown K2 nv\[12\] 48 40 0 s\[12\] 0 40 4' -and $want -match 'elab 4 5 1 6' -and $want -match 'deducecv 4 7 3 4 25 3' -and $want -match 'deducecv 1 0 1 0 1' -and $want -match 'arrparm 249' -and $want -match 'qualdeduce 11 3 12 8 20 72' -and $want -match 'qualdeduce 1 2 105 203 8 16 9 3\.5' -and $want -match 'protstatic 32 64 15' -and $want -match 'specconst 2 13 20 2' -and $want -match 'newhide 7 7 7 4' -and $want -match 'opaccess 161 109 -21 42' -and $want -match 'typeaccess 11 8 19 42' -and $want -match 'dtoraccess 1 134' -and $want -match 'friendtmpl 35 36 14 1 0' -and $want -match 'baseaccess 14 2 7 7 5 16 10' -and $want -match 'fnarr 4 2 8 4 4 15 11 0 0 9 31' -and $want -match 'fnref 41 2 8 2 10 14' -and $want -match 'accessrest 12 13 9 6 8 7 17' -and $want -match 'fnarr2 20 20 1 4 7 2 10 100 60') { Write-Host "  ok   front2.cpp (-O$lvl) prints what cl's build prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL front2.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    # parameters declared as arrays decorate as cl's const pointers (front2.cpp arrparm::, the names cl gives them)
    $arrNames = @('?f1@arrparm@@YAHQEAH@Z', '?f2@arrparm@@YAHQEAH0@Z', '?f3@arrparm@@YAHQEAHPEAH0@Z', '?f4@arrparm@@YAHQEAPEBDQEBH@Z', '?f5@arrparm@@YAHQEAY02H0@Z', '?g@S@arrparm@@QEBAHQEAH@Z', '?h@S@arrparm@@SAHQEAD@Z', '?decl@arrparm@@YAHQEAH@Z', '?decl2@arrparm@@YAHPEAH@Z', '??$t1@H@arrparm@@YAHQEAH@Z', '??$t1@N@arrparm@@YAHQEAN@Z')
    $asmArr = if (Test-Path "out\x64\front2_06.asm") { Get-Content "out\x64\front2_06.asm" -Raw } else { "" }
    $arrMissing = @($arrNames | Where-Object { -not $asmArr.Contains("$_ PROC") })
    if ($arrMissing.Count -eq 0) { Write-Host "  ok   front2.cpp (-O0): $($arrNames.Count) functions with array parameters decorate as cl's (QEAH, QEAPEBD, QEAY02H, back-references)" }
    else { Write-Host "  FAIL front2.cpp (-O0): not in the listing: $($arrMissing -join ' ')"; $failed++ }
    # array parameters inside the function types of a parameter (front2.cpp fnarr::, the names of cl's object): const
    # pointers there as well, the second use of a function type a back-reference however it is written, a reference
    # to a function as A6A (G15)
    $fnArrNames = @('?one@fnarr@@YAHQEAH@Z', '?fp@fnarr@@YAHP6AHQEAH@Z0@Z', '?two@fnarr@@YAHP6AHQEAH@Z1@Z', '?two2@fnarr@@YAHP6AHPEAH@Z1@Z', '?mix@fnarr@@YAHP6AHPEAH@ZQEAH@Z', '?mix2@fnarr@@YAHP6AHQEAH@ZPEAH@Z', '?ref@fnarr@@YAHA6AHQEAPEAD@ZPEAPEAD@Z', '?inner@fnarr@@YAHP6AHQEAH@Z0@Z', '?deep@fnarr@@YAHP6AHP6AHQEAH@Z0@Z@Z', '?rep@fnarr@@YAHP6AHP6AHQEAH@Z10@Z@Z', '?ret@fnarr@@YAHP6AP6AHQEAH@ZQEAD@Z@Z', '?m@S@fnarr@@QEAAHP6AXQEAH@Z@Z', '?sm@S@fnarr@@SAHP6AHQEBD@Z0@Z', '??$tp@H@fnarr@@YAHP6AHQEAH@ZH@Z')
    $fnArrMissing = @($fnArrNames | Where-Object { -not $asmArr.Contains("$_ PROC") })
    if ($fnArrMissing.Count -eq 0) { Write-Host "  ok   front2.cpp (-O0): $($fnArrNames.Count) functions with array parameters inside function-pointer parameters decorate as cl's (P6AHQEAH@Z, back-references, A6A)" }
    else { Write-Host "  FAIL front2.cpp (-O0): not in the listing: $($fnArrMissing -join ' ')"; $failed++ }
    # and in the type a declared function returns, through a typedef and in a variable's type (front2.cpp fnarr2::, the
    # names of cl's object; G15, the rest)
    $fnArr2Fns = @('?pick@fnarr@@YAP6AHQEAH@ZQEAD@Z', '?pick@fnarr2@@YAP6AHQEAH@ZQEAD@Z', '?pick3@fnarr2@@YAP6AP6AHQEAH@ZQEAD@ZH@Z', '?t1@fnarr2@@YAHP6AHQEAH@Z@Z', '?t2@fnarr2@@YAHP6AHPEAH@Z@Z', '?t3@fnarr2@@YAHP6AHQEAH@Z@Z', '?t4@fnarr2@@YAHP6AHQEAH@Z1@Z', '?t5@fnarr2@@YAHP6AHPEAH@Z1@Z', '?t6@fnarr2@@YAHA6AHQEAH@Z@Z')
    $fnArr2Vars = @('?gp@fnarr2@@3P6AHQEAH@ZEA', '?gp2@fnarr2@@3P6AHPEAH@ZEA', '?gp3@fnarr2@@3P6AHQEAH@ZEA', '?gp4@fnarr2@@3P6AHQEAH@ZEA', '?garr@fnarr2@@3PAP6AHQEAH@ZA', '?gpp@fnarr2@@3P6AP6AHQEAH@ZQEAD@ZEA', '?sm@S@fnarr2@@2P6AHQEAH@ZEA', '?sm2@S@fnarr2@@2P6AHQEAH@ZEA')
    $fnArr2Missing = @($fnArr2Fns | Where-Object { -not $asmArr.Contains("$_ PROC") }) + @($fnArr2Vars | Where-Object { -not $asmArr.Contains("PUBLIC $_") })
    if ($fnArr2Missing.Count -eq 0) { Write-Host "  ok   front2.cpp (-O0): $($fnArr2Fns.Count) functions and $($fnArr2Vars.Count) variables with array parameters in a result type, a typedef or a variable's type decorate as cl's" }
    else { Write-Host "  FAIL front2.cpp (-O0): not in the listing: $($fnArr2Missing -join ' ')"; $failed++ }
    # covariant-vbase.cpp: a covariant override in a virtual base's vftable whose overrider is held by another virtual
    # base. The output is compared with the lines the standard requires and with the build of Visual Studio's
    # clang-cl (the reference here, at /Od for -O0 and /O2 for -O2), not with cl's build: cl's own vtordispex thunk
    # there skips the return adjustment (the file's header)
    $wantCov = "covvbx 2 3 2 1 5`ncovvbx 2 5 1 15`ncovvbx 1 1 2 3`ncovvbx 2 3 7 1`n"
    $clangCl = "$VsRoot\VC\Tools\Llvm\x64\bin\clang-cl.exe"
    if (-not (Test-Path $clangCl)) { Write-Host "  note covariant-vbase.cpp: clang-cl is not installed ($clangCl); compared with the fixed lines only" }
    foreach ($lvl in @(0, 2)) {
        $nm = "covvbx_$lvl"
        $refCov = $wantCov
        if (Test-Path $clangCl) {
            $copt = if ($lvl -eq 0) { "/Od" } else { "/O2" }
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $ccOut = & $clangCl /nologo /c /EHsc $copt "/Foout\x64\${nm}_clang.obj" "..\examples\sample-front\covariant-vbase.cpp" 2>&1 | Out-String
            $rc = $LASTEXITCODE
            if ($rc -eq 0) { $ccOut += Link-Program @("out\x64\${nm}_clang.obj") "out\x64\${nm}_clang.exe" | Out-String; $rc = $LASTEXITCODE }
            $ErrorActionPreference = $eap
            if ($rc -ne 0) { Write-Host "  FAIL clang-cl $copt of sample-front\covariant-vbase.cpp:"; Write-Host $ccOut; $failed++; continue }
            $refCov = ((& ".\out\x64\${nm}_clang.exe" 2>&1 | Out-String) -replace "`r", "")
            if ($LASTEXITCODE -ne 0 -or $refCov -ne $wantCov) { Write-Host "  FAIL covariant-vbase.cpp: clang-cl $copt does not print the lines the standard requires (exit $LASTEXITCODE):"; Write-Host $refCov; $failed++; continue }
        }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\covariant-vbase.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\covariant-vbase.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = ((& ".\out\x64\${nm}.exe" 2>&1 | Out-String) -replace "`r", "")
        if ($LASTEXITCODE -eq 0 -and $got -eq $refCov) { Write-Host "  ok   covariant-vbase.cpp (-O$lvl): the overrider in another virtual base returns the adjusted pointer (4 lines, as clang-cl's build and the standard's)" }
        else { Write-Host "  FAIL covariant-vbase.cpp (-O$lvl, exit $LASTEXITCODE):"; Write-Host $got; $failed++ }
    }
    # covariant-variadic.cpp: a variadic function overridden with a covariant return type that moves the pointer; cl
    # refuses the program (C2688), so the output is compared with the lines the file's header lists. The last two: the
    # static locals of CD3::w are one object each for its two bodies, built once and destroyed once after main (G9)
    $wantVar = "covvar 2 60 21 1`ncovvar 2 11 9`ncovvar 2 115 109 1 5`ncovvar static 4101 4102 4103 1`ncovvar dropped 41 1`n"
    foreach ($lvl in @(0, 2)) {
        $nm = "covvar_$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\covariant-variadic.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\covariant-variadic.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = ((& ".\out\x64\${nm}.exe" 2>&1 | Out-String) -replace "`r", "")
        if ($LASTEXITCODE -eq 0 -and $got -eq $wantVar) { Write-Host "  ok   covariant-variadic.cpp (-O$lvl): the overridden slots return the adjusted pointer and reference, the two bodies share their static locals (5 lines)" }
        else { Write-Host "  FAIL covariant-variadic.cpp (-O$lvl, exit $LASTEXITCODE):"; Write-Host $got; $failed++ }
    }
    $asmCov = if (Test-Path "out\x64\covvbx_06.asm") { Get-Content "out\x64\covvbx_06.asm" -Raw } else { "" }
    if ($asmCov.Contains('DQ OFFSET ?get@VL@@$R4BA@7PPPPPPPM@CI@EAAPEAUX@@XZ') -and $asmCov.Contains('DQ OFFSET ?ref@VL@@$R4BA@7PPPPPPPM@CI@EAAAEAUX@@HHHHH@Z')) { Write-Host "  ok   covariant-vbase.cpp (-O0): WL's slots hold the vtordispex thunks to the return thunks (`$R4 named with the slot's return type)" }
    else { Write-Host "  FAIL covariant-vbase.cpp (-O0): the `$R4 thunks returning X* and X& are not in WL's vftable"; $failed++ }
    # an adjustor thunk that moves this forward (VL::get in the vftable of VL's virtual base): the adjustment spelled as
    # cl spells it, an unsigned 32-bit number (cl's object has ?get@VL@@WPPPPPPMI@EAAPEAUY@@XZ; it was W?DI@) (G16)
    if ($asmCov.Contains('?get@VL@@WPPPPPPMI@EAAPEAUY@@XZ') -and -not $asmCov.Contains('@VL@@W?')) { Write-Host "  ok   covariant-vbase.cpp (-O0): the adjustor thunk with a negative adjustment is named as cl's (WPPPPPPMI@)" }
    else { Write-Host "  FAIL covariant-vbase.cpp (-O0): the adjustor thunk of VL::get is not named ?get@VL@@WPPPPPPMI@EAAPEAUY@@XZ"; $failed++ }
    # vbase-first.cpp: classes that name a virtual base before the polymorphic base whose function they override (G45:
    # the override was left out of the table at offset 0, a call through that base reached the base's function). Built
    # by cl (the reference, /Od) and by cppparse at -O0 and -O2 in each of the three exception models: the programs
    # print the same lines. And every vftable of cl's listing is in cppparse's -O0 listing with the same slots, the
    # adjustor thunks by their names (W<adjustment>), those of namespace covm apart (G47, the file says why)
    function Get-VfTables([string]$asm) {
        $tables = @{}; $cur = $null
        foreach ($l in (Get-Content $asm)) {
            if ($l -match '^(\?\?_7\S+)\s+DQ\s+FLAT:\S+') { $cur = $Matches[1]; $tables[$cur] = @(); continue }      # cl: the label on the line before the first slot
            if ($l -match '^(\?\?_7\S+)\s+LABEL\s+QWORD') { $cur = $Matches[1]; $tables[$cur] = @(); continue }      # cppparse: a line of its own
            if ($null -ne $cur -and $l -match '^\s+DQ\s+(?:FLAT:|OFFSET\s+)(\S+)') { $tables[$cur] += $Matches[1]; continue }
            $cur = $null
        }
        return $tables
    }
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /FAs "/Faout\x64\vbfirst_cl.asm" "/Foout\x64\vbfirst_cl.obj" "..\examples\sample-front\vbase-first.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-front\vbase-first.cpp" }
    $clOut = Link-Program @("out\x64\vbfirst_cl.obj") "out\x64\vbfirst_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl's object of sample-front\vbase-first.cpp does not link" }
    $wantVbf = ((& ".\out\x64\vbfirst_cl.exe" 2>&1 | Out-String) -replace "`r", "")
    $vbfLines = @($wantVbf -split "`n" | Where-Object { $_.Trim() }).Count
    if ($LASTEXITCODE -ne 0 -or $vbfLines -lt 80) { Write-Host "  FAIL vbase-first.cpp: cl's build exits with $LASTEXITCODE and prints $vbfLines lines"; $failed++ }
    else {
        foreach ($lvl in @(0, 2)) {
            $sameIn = @()
            foreach ($eh in @("table", "sjlj", "cl")) {
                $nm = "vbfirst_${lvl}_$eh"
                Remove-Item "out\x64\${nm}6.obj", "out\x64\${nm}6.asm" -ErrorAction SilentlyContinue
                $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
                $c6 = & $exe --compile6 "-O$lvl" "--eh=$eh" --obj --msvc-mangling -I ..\include -o out\x64 -n $nm "..\examples\sample-front\vbase-first.cpp" 2>&1 | Out-String
                $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
                if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-front\vbase-first.cpp at -O$lvl --eh=${eh}:"; Write-Host ($c6 -split "`n" | Where-Object { $_ -match 'error' } | Select-Object -First 6); $failed++; continue }
                $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
                if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
                $got = ((& ".\out\x64\${nm}.exe" 2>&1 | Out-String) -replace "`r", "")
                if ($LASTEXITCODE -eq 0 -and $got -eq $wantVbf) { $sameIn += $eh; continue }
                Write-Host "  FAIL vbase-first.cpp (-O$lvl --eh=$eh, exit $LASTEXITCODE) differs from cl's build:"
                $wantL = $wantVbf -split "`n"; $gotL = $got -split "`n"; $shown = 0
                for ($li = 0; $li -lt [Math]::Max($wantL.Count, $gotL.Count) -and $shown -lt 8; $li++) { if ($wantL[$li] -ne $gotL[$li]) { Write-Host "         cl:       $($wantL[$li])"; Write-Host "         cppparse: $($gotL[$li])"; $shown++ } }
                $failed++
            }
            if ($sameIn.Count -eq 3) { Write-Host "  ok   vbase-first.cpp (-O$lvl): the overrides reach the tables of the bases named after a virtual base; prints what cl's build prints in the models $($sameIn -join ', ') ($vbfLines lines)" }
        }
        if (Test-Path "out\x64\vbfirst_0_table6.asm") {
            $clVf = Get-VfTables "out\x64\vbfirst_cl.asm"; $ourVf = Get-VfTables "out\x64\vbfirst_0_table6.asm"
            $vfDiff = @(); $vfSame = 0; $vfApart = 0
            foreach ($k in ($clVf.Keys | Sort-Object)) {
                if ($k -match '@covm@@') { $vfApart++; continue }
                if (-not $ourVf.ContainsKey($k)) { $vfDiff += "$k is not in cppparse's listing"; continue }
                $slotsCl = $clVf[$k] -join ' '; $slotsOur = $ourVf[$k] -join ' '
                if ($slotsCl -eq $slotsOur) { $vfSame++ } else { $vfDiff += "${k}: cl [$slotsCl], cppparse [$slotsOur]" }
            }
            if ($vfDiff.Count -eq 0 -and $vfSame -ge 200) { Write-Host "  ok   vbase-first.cpp (-O0): the $vfSame vftables hold what cl's hold, slot for slot, adjustor thunks included ($vfApart of namespace covm apart, G47)" }
            else { Write-Host "  FAIL vbase-first.cpp (-O0): $($vfDiff.Count) vftable(s) differ from cl's, $vfSame are the same:"; $vfDiff | Select-Object -First 8 | ForEach-Object { Write-Host "         $_" }; $failed++ }
        }
    }
    # a pointer and a reference to an array in a function's decorated name, as cl's (front2.cpp tiarray::byPtr, byRef)
    $asmF2 = if (Test-Path "out\x64\front2_06.asm") { Get-Content "out\x64\front2_06.asm" -Raw } else { "" }
    if ($asmF2.Contains('?byPtr@tiarray@@YAHPEAY02H@Z PROC') -and $asmF2.Contains('?byRef@tiarray@@YAHAEAY01$$CBH@Z PROC')) { Write-Host "  ok   front2.cpp (-O0): a pointer and a reference to an array decorate as cl's (PEAY02H, AEAY01`$`$CBH)" }
    else { Write-Host "  FAIL front2.cpp (-O0): ?byPtr@tiarray@@YAHPEAY02H@Z or ?byRef@tiarray@@YAHAEAY01`$`$CBH@Z is not in the listing"; $failed++ }
    # MatrixTest.cpp (Matrix.hpp, Vector.hpp; asserts and streams) against cl: member templates of class templates,
    # injected class names with arguments, a function template declared before its definition. std::cout comes
    # from the libcpp/iostream.cpp objects of test 15
    $clOut = Build-Program @("/EHsc", "/W3", "/I..", "/Foout\x64\", "..\MatrixTest.cpp") @("out\x64\MatrixTest.obj") "out\x64\matrixtest_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile MatrixTest.cpp" }
    $want = & ".\out\x64\matrixtest_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "matrixtest$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -I ..\include -I .. -o out\x64 -n $nm "..\MatrixTest.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of MatrixTest.cpp at -O$lvl :"; Write-Host ($c6 -split "`n" | Select-Object -First 20); $failed++; continue }
        $clOut = Link-Program @("out\x64\${nm}6.obj", "out\x64\iostreamL${lvl}6.obj", "out\x64\cppparse_rt.obj") "out\x64\${nm}.exe" | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of ${nm}6.obj:"; Write-Host $clOut; $failed++; continue }
        $got = & ".\out\x64\${nm}.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $got -eq $want) { Write-Host "  ok   MatrixTest.cpp (-O$lvl) runs and prints what cl's build prints ($(($want -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
        else { Write-Host "  FAIL MatrixTest.cpp (-O$lvl, exit $LASTEXITCODE) differs from cl's build:"; Write-Host "  cl:"; Write-Host $want; Write-Host "  cppparse:"; Write-Host $got; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed front-end check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 18: CodeView line tables (.debug`$S) read back through dbghelp ==="
    # sample-dbg\lines.cpp built by cppparse at -O0 and -O2 and linked with /DEBUG; the object must hold a .debug$S for
    # .text and an associative one per COMDAT function; dbgcheck.exe (cl, dbghelp) asks the PDB for the line at each
    # function's start (SymGetLineFromAddr64) and the lines that follow in it (SymGetLineNext64): the start is the
    # function's own line, every line lies in its body, and the statement lines expected at that level are there
    $failed = 0
    $clOut = Build-Program @("/EHsc", "/W3", "/Foout\x64\", "..\examples\sample-dbg-driver\dbgcheck.cpp") @("out\x64\dbgcheck.obj") "out\x64\dbgcheck.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\dbgcheck.cpp" }
    $ranges = @{ twice = @(6, 11); main = @(24, 30); "Acc::add" = @(16, 21) }
    foreach ($lvl in @(0, 2)) {
        $nm = "lines$lvl"
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -o out\x64 -n $nm "..\examples\sample-dbg\lines.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "out\x64\${nm}6.obj")) { Write-Host "  FAIL --compile6 of sample-dbg\lines.cpp at -O$lvl :"; Write-Host $c6; $failed++; continue }
        $hdr = & $dumpbin /nologo /headers "out\x64\${nm}6.obj" | Out-String
        $dbg = ([regex]::Matches($hdr, '\.debug\$S name')).Count
        if ($dbg -ge 3) { Write-Host "  ok   ${nm}6.obj has $dbg .debug`$S sections (the functions of .text, one per COMDAT function)" } else { Write-Host "  FAIL ${nm}6.obj has $dbg .debug`$S section(s)"; $failed++ }
        $lo = Link-Program @("out\x64\${nm}6.obj", "out\x64\cppparse_rt.obj") "out\x64\$nm.exe" @("/DEBUG") | Out-String
        if ($LASTEXITCODE -ne 0 -or $lo -match 'warning') { Write-Host "  FAIL link /DEBUG of ${nm}6.obj:"; Write-Host $lo; $failed++; continue }
        $run = & ".\out\x64\$nm.exe" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $run -notmatch 'total 20') { Write-Host "  FAIL $nm.exe:"; Write-Host $run; $failed++; continue }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $got = & ".\out\x64\dbgcheck.exe" "out\x64\$nm.exe" twice main "Acc::add" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        # -O0: every statement line; -O2: the lines the optimiser keeps (twice folds to one return, main's loop is left)
        $want = if ($lvl -eq 0) { @{ twice = @(8, 9, 10); main = @(26, 27, 28, 29); "Acc::add" = @(18, 19) } } else { @{ twice = @(10); main = @(27, 28, 29); "Acc::add" = @(18) } }
        $problems = @()
        if ($rc -ne 0) { $problems += "dbgcheck exit $rc" }
        foreach ($fn in $ranges.Keys) {
            $m = [regex]::Match($got, "(?m)^" + [regex]::Escape($fn) + ": lines\.cpp (\d+)((?: \d+)*)\s*$")
            if (-not $m.Success) { $problems += "$fn not reported"; continue }
            $lines = @([int]$m.Groups[1].Value) + @($m.Groups[2].Value.Trim() -split ' ' | Where-Object { $_ } | ForEach-Object { [int]$_ })
            if ($lines[0] -ne $ranges[$fn][0]) { $problems += "$fn starts at line $($lines[0]), not $($ranges[$fn][0])" }
            foreach ($l in $lines) { if ($l -lt $ranges[$fn][0] -or $l -gt $ranges[$fn][1]) { $problems += "$fn has line $l outside its body" } }
            foreach ($l in $want[$fn]) { if ($lines -notcontains $l) { $problems += "$fn lacks line $l" } }
        }
        if ($problems.Count -eq 0) { Write-Host "  ok   dbghelp maps $nm.exe's code to lines.cpp (-O$lvl): $((($got -split "`n") | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() }) -join '; ')" }
        else { Write-Host "  FAIL dbghelp on $nm.exe (-O$lvl): $($problems -join '; ')"; Write-Host $got; $failed++ }
        # every line record lies on an instruction start (the COFF writer moves the records with the code when it shortens
        # a jump): dbgcheck -offsets gives each record's offset in its function, dumpbin /disasm the instructions
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $offs = & ".\out\x64\dbgcheck.exe" -offsets "out\x64\$nm.exe" twice main "Acc::add" 2>&1 | Out-String
        $ErrorActionPreference = $eap
        $dis = & $dumpbin /nologo /disasm "out\x64\$nm.exe" | Out-String
        $labels = @{ twice = '\?twice@@YAHH@Z'; main = 'main'; "Acc::add" = '\?add@Acc@@QEAAXH@Z' }
        $bad = @(); $records = 0
        foreach ($fn in $labels.Keys) {
            $m = [regex]::Match($offs, "(?m)^" + [regex]::Escape($fn) + ": size (\d+)((?: \d+:\d+)+)\s*$")
            if (-not $m.Success) { $bad += "$fn has no records"; continue }
            $body = [regex]::Match($dis, "(?ms)^" + $labels[$fn] + ":\r?\n(.*?)(?=^\S+:\r?$)")
            if (-not $body.Success) { $bad += "$fn not in the disassembly"; continue }
            $addrs = @([regex]::Matches($body.Groups[1].Value, '(?m)^  ([0-9A-F]{16}): ') | ForEach-Object { [Convert]::ToInt64($_.Groups[1].Value, 16) })
            $starts = @{}; foreach ($ad in $addrs) { $starts[[int]($ad - $addrs[0])] = $true }
            $prev = -1
            foreach ($rec in ($m.Groups[2].Value.Trim() -split ' ')) {
                $off = [int]($rec -split ':')[0]; $records++
                if (-not $starts.ContainsKey($off)) { $bad += "$fn record at offset $off is inside an instruction" }
                if ($off -le $prev) { $bad += "$fn records out of order at offset $off" }
                $prev = $off
            }
        }
        if ($bad.Count -eq 0) { Write-Host "  ok   $nm.exe (-O$lvl): all $records line records of twice, main and Acc::add lie on instruction starts" }
        else { Write-Host "  FAIL $nm.exe (-O$lvl) line record offsets: $($bad -join '; ')"; Write-Host $offs; $failed++ }
    }
    # the comparator of "debug information changes no code" (scripts\cvsame.ps1, docs\design-codeview.md 5.5/1) on
    # cases with known answers, made from lines06.obj: it has to find every change outside the debug sections and to
    # let pass what -g may add
    if (Test-Path "out\x64\lines06.obj") {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $st = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -SelfTest -A "out\x64\lines06.obj" -B "out\x64\cvsame-selftest" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -eq 0 -and $st -match '(\d+) cases with known answers, 0 wrong') { Write-Host "  ok   cvsame.ps1 gives the expected verdict on $($Matches[1]) objects and listings made from lines06.obj" }
        else { Write-Host "  FAIL cvsame.ps1 -SelfTest (exit $rc):"; Write-Host $st; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed CodeView check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 19: version 6 self-compile (--compile6 -O0 of cppparse's own sources and libcpp, linked and run) ==="
    # row P3: a change to the front end, the back end or a header that breaks the self-host fails the suite, not only
    # selfhost.ps1. The units compile to out\self6, one process each and as many at a time as there are processors;
    # the objects are linked with libcpp and the exception runtime, and the program must compile sample-opt to the
    # object and listings the compiler under test writes. selfhost.ps1 stays the full check (-O2, generation two).
    $failed = 0
    $selfDir = "$Sandbox\out\self6"
    New-Item -ItemType Directory -Force $selfDir, "$selfDir\run", "$selfDir\ref" | Out-Null
    Get-ChildItem $selfDir -File -Recurse | Remove-Item
    $selfInc = @(); foreach ($sdir in $SourceDirs) { $selfInc += "-I `"$Src\cppparse\$sdir`"" }
    $selfInc += @("-I `"$Src\include`"", "-I `"$Src\platform`"")
    $selfJobs = New-Object System.Collections.ArrayList
    foreach ($sdir in $SourceDirs) {
        foreach ($f in Get-ChildItem "$Src\cppparse\$sdir\*.cpp") {
            $null = $selfJobs.Add(@{ Name = $f.BaseName; Args = "--compile6 -O0 --obj --msvc-mangling -q $($selfInc -join ' ') -o `"$selfDir`" -n $($f.BaseName) `"$($f.FullName)`"" })
        }
    }
    $selfUnits = $selfJobs.Count
    $null = $selfJobs.Add(@{ Name = "iostream"; Args = "--compile6 -O0 --obj --msvc-mangling -q -I `"$Src\include`" -o `"$selfDir`" -n iostream `"$Src\libcpp\iostream.cpp`"" })
    $selfStart = Get-Date
    $running = New-Object System.Collections.ArrayList
    $next = 0; $slots = [Math]::Max(1, [Environment]::ProcessorCount)
    $selfBad = @()
    while ($next -lt $selfJobs.Count -or $running.Count -gt 0) {
        while ($next -lt $selfJobs.Count -and $running.Count -lt $slots) {
            $job = $selfJobs[$next]; $next++
            $proc = Start-Process -FilePath $exe -ArgumentList $job.Args -WorkingDirectory $Sandbox -NoNewWindow -PassThru -RedirectStandardOutput "$selfDir\$($job.Name).out" -RedirectStandardError "$selfDir\$($job.Name).err"
            $null = $proc.Handle   # keeps the process handle so that ExitCode is available after the exit
            $null = $running.Add(@{ Job = $job; Proc = $proc })
        }
        $null = $running[0].Proc.WaitForExit(250)
        foreach ($r in @($running)) {
            if (-not $r.Proc.HasExited) { continue }
            $running.Remove($r)
            if ($r.Proc.ExitCode -ne 0 -or -not (Test-Path "$selfDir\$($r.Job.Name)6.obj")) { $selfBad += "$($r.Job.Name) (exit $($r.Proc.ExitCode))" }
        }
    }
    $selfSecs = ((Get-Date) - $selfStart).TotalSeconds
    if ($selfBad.Count -eq 0) { Write-Host ("  ok   all {0} own sources and libcpp\iostream.cpp compile at -O0 ({1:N0} s, {2} at a time)" -f $selfUnits, $selfSecs, $slots) }
    else {
        Write-Host "  FAIL $($selfBad.Count) unit(s) not compiled: $($selfBad -join ', ')"
        foreach ($b in @($selfBad | Select-Object -First 3)) {
            $nm = ($b -split ' ')[0]
            Get-Content "$selfDir\$nm.out", "$selfDir\$nm.err" -ErrorAction SilentlyContinue | Select-String -Pattern "error|note" | Select-Object -First 4 | ForEach-Object { Write-Host "    $($_.Line.Trim())" }
        }
        $failed++
    }
    if ($failed -eq 0) {
        $selfObjs = @(); foreach ($j in $selfJobs) { $selfObjs += "$selfDir\$($j.Name)6.obj" }
        $lo = Link-Program ($selfObjs + @("out\x64\cppparse_rt.obj")) "$selfDir\cppparse-s6.exe" | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$selfDir\cppparse-s6.exe")) { Write-Host "  FAIL link of the self-compiled objects:"; Write-Host $lo; $failed++ }
        else {
            Write-Host "  ok   $($selfObjs.Count) objects, the exception runtime and the C runtime link to out\self6\cppparse-s6.exe"
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $c6 = & "$selfDir\cppparse-s6.exe" --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\self6\run -n self-opt "..\examples\sample-opt\opt.cpp" 2>&1 | Out-String
            $rc = $LASTEXITCODE
            $null = & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o out\self6\ref -n self-opt "..\examples\sample-opt\opt.cpp" 2>&1
            $ErrorActionPreference = $eap
            $differ = @()
            foreach ($suffix in @("6.obj", "6.asm", "-ir6.txt", "-asm6.txt", "-sym6.txt")) {
                $fa = "$selfDir\run\self-opt$suffix"; $fb = "$selfDir\ref\self-opt$suffix"
                if (-not (Test-Path $fa) -or -not (Test-Path $fb) -or -not [System.Linq.Enumerable]::SequenceEqual([byte[]][System.IO.File]::ReadAllBytes($fa), [byte[]][System.IO.File]::ReadAllBytes($fb))) { $differ += $suffix }
            }
            if ($rc -eq 0 -and $differ.Count -eq 0) { Write-Host "  ok   cppparse-s6.exe compiles sample-opt to the object and listings the compiler under test writes" }
            else { Write-Host "  FAIL cppparse-s6.exe on sample-opt (exit $rc), differs: $($differ -join ', ')"; Write-Host $c6; $failed++ }
        }
    }
    if ($failed -ne 0) { throw "$failed self-compile check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 20: CodeView types and globals (-g): .debug`$T and the data symbols read back through dbghelp ==="
    # row M10, docs\design-codeview.md stage 1. sample-dbg\types.cpp and types2.cpp, one program of two units, built with
    # -g at -O0 and -O2:
    #   - the objects with -g against the objects without: cvsame.ps1 (debug information changes no code)
    #   - linked with /DEBUG by Microsoft's link and, where GRIDE is found, by GRIDE's: no message from either
    #   - the program prints what it knows of its own types (sizes, member offsets measured in objects, bit-fields found
    #     by storing into them, base offsets, enumerators, the sizes of its variables); sample-dbg-driver\dbgtypes.cpp
    #     (cl, dbghelp) prints the same facts from the PDB: every line of the program must be one of the driver's
    #   - the driver's lines about the program's types and variables are the same from both linkers' PDBs
    #   - the same source by cl /Z7 (cppparse's headers, /X): the driver's description of the program's types, every
    #     member with its type, is the same text for cl's build and for cppparse's
    #   - mixed: types.cpp by cppparse -g and types2.cpp by cl /Z7 in one program (the classes of types.h are then in the
    #     PDB from both compilers): the facts hold as before
    #   - llvm-pdbutil (where Visual Studio has it) dumps the -g objects without an error
    #   - the member functions (stage 4): sample-dbg-driver\diatypes.cpp (cl, the DIA SDK of Visual Studio, where there
    #     is one) lists every member function of the program's classes with its access, kind (static, virtual, the
    #     vftable slot it introduces, pure), type and this adjustment: the same lines for cl's build and for cppparse's,
    #     what cl generates itself apart, from both linkers' PDBs and in the mixed program
    # and at -O0 the self-compiled objects of test 19 against the same sources compiled with -g: cvsame over all of them
    $failed = 0
    $tdir = "$Sandbox\out\dbgtypes"
    New-Item -ItemType Directory -Force $tdir | Out-Null
    Get-ChildItem $tdir -File -Recurse | Remove-Item
    $linkExe = "$Msvc\bin\Hostx64\x64\link.exe"
    $gride = & { . "$Sandbox\scripts\toolset.ps1"; ,@($GrideRoot, $GrideLink) }
    $grideLink = $gride[1]
    $pdbutil = "$VsRoot\VC\Tools\Llvm\x64\bin\llvm-pdbutil.exe"
    $libs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
    # (an object file name, not a folder: "/Fo<folder>\" ends in \" when the path has a space, which cl reads as a quote)
    $clOut = Build-Program @("/EHsc", "/W3", "/Fo$tdir\dbgtypes.obj", "..\examples\sample-dbg-driver\dbgtypes.cpp") @("$tdir\dbgtypes.obj") "$tdir\dbgtypes.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\dbgtypes.cpp" }
    $diaSdk = "$VsRoot\DIA SDK"
    $diaTypes = ""
    if (Test-Path "$diaSdk\include\dia2.h") {
        $clOut = Build-Program @("/EHsc", "/W3", "/I$diaSdk\include", "/Fo$tdir\diatypes.obj", "..\examples\sample-dbg-driver\diatypes.cpp") @("$tdir\diatypes.obj", "diaguids.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib") "$tdir\diatypes.exe" @("/LIBPATH:$diaSdk\lib\amd64") | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\diatypes.cpp" }
        $diaTypes = "$tdir\diatypes.exe"
    }
    foreach ($u in "types", "types2") {
        $clOut = & $cl /nologo /c /EHsc /W3 /Od /Z7 /X /I..\include "/Fo$tdir\$u-cl.obj" "..\examples\sample-dbg\$u.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg\$u.cpp" }
    }
    # the driver's lines about what the program reports: the types of its "size" and "enumerator" lines, its variables
    function Select-About([string[]]$own, [string[]]$driver) {
        $names = New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($l in $own) {
            if ($l -match '^(?:size|global) (.+) \d+$') { $null = $names.Add($Matches[1]) }
            elseif ($l -match '^enumerator (.+) \S+ -?\d+$') { $null = $names.Add($Matches[1]) }
        }
        $out = New-Object System.Collections.ArrayList
        foreach ($l in $driver) {
            $sp = $l.IndexOf(' '); if ($sp -lt 0) { continue }
            $rest = $l.Substring($sp + 1)
            foreach ($n in $names) { if ($rest -eq $n -or $rest.StartsWith("$n ")) { $null = $out.Add($l); break } }
        }
        return ,@($out | Sort-Object)
    }
    # links one build of the program, runs it and the driver; returns the program's lines and the driver's
    function Test-TypesBuild([string]$what, [string]$linker, [string[]]$objs, [string]$exeName) {
        $texe = "$tdir\$exeName.exe"
        $lo = & $linker /nologo /DEBUG /INCREMENTAL:NO /SUBSYSTEM:CONSOLE "/OUT:$texe" @objs "out\x64\cppparse_rt.obj" @libs 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $texe) -or $lo.Trim() -ne "") { Write-Host "  FAIL $what, link /DEBUG (it has to say nothing):"; Write-Host $lo; return $null }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $own = @(& $texe 2>&1 | ForEach-Object { "$_".TrimEnd() }); $rc = $LASTEXITCODE
        $drv = @(& "$tdir\dbgtypes.exe" $texe -types -local localSizes s_local -local localSizesOther s_local 2>&1 | ForEach-Object { "$_".TrimEnd() }); $drc = $LASTEXITCODE
        $meth = @(); $mrc = 0
        if ($diaTypes -ne "") { $meth = @(& $diaTypes "$diaSdk\bin\amd64\msdia140.dll" $texe 2>&1 | ForEach-Object { "$_".TrimEnd() }); $mrc = $LASTEXITCODE }
        $ErrorActionPreference = $eap
        if ($mrc -ne 0) { Write-Host "  FAIL ${what}: diatypes exits with ${mrc}: $($meth | Select-Object -First 2)"; return $null }
        if ($rc -ne 0 -or $own.Count -lt 100) { Write-Host "  FAIL ${what}: the program exits with $rc and prints $($own.Count) lines"; return $null }
        $set = New-Object 'System.Collections.Generic.HashSet[string]'; foreach ($l in $drv) { $null = $set.Add($l) }
        $missing = @($own | Where-Object { -not $set.Contains($_) })
        if ($drc -ne 0 -or $missing.Count -ne 0) {
            Write-Host "  FAIL ${what}: dbgtypes exit $drc; $($missing.Count) of the program's $($own.Count) facts are not what the PDB says:"
            $missing | Select-Object -First 8 | ForEach-Object { Write-Host "         $_" }
            return $null
        }
        return @{ Own = $own; About = (Select-About $own $drv); Methods = (Select-About $own $meth) }
    }
    # the member functions of two builds, as DIA lists them: the same lines, and enough of them
    function Test-Methods([string]$what, $a, $b, [string]$ok) {
        if ($diaTypes -eq "") { return 0 }
        if (@($a.Methods).Count -eq 0 -or @($b.Methods).Count -eq 0) { Write-Host "  FAIL ${what}: DIA lists $(@($a.Methods).Count) and $(@($b.Methods).Count) member functions in the program's classes"; return 1 }
        $d = @(Compare-Object @($a.Methods) @($b.Methods))
        if ($d.Count -eq 0 -and @($b.Methods).Count -ge 90) { Write-Host "  ok   ${what}: $ok ($(@($b.Methods).Count) lines: access, kind, vftable slot, type, this adjustment)"; return 0 }
        Write-Host "  FAIL ${what}: the member functions DIA lists differ in $($d.Count) of $(@($b.Methods).Count) lines:"
        $d | Select-Object -First 8 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }
        return 1
    }
    if ($diaTypes -eq "") { Write-Host "  note the DIA SDK is not found: the member functions of the classes are not compared" }
    $clBuild = Test-TypesBuild "cl /Z7's build" $linkExe @("$tdir\types-cl.obj", "$tdir\types2-cl.obj") "types-cl"
    if ($null -eq $clBuild) { $failed++ }
    foreach ($lvl in @(0, 2)) {
        $plain = "$tdir\plain$lvl"
        New-Item -ItemType Directory -Force $plain | Out-Null
        $msOfG = $null
        # -g, and -g2: the same and the member functions in the classes
        foreach ($gopt in @("-g", "-g2")) {
            $at = "-O$lvl $gopt"
            $members = $gopt -eq "-g2"
            $withG = if ($members) { "$tdir\g2-$lvl" } else { "$tdir\g$lvl" }
            New-Item -ItemType Directory -Force $withG | Out-Null
            $bad = @()
            foreach ($u in "types", "types2") {
                $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
                if (-not $members) { $null = & $exe --compile6 "-O$lvl" -g0 --obj --msvc-mangling -q -I ..\include -o $plain -n $u "..\examples\sample-dbg\$u.cpp" 2>&1 }
                $c6 = & $exe --compile6 "-O$lvl" $gopt --obj --msvc-mangling -q -I ..\include -o $withG -n $u "..\examples\sample-dbg\$u.cpp" 2>&1 | Out-String
                $ErrorActionPreference = $eap
                if (-not (Test-Path "$plain\${u}6.obj") -or -not (Test-Path "$withG\${u}6.obj")) { $bad += $u; Write-Host $c6 }
            }
            if ($bad.Count) { Write-Host "  FAIL --compile6 $at of sample-dbg\$($bad -join ', ')"; $failed++; continue }
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $same = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -A $plain -B $withG -Listings 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -eq 0 -and $same -match '2 pair\(s\): 0 the same \(listings too\), 2 differ in debug information only, 0 violate') { Write-Host "  ok   ${at}: the two objects and listings are what they are without it but for CodeView (cvsame)" }
            else { Write-Host "  FAIL ${at}: cvsame on the objects without and with $gopt (exit $rc):"; Write-Host $same; $failed++ }
            if ($members) {
                # the member functions are described when the unit's code is generated: the listing, markers and all, is -g's
                $differ = @("types", "types2" | Where-Object { (Get-FileHash "$withG\${_}6.asm").Hash -ne (Get-FileHash "$tdir\g$lvl\${_}6.asm").Hash })
                if ($differ.Count -eq 0) { Write-Host "  ok   ${at}: the two listings are -g's, byte for byte" }
                else { Write-Host "  FAIL ${at}: the listing of $($differ -join ', ') is not -g's"; $failed++ }
            }
            $hdr = & $dumpbin /nologo /headers "$withG\types6.obj" | Out-String
            if ($hdr -notmatch '\.debug\$T name') { Write-Host "  FAIL ${at}: types6.obj has no .debug`$T"; $failed++ }
            $objs = @("$withG\types6.obj", "$withG\types26.obj")
            $tag = if ($members) { "types$lvl-g2" } else { "types$lvl" }
            $ms = Test-TypesBuild "$at, Microsoft's link" $linkExe $objs "$tag-ms"
            if ($null -eq $ms) { $failed++; continue }
            Write-Host "  ok   ${at}: link /DEBUG says nothing; all $($ms.Own.Count) facts the program prints of its types and variables are the PDB's (dbghelp)"
            if ($null -ne $clBuild) {
                $d = @(Compare-Object $clBuild.About $ms.About)
                if ($d.Count -eq 0) { Write-Host "  ok   ${at}: dbghelp's description of the program's types ($($ms.About.Count) lines: members with their types, bases, enumerators) is the one of cl /Z7's build" }
                else { Write-Host "  FAIL ${at}: $($d.Count) lines of the type description differ from cl /Z7's build:"; $d | Select-Object -First 8 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
                if ($members) { $failed += (Test-Methods $at $clBuild $ms "the member functions DIA lists in the program's classes are those of cl /Z7's build, what cl generates itself apart") }
            }
            if (-not $members) {
                $msOfG = $ms
                if ($diaTypes -ne "" -and @($ms.Methods).Count -ne 0) { Write-Host "  FAIL ${at}: DIA lists $(@($ms.Methods).Count) member functions in the classes, which are -g2's"; $failed++ }
            }
            elseif ($null -ne $msOfG -and @(Compare-Object $msOfG.About $ms.About).Count -ne 0) { Write-Host "  FAIL ${at}: dbghelp's description of the types is not -g's"; $failed++ }
            $mixed = Test-TypesBuild "$at, types.cpp by cppparse and types2.cpp by cl" $linkExe @("$withG\types6.obj", "$tdir\types2-cl.obj") "$tag-mixed-ms"
            if ($null -eq $mixed) { $failed++ }
            elseif (@(Compare-Object $ms.About $mixed.About).Count -eq 0) { Write-Host "  ok   ${at}: mixed with cl /Z7's object of types2.cpp, the facts hold and the description is the same" }
            else { Write-Host "  FAIL ${at}: mixed with cl's object the description differs"; $failed++ }
            if ($members -and $null -ne $mixed) { $failed += (Test-Methods "$at, mixed" $ms $mixed "the member functions are the same lines") }
            if ($grideLink -ne "") {
                $gr = Test-TypesBuild "$at, GRIDE's link" $grideLink $objs "$tag-gr"
                $grMixed = Test-TypesBuild "$at, mixed, GRIDE's link" $grideLink @("$withG\types6.obj", "$tdir\types2-cl.obj") "$tag-mixed-gr"
                if ($null -eq $gr -or $null -eq $grMixed) { $failed++ }
                elseif (@(Compare-Object $ms.About $gr.About).Count -eq 0 -and @(Compare-Object $ms.About $grMixed.About).Count -eq 0) { Write-Host "  ok   ${at}: GRIDE's link says nothing, and its PDB gives the driver the same $($gr.About.Count) lines, mixed too" }
                else { Write-Host "  FAIL ${at}: the driver's lines differ between Microsoft's PDB and GRIDE's"; Compare-Object $ms.About $gr.About | Select-Object -First 8 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
                if ($members -and $null -ne $gr -and $null -ne $grMixed) {
                    $failed += (Test-Methods "$at, GRIDE's link" $ms $gr "the member functions from GRIDE's PDB are the lines from Microsoft's")
                    $failed += (Test-Methods "$at, mixed, GRIDE's link" $ms $grMixed "the member functions are the same lines")
                }
            } else { Write-Host "  note ${at}: GRIDE not found, its link is not tried" }
            if (Test-Path $pdbutil) {
                $perr = @(); $listed = 0
                foreach ($o in $objs) {
                    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
                    $dump = @(& $pdbutil dump -types -symbols $o 2>&1 | ForEach-Object { "$_" }); $prc = $LASTEXITCODE
                    $ErrorActionPreference = $eap
                    # the tool's own complaints (the word "error" alone may be part of a name)
                    $perr += @($dump | Where-Object { $_ -match '(?i)^\s*error\b|: error:|llvm-pdbutil(\.exe)?:|corrupt|invalid (type|record|leaf)|unknown (leaf|record|symbol)' } | Select-Object -First 2)
                    if ($prc -ne 0) { $perr += "exit $prc on $o" }
                    if ($dump.Count -lt 50) { $perr += "only $($dump.Count) lines for $o" }
                    $listed += @($dump | Where-Object { $_ -match '- LF_ONEMETHOD \[|- Method \[type = ' }).Count
                }
                # (the classes list their member functions with -g2 alone: LF_ONEMETHOD, and LF_METHOD with its LF_METHODLIST)
                if ($members -and $listed -lt 100) { $perr += "only $listed member functions are listed in the classes of the two objects" }
                if (-not $members -and $listed -ne 0) { $perr += "$listed member functions are listed in the classes, which are -g2's" }
                if ($perr.Count -eq 0) { Write-Host "  ok   ${at}: llvm-pdbutil dumps the types and symbols of both objects without an error; the classes list $listed member functions" }
                else { Write-Host "  FAIL ${at}: llvm-pdbutil: $($perr -join ' | ')"; $failed++ }
            }
        }
    }
    # invariant 1 on the compiler's own sources: test 19's objects (-O0, out\self6) and the same units with -g, and
    # with -g2 (the member functions of every class the compiler has)
    if ($env:CPPPARSE_G -eq "1" -or $env:CPPPARSE_G -eq "2") { Write-Host "  note CPPPARSE_G=$($env:CPPPARSE_G): test 19's objects have -g already, the own sources are not compared" }
    elseif (Test-Path "$Sandbox\out\self6\main6.obj") {
        $sizeG = 0
        foreach ($gopt in @("-g", "-g2")) {
            $gdir = if ($gopt -eq "-g2") { "$Sandbox\out\self6g2" } else { "$Sandbox\out\self6g" }
            New-Item -ItemType Directory -Force $gdir | Out-Null
            Get-ChildItem $gdir -File | Remove-Item
            $gJobs = New-Object System.Collections.ArrayList
            foreach ($j in $selfJobs) { $null = $gJobs.Add(@{ Name = $j.Name; Args = ($j.Args -replace [regex]::Escape("-o `"$selfDir`""), "$gopt -o `"$gdir`"") }) }
            $gStart = Get-Date
            $running = New-Object System.Collections.ArrayList
            $next = 0; $gBad = @()
            while ($next -lt $gJobs.Count -or $running.Count -gt 0) {
                while ($next -lt $gJobs.Count -and $running.Count -lt $slots) {
                    $job = $gJobs[$next]; $next++
                    $proc = Start-Process -FilePath $exe -ArgumentList $job.Args -WorkingDirectory $Sandbox -NoNewWindow -PassThru -RedirectStandardOutput "$gdir\$($job.Name).out" -RedirectStandardError "$gdir\$($job.Name).err"
                    $null = $proc.Handle
                    $null = $running.Add(@{ Job = $job; Proc = $proc })
                }
                $null = $running[0].Proc.WaitForExit(250)
                foreach ($r in @($running)) {
                    if (-not $r.Proc.HasExited) { continue }
                    $running.Remove($r)
                    if ($r.Proc.ExitCode -ne 0 -or -not (Test-Path "$gdir\$($r.Job.Name)6.obj")) { $gBad += "$($r.Job.Name) (exit $($r.Proc.ExitCode))" }
                }
            }
            if ($gBad.Count -ne 0) { Write-Host "  FAIL with $gopt $($gBad.Count) own unit(s) are not compiled: $($gBad -join ', ')"; Get-Content "$gdir\$(($gBad[0] -split ' ')[0]).err" -ErrorAction SilentlyContinue | Select-Object -First 4 | ForEach-Object { Write-Host "    $_" }; $failed++ }
            else {
                $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
                $same = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -A $selfDir -B $gdir -Listings -Quiet 2>&1 | Out-String
                $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
                $sizeA = (Get-ChildItem "$selfDir\*.obj" | Measure-Object Length -Sum).Sum; $sizeB = (Get-ChildItem "$gdir\*.obj" | Measure-Object Length -Sum).Sum
                if ($rc -eq 0 -and $same -match "$($gJobs.Count) pair\(s\): 0 the same \(listings too\), $($gJobs.Count) differ in debug information only, 0 violate") {
                    $beside = if ($gopt -eq "-g2" -and $sizeG -gt 0) { "; {0:P1} more than -g's" -f ($sizeB / $sizeG - 1) } else { "" }
                    Write-Host ("  ok   the {0} own sources and libcpp with {5} ({1:N0} s): every object and listing is what it is without it but for CodeView (cvsame); {2:N0} bytes of objects without -g, {3:N0} with ({4:P0} more{6})" -f $selfUnits, ((Get-Date) - $gStart).TotalSeconds, $sizeA, $sizeB, ($sizeB / $sizeA - 1), $gopt, $beside)
                } else { Write-Host "  FAIL cvsame on the own sources without and with $gopt (exit $rc):"; Write-Host $same; $failed++ }
                if ($gopt -eq "-g") { $sizeG = $sizeB }
            }
        }
    }
    if ($failed -ne 0) { throw "$failed CodeView type check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 21: CodeView variables (-g): the value harness, read back through dbghelp and DIA ==="
    # row M10, docs\design-codeview.md stages 2 and 3. sample-dbg\vars.cpp (functions of every shape the design names) by
    # -g at -O0, and at -O1, -O2 and -O3 (below), in each exception model, with sample-dbg-driver\varsck.cpp (cl:
    # checkpoint, ckPush, ckPop), linked with /DEBUG:
    #   - with -g the object and listing are what they are without it but for CodeView (cvsame)
    #   - dbgvars.exe (cl, dbghelp, the Win32 debug API) stops at every checkpoint and compares every variable the PDB gives
    #     a place for with what the program says it holds: in the checkpoint's caller, in the frames above it that
    #     announced themselves, and at the first address of the caller's next line; every declared parameter listed in
    #     order, a parameter the function never reads listed without a place
    #   - diavars.exe (cl, the DIA SDK of Visual Studio, where there is one) gives the same variables the same places at
    #     each checkpoint's caller
    #   - the table model's objects through the link variants: /DEBUG alone (incremental), /DEBUG /OPT:REF,ICF and from a
    #     static library, by Microsoft's link and by GRIDE's where it is found; gride-dbg's dv at eight checkpoint lines
    #     is the same from both linkers' programs
    $failed = 0
    $vdir = "$Sandbox\out\dbgvars"
    New-Item -ItemType Directory -Force $vdir | Out-Null
    Get-ChildItem $vdir -File -Recurse | Remove-Item
    $linkExe = "$Msvc\bin\Hostx64\x64\link.exe"
    $libExe = "$Msvc\bin\Hostx64\x64\lib.exe"
    $gride = & { . "$Sandbox\scripts\toolset.ps1"; ,@($GrideRoot, $GrideLink) }
    $grideLink = $gride[1]
    $libs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
    $drv = "..\examples\sample-dbg-driver"
    $clOut = Build-Program @("/EHsc", "/W3", "/Fo$vdir\dbgvars.obj", "$drv\dbgvars.cpp") @("$vdir\dbgvars.obj") "$vdir\dbgvars.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\dbgvars.cpp" }
    $clOut = & $cl /nologo /c /Od /W3 "/Fo$vdir\varsck.obj" "$drv\varsck.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\varsck.cpp" }
    $diaSdk = "$VsRoot\DIA SDK"
    $diaExe = ""
    if (Test-Path "$diaSdk\include\dia2.h") {
        $clOut = Build-Program @("/EHsc", "/W3", "/I$diaSdk\include", "/Fo$vdir\diavars.obj", "$drv\diavars.cpp") @("$vdir\diavars.obj", "diaguids.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib") "$vdir\diavars.exe" @("/LIBPATH:$diaSdk\lib\amd64") | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\diavars.cpp" }
        $diaExe = "$vdir\diavars.exe"
    }
    # links one build and runs dbgvars on it (and DIA with -Dia); returns the program's path or "" after a failure
    # -Optimized: a build at -O1 and up, where a variable may be listed without a place (dbgvars -optimized) but the ones
    # the program's texts mark must have theirs, and at least 60 values are read back
    function Test-VarsBuild([string]$what, [string]$linker, [string[]]$inputs, [string[]]$opts, [string]$exeName, [switch]$Dia, [switch]$Optimized) {
        $vexe = "$vdir\$exeName.exe"
        $lo = & $linker /nologo @opts /SUBSYSTEM:CONSOLE "/OUT:$vexe" @inputs "out\x64\cppparse_rt.obj" @libs 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $vexe) -or $lo.Trim() -ne "") { Write-Host "  FAIL ${what}: link $($opts -join ' ') (it has to say nothing):"; Write-Host $lo; return "" }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $dvArgs = @($vexe, "-places", "$vdir\$exeName.places.txt"); if ($Optimized) { $dvArgs += "-optimized" }
        $got = & "$vdir\dbgvars.exe" @dvArgs 2>&1 | Out-String; $rc = $LASTEXITCODE
        $ErrorActionPreference = $eap
        $sum = [regex]::Match($got, 'dbgvars: (\d+) checkpoints, (\d+) next lines, (\d+) values compared, (\d+) failures; (\d+) times a variable without a place')
        if ($rc -ne 0 -or -not $sum.Success -or [int]$sum.Groups[1].Value -lt 23 -or [int]$sum.Groups[2].Value -ne [int]$sum.Groups[1].Value -or ($Optimized -and [int]$sum.Groups[3].Value -lt 60)) { Write-Host "  FAIL ${what}: dbgvars (exit $rc):"; ($got -split "`n") | Where-Object { $_ -match 'FAIL|dbgvars:' } | Select-Object -First 12 | ForEach-Object { Write-Host "       $($_.Trim())" }; return "" }
        if ($Optimized) { Write-Host "  ok   ${what}: $($sum.Groups[1].Value) checkpoints, $($sum.Groups[3].Value) values right in the caller, the frames above and at the next line, none wrong, the marked ones all there; $($sum.Groups[5].Value) times a variable without a place (dbghelp)" }
        else { Write-Host "  ok   ${what}: $($sum.Groups[1].Value) checkpoints, $($sum.Groups[3].Value) values right in the caller, the frames above and at the next line (dbghelp)" }
        if ($Dia -and $diaExe -ne "") {
            # DIA at the same addresses: the same variables with the same places (dbghelp lists "this" as a local, DIA as
            # the object pointer: its kind is not compared)
            $mine = [ordered]@{}; $cur = $null
            foreach ($l in Get-Content "$vdir\$exeName.places.txt") { if ($l -match '^@(\w+) ') { $cur = $Matches[1]; $mine[$cur] = @() } elseif ($cur) { $mine[$cur] += ($l.Trim() -replace '^[PL] this ', 'X this ') } }
            $diaOut = & $diaExe "$diaSdk\bin\amd64\msdia140.dll" $vexe -rva @($mine.Keys) 2>&1
            $theirs = @{}; $cur = $null
            foreach ($l in $diaOut) { if ("$l" -match '^@(\w+) ') { $cur = $Matches[1]; $theirs[$cur] = @() } elseif ($cur -and "$l".Trim() -notmatch '^S ') { $theirs[$cur] += ("$l".Trim() -replace '^[PL] this ', 'X this ') } }
            $differ = @()
            foreach ($k in $mine.Keys) { if (-not $theirs.ContainsKey($k) -or @(Compare-Object @($mine[$k] | Sort-Object) @($theirs[$k] | Sort-Object)).Count -ne 0) { $differ += $k } }
            if ($differ.Count -eq 0) { Write-Host "  ok   ${what}: DIA gives the same variables the same places at all $($mine.Count) checkpoint callers" }
            else { Write-Host "  FAIL ${what}: DIA differs from dbghelp at $($differ.Count) address(es): $($differ -join ', ')"; return "" }
        }
        return $vexe
    }
    $grideExe = ""; $msExe = ""
    foreach ($eh in "table", "sjlj", "cl") {
        $plain = "$vdir\plain-$eh"; $withG = "$vdir\g-$eh"
        New-Item -ItemType Directory -Force $plain, $withG | Out-Null
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $null = & $exe --compile6 -O0 -g0 "--eh=$eh" --obj --msvc-mangling -q -I ..\include -o $plain -n vars "..\examples\sample-dbg\vars.cpp" 2>&1
        $c6 = & $exe --compile6 -O0 -g "--eh=$eh" --obj --msvc-mangling -q -I ..\include -o $withG -n vars "..\examples\sample-dbg\vars.cpp" 2>&1 | Out-String
        $ErrorActionPreference = $eap
        if (-not (Test-Path "$plain\vars6.obj") -or -not (Test-Path "$withG\vars6.obj")) { Write-Host "  FAIL --compile6 -O0 --eh=$eh of sample-dbg\vars.cpp:"; Write-Host $c6; $failed++; continue }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $same = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -A $plain -B $withG -Listings 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -eq 0 -and $same -match '1 pair\(s\): 0 the same \(listings too\), 1 differ in debug information only, 0 violate') { Write-Host "  ok   --eh=${eh}: with -g the object and listing are what they are without it but for CodeView (cvsame)" }
        else { Write-Host "  FAIL --eh=${eh}: cvsame on the object without and with -g (exit $rc):"; Write-Host $same; $failed++ }
        $inputs = @("$withG\vars6.obj", "$vdir\varsck.obj")
        $r = Test-VarsBuild "--eh=$eh, Microsoft's link" $linkExe $inputs @("/DEBUG", "/INCREMENTAL:NO") "vars-$eh-ms" -Dia
        if ($r -eq "") { $failed++ } elseif ($eh -eq "table") { $msExe = $r }
        if ($eh -ne "table") { continue }
        # the link variants, on the table model's object
        $lo = & $libExe /nologo "/OUT:$vdir\vars.lib" "$withG\vars6.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL lib of vars6.obj:"; Write-Host $lo; $failed++ }
        foreach ($v in @(@("/DEBUG", "incremental"), @("/DEBUG /OPT:REF,ICF", "optref"), @("/DEBUG /INCREMENTAL:NO", "lib"))) {
            $vin = if ($v[1] -eq "lib") { @("$vdir\vars.lib", "$vdir\varsck.obj") } else { $inputs }
            $what = if ($v[1] -eq "lib") { "the object from a static library" } else { $v[0] }
            if ((Test-VarsBuild "--eh=table, Microsoft's link, $what" $linkExe $vin @($v[0] -split ' ') "vars-$eh-ms-$($v[1])") -eq "") { $failed++ }
        }
        if ($grideLink -ne "") {
            $grideExe = Test-VarsBuild "--eh=table, GRIDE's link" $grideLink $inputs @("/DEBUG", "/INCREMENTAL:NO") "vars-$eh-gr" -Dia
            if ($grideExe -eq "") { $failed++ }
            foreach ($v in @(@("/DEBUG /OPT:REF,ICF", "optref"), @("/DEBUG", "lib"))) {
                $vin = if ($v[1] -eq "lib") { @("$vdir\vars.lib", "$vdir\varsck.obj") } else { $inputs }
                $what = if ($v[1] -eq "lib") { "the object from a static library" } else { $v[0] }
                if ((Test-VarsBuild "--eh=table, GRIDE's link, $what" $grideLink $vin @($v[0] -split ' ') "vars-$eh-gr-$($v[1])") -eq "") { $failed++ }
            }
        } else { Write-Host "  note GRIDE not found, its link is not tried" }
    }
    # gride-dbg (GRIDE's debugger, dbghelp's reader) on the programs of both linkers: the same answers to dv
    $grideDbg = if ($gride[0] -ne "") { "$($gride[0])\gride-dbg.exe" } else { "" }
    if ($grideDbg -ne "" -and (Test-Path $grideDbg) -and $grideExe -ne "" -and $msExe -ne "") {
        $vlines = @(Get-Content "..\examples\sample-dbg\vars.cpp")
        $bps = @(); for ($k = 0; $k -lt $vlines.Count; ++$k) { if ($vlines[$k] -match '^\s+checkpoint\("fn=(refs|params|sum|Both::get|blocks|handler|floats)') { $bps += ($k + 1) } }
        $cmds = @(); foreach ($l in $bps) { $cmds += "bp ``vars.cpp:$l``" }; foreach ($l in $bps) { $cmds += "g"; $cmds += "dv /t /V" }
        $answers = @{}
        foreach ($pe in $msExe, $grideExe) {
            $in = "$pe.dbg.in.txt"; $out = "$pe.dbg.out.txt"
            [IO.File]::WriteAllText($in, (($cmds + "q") -join "`n") + "`n", (New-Object System.Text.ASCIIEncoding))
            $p = Start-Process -FilePath $grideDbg -ArgumentList "`"$pe`"" -WorkingDirectory $vdir -RedirectStandardInput $in -RedirectStandardOutput $out -RedirectStandardError "$pe.dbg.err.txt" -NoNewWindow -PassThru
            if (-not $p.WaitForExit(120000)) { $p.Kill() }
            $answers[$pe] = @(Get-Content $out | ForEach-Object { $_ -replace '[0-9a-f]{8}`[0-9a-f]{8}', 'ADDR' -replace '\+0x[0-9a-f]+:', '+OFF:' -replace '^vars-table-(ms|gr)!', 'vars!' })
        }
        $hits = @($answers[$msExe] | Where-Object { $_ -match '^Breakpoint \d+ hit' }).Count
        $vals = @($answers[$msExe] | Where-Object { $_ -match '^ADDR @rbp' }).Count
        if ($hits -eq $bps.Count -and $vals -gt 30 -and @(Compare-Object $answers[$msExe] $answers[$grideExe]).Count -eq 0) { Write-Host "  ok   gride-dbg stops at $hits checkpoint lines and dv lists $vals variables in the frame, the same lines from both linkers' programs" }
        else { Write-Host "  FAIL gride-dbg: $hits of $($bps.Count) breakpoints hit, $vals variables in the frame; from the two linkers' programs:"; Compare-Object $answers[$msExe] $answers[$grideExe] | Select-Object -First 8 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
    } elseif ($grideLink -ne "") { Write-Host "  note gride-dbg not found, it is not tried" }
    # stage 3, the optimiser's tables (docs\design-codeview.md 5.4), read in the IR dump of vars.cpp at -O2 with -g: which
    # variables have one definition (a temporary), which lost a store. A parameter that is not assigned has its incoming
    # value (the adjusted "this" in Both::get), a local assigned once the value it was assigned; a variable assigned
    # twice, and the scalars of a function whose handler resumes, have none
    $tdir = "$vdir\tables"
    New-Item -ItemType Directory -Force $tdir | Out-Null
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O2 -g --eh=table --obj --msvc-mangling -q -I ..\include -o $tdir -n vars "..\examples\sample-dbg\vars.cpp" 2>&1 | Out-String
    $ErrorActionPreference = $eap
    if (-not (Test-Path "$tdir\vars-ir6.txt")) { Write-Host "  FAIL --compile6 -O2 -g of sample-dbg\vars.cpp:"; Write-Host $c6; $failed++ }
    else {
        $tables = @{}; $after = $false; $cur = ""
        foreach ($l in Get-Content "$tdir\vars-ir6.txt") {
            if ($l -match '^--- after') { $after = $true; continue }
            if ($l -match '^=====') { $after = $false; continue }
            if (-not $after) { continue }
            if ($l -match '^function ([^(]+)') { $cur = $Matches[1].Trim(); $tables[$cur] = @{ Names = @{}; One = @(); Stale = @() }; continue }
            if ($cur -eq "") { continue }
            if ($l -match '^  local(\d+): .*\[-g (\S+) T\d+ block ([^\]]*)\]') { $local = $Matches[1]; $shown = $Matches[2]; $marks = $Matches[3]; $tables[$cur].Names[$local] = $shown; if ($marks -like '*a store removed*') { $tables[$cur].Stale += $shown } }
            elseif ($l -match '^  -g: .*one definition: (.*)$') { foreach ($m in [regex]::Matches($Matches[1], 'local(\d+) = t\d+')) { $n = $tables[$cur].Names[$m.Groups[1].Value]; if ($n) { $tables[$cur].One += $n } } }
        }
        $wrong = @()
        foreach ($w in @(@("many", "a b c d e f g h sum", ""), @("params", "used", "unused __formal changed"), @("Both::get", "this k r", ""), @("nested", "a mid deep", "top"),
                         @("sum", "", "s i"), @("floats", "f d half twice", ""), @("resumes", "", "v observed extra"), @("leaf", "q w", ""))) {
            $t = $tables[$w[0]]
            if (-not $t) { $wrong += "$($w[0]): not in the dump"; continue }
            foreach ($n in @($w[1] -split ' ' | Where-Object { $_ })) { if ($t.One -notcontains $n) { $wrong += "$($w[0]): $n has no definition" } }
            foreach ($n in @($w[2] -split ' ' | Where-Object { $_ })) { if ($t.One -contains $n) { $wrong += "$($w[0]): $n has one, and is assigned more than once or lives in memory" } }
        }
        if ($tables["resumes"] -and $tables["resumes"].Stale -notcontains "observed") { $wrong += "resumes: observed is not marked as having lost a store" }
        if ($wrong.Count -eq 0) { Write-Host "  ok   -O2: the optimiser's tables name the one definition of the variables that have one (the IR dump of vars.cpp), and the scalar that lost a store" }
        else { Write-Host "  FAIL -O2: the optimiser's tables of vars.cpp:"; $wrong | Select-Object -First 10 | ForEach-Object { Write-Host "       $_" }; $failed++ }
    }
    # stage 3 (docs\design-codeview.md section 6, row 3): the value harness at -O1, -O2 and -O3 in each exception model.
    # With -g the code is what it is without it (cvsame); the compiler's own check of the records and of every register
    # range against the code it encoded (CvCheck (b), (c)) says nothing; dbgvars -optimized finds no wrong value in a
    # checkpoint's caller, in the frames above it and at the next line, every declared parameter listed, the variables
    # the texts mark ("+") in their places, and reads 60 values or more; DIA gives the same variables the same places;
    # the -O2 objects through the link variants and both linkers; -j 1 and -j 4 write the same object
    $optMs = ""; $optGride = ""
    foreach ($lvl in 1, 2, 3) {
        foreach ($eh in "table", "sjlj", "cl") {
            $plain = "$vdir\plain-$eh-O$lvl"; $withG = "$vdir\g-$eh-O$lvl"
            New-Item -ItemType Directory -Force $plain, $withG | Out-Null
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $null = & $exe --compile6 "-O$lvl" -g0 "--eh=$eh" --obj --msvc-mangling -q -I ..\include -o $plain -n vars "..\examples\sample-dbg\vars.cpp" 2>&1
            $c6 = & $exe --compile6 "-O$lvl" -g "--eh=$eh" --obj --msvc-mangling -q -I ..\include -o $withG -n vars "..\examples\sample-dbg\vars.cpp" 2>&1 | Out-String
            $ErrorActionPreference = $eap
            if (-not (Test-Path "$plain\vars6.obj") -or -not (Test-Path "$withG\vars6.obj") -or $c6 -match 'CodeView|internal error') { Write-Host "  FAIL --compile6 -O$lvl -g --eh=$eh of sample-dbg\vars.cpp:"; Write-Host $c6; $failed++; continue }
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $same = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -A $plain -B $withG -Listings 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -ne 0 -or $same -notmatch '1 pair\(s\): 0 the same \(listings too\), 1 differ in debug information only, 0 violate') { Write-Host "  FAIL -O$lvl --eh=${eh}: cvsame on the object without and with -g (exit $rc):"; Write-Host $same; $failed++; continue }
            $inRegister = @(Select-String -Path "$withG\vars6.asm" -Pattern '^;@loc \d+ [a-z]').Count
            if ($inRegister -lt 20) { Write-Host "  FAIL -O$lvl --eh=${eh}: $inRegister places in a register in the listing of vars.cpp (;@loc), 20 or more expected"; $failed++; continue }
            Write-Host "  ok   -O$lvl --eh=${eh}: with -g the object and listing are what they are without it but for CodeView (cvsame); $inRegister places in a register pass the compiler's own check"
            $inputs = @("$withG\vars6.obj", "$vdir\varsck.obj")
            $r = Test-VarsBuild "-O$lvl --eh=$eh, Microsoft's link" $linkExe $inputs @("/DEBUG", "/INCREMENTAL:NO") "vars-$eh-O$lvl-ms" -Dia -Optimized
            if ($r -eq "") { $failed++ } elseif ($lvl -eq 2 -and $eh -eq "table") { $optMs = $r }
            if ($lvl -ne 2 -or $eh -ne "table") { continue }
            # the link variants, on the -O2 object of the table model
            $lo = & $libExe /nologo "/OUT:$vdir\vars-O2.lib" "$withG\vars6.obj" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL lib of the -O2 vars6.obj:"; Write-Host $lo; $failed++ }
            foreach ($v in @(@("/DEBUG", "incremental"), @("/DEBUG /OPT:REF,ICF", "optref"), @("/DEBUG /INCREMENTAL:NO", "lib"))) {
                $vin = if ($v[1] -eq "lib") { @("$vdir\vars-O2.lib", "$vdir\varsck.obj") } else { $inputs }
                $what = if ($v[1] -eq "lib") { "the object from a static library" } else { $v[0] }
                if ((Test-VarsBuild "-O2 --eh=table, Microsoft's link, $what" $linkExe $vin @($v[0] -split ' ') "vars-$eh-O2-ms-$($v[1])" -Optimized) -eq "") { $failed++ }
            }
            if ($grideLink -ne "") {
                $optGride = Test-VarsBuild "-O2 --eh=table, GRIDE's link" $grideLink $inputs @("/DEBUG", "/INCREMENTAL:NO") "vars-$eh-O2-gr" -Dia -Optimized
                if ($optGride -eq "") { $failed++ }
                foreach ($v in @(@("/DEBUG /OPT:REF,ICF", "optref"), @("/DEBUG", "lib"))) {
                    $vin = if ($v[1] -eq "lib") { @("$vdir\vars-O2.lib", "$vdir\varsck.obj") } else { $inputs }
                    $what = if ($v[1] -eq "lib") { "the object from a static library" } else { $v[0] }
                    if ((Test-VarsBuild "-O2 --eh=table, GRIDE's link, $what" $grideLink $vin @($v[0] -split ' ') "vars-$eh-O2-gr-$($v[1])" -Optimized) -eq "") { $failed++ }
                }
            }
            # the same object whatever the number of threads (invariant 2)
            $one = "$vdir\g-O2-j1"; $four = "$vdir\g-O2-j4"
            New-Item -ItemType Directory -Force $one, $four | Out-Null
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $null = & $exe --compile6 -O2 -g --eh=table --obj --msvc-mangling -q -j 1 -I ..\include -o $one -n vars "..\examples\sample-dbg\vars.cpp" 2>&1
            $null = & $exe --compile6 -O2 -g --eh=table --obj --msvc-mangling -q -j 4 -I ..\include -o $four -n vars "..\examples\sample-dbg\vars.cpp" 2>&1
            $ErrorActionPreference = $eap
            $differ = @(); foreach ($n in "vars6.obj", "vars6.asm", "vars-cv6.txt") { if (-not (Test-Path "$one\$n") -or -not (Test-Path "$four\$n") -or (Get-FileHash "$one\$n").Hash -ne (Get-FileHash "$four\$n").Hash) { $differ += $n } }
            if ($differ.Count -eq 0) { Write-Host "  ok   -O2 -g: -j 1 and -j 4 write the same object, listing and description" }
            else { Write-Host "  FAIL -O2 -g: -j 1 and -j 4 differ in $($differ -join ', ')"; $failed++ }
        }
    }
    # gride-dbg on the -O2 programs of both linkers: the same answers to dv (a variable in a register shows as GRIDE's
    # debugger shows it today, docs\design-codeview.md section 10, D4 to D6; what is compared is that the two PDBs agree;
    # a reference in a register is printed as the register's value, an address of the run, which is taken out)
    if ($grideDbg -ne "" -and (Test-Path $grideDbg) -and $optGride -ne "" -and $optMs -ne "") {
        $vlines = @(Get-Content "..\examples\sample-dbg\vars.cpp")
        $bps = @(); for ($k = 0; $k -lt $vlines.Count; ++$k) { if ($vlines[$k] -match '^\s+checkpoint\("fn=(refs|params|sum|Both::get|blocks|handler|floats|regs|makeBig)') { $bps += ($k + 1) } }
        $cmds = @(); foreach ($l in $bps) { $cmds += "bp ``vars.cpp:$l``" }; foreach ($l in $bps) { $cmds += "g"; $cmds += "dv /t /V" }
        $answers = @{}
        foreach ($pe in $optMs, $optGride) {
            $in = "$pe.dbg.in.txt"; $out = "$pe.dbg.out.txt"
            [IO.File]::WriteAllText($in, (($cmds + "q") -join "`n") + "`n", (New-Object System.Text.ASCIIEncoding))
            $p = Start-Process -FilePath $grideDbg -ArgumentList "`"$pe`"" -WorkingDirectory $vdir -RedirectStandardInput $in -RedirectStandardOutput $out -RedirectStandardError "$pe.dbg.err.txt" -NoNewWindow -PassThru
            if (-not $p.WaitForExit(120000)) { $p.Kill() }
            $answers[$pe] = @(Get-Content $out | ForEach-Object { $_ -replace '[0-9a-f]{8}`[0-9a-f]{8}', 'ADDR' -replace '\+0x[0-9a-f]+:', '+OFF:' -replace '^vars-table-O2-(ms|gr)!', 'vars!' -replace ' = \d{9,}$', ' = ADDR' })
        }
        $hits = @($answers[$optMs] | Where-Object { $_ -match '^Breakpoint \d+ hit' }).Count
        if ($hits -eq $bps.Count -and @(Compare-Object $answers[$optMs] $answers[$optGride]).Count -eq 0) { Write-Host "  ok   -O2: gride-dbg stops at $hits checkpoint lines and dv prints the same $($answers[$optMs].Count) lines from both linkers' programs" }
        else { Write-Host "  FAIL -O2: gride-dbg: $hits of $($bps.Count) breakpoints hit; from the two linkers' programs:"; Compare-Object $answers[$optMs] $answers[$optGride] | Select-Object -First 8 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed CodeView variable check(s) failed" }

    Write-Host ""
    Write-Host "=== Test 22: CodeView source checksums (-g): the SHA-256 of each source in the file table, read back through dbghelp ==="
    # row M10, docs\design-codeview.md stage 5. With -g every entry of an object's file table (DEBUG_S_FILECHKSMS) carries
    # the SHA-256 of its source file, of the bytes the preprocessor read, as cl 19.52's /Z7 objects do; a debugger holds
    # a source against it.
    #   - the hash (cv\CvSha256): cppparse --sha256 on the messages of the standard's examples (FIPS 180-4) against their
    #     known digests, on files of the lengths around the padding's limits (0, 55, 56, 63, 64, 65 bytes and more) against
    #     Get-FileHash, and on one file of 2^29 + 1 bytes, whose length in bits does not fit 32 bits
    #   - sample-dbg\sums.cpp and sums.h, copied to out\dbgsums\src (the test edits them), built with -g and -g2 at -O0
    #     and -O2: cvsame against the object without (debug information changes no code); every entry of the object's file
    #     table has kind 3 and 32 bytes, the SHA-256 of the file it names (the table read from the object's bytes here and,
    #     where Visual Studio has it, by llvm-readobj); -g2's table is -g's; without the option the table has the files
    #     with code alone and no entry has a checksum
    #   - the same unit by cl /Z7: the two tables name the same files, every one the unit includes, and each entry is
    #     cl's (kind, checksum, 40 bytes)
    #   - linked with /DEBUG by Microsoft's link and, where GRIDE is found, by GRIDE's, neither saying anything:
    #     sample-dbg-driver\dbgsum.cpp (cl, dbghelp: SymEnumSourceFiles, SymGetSourceFileChecksum) gives the file's own
    #     SHA-256 for the unit, for its header and for every other file of the table; without -g it gives none
    #   - a unit whose #line directives name a file that does not exist and one it does not read: those entries have no
    #     checksum, in cl's object as in cppparse's; it includes a header twice, in two spellings: one entry in both
    #   - the unit and its header edited (a line added to each): the checksums the PDBs hold are no longer the files';
    #     built again, they are
    #   - the compiler's own sources with -g (test 20's objects): every entry of every object's table is its file's SHA-256
    $failed = 0
    $sdir = "$Sandbox\out\dbgsums"
    New-Item -ItemType Directory -Force $sdir, "$sdir\src", "$sdir\len" | Out-Null
    Get-ChildItem $sdir -File -Recurse | Remove-Item
    $linkExe = "$Msvc\bin\Hostx64\x64\link.exe"
    $gride = & { . "$Sandbox\scripts\toolset.ps1"; ,@($GrideRoot, $GrideLink) }
    $grideLink = $gride[1]
    $readobj = "$VsRoot\VC\Tools\Llvm\x64\bin\llvm-readobj.exe"
    $libs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
    $shaOf = @{}
    function Get-Sha([string]$path) {
        if (-not $shaOf.ContainsKey($path)) { $shaOf[$path] = if (Test-Path -LiteralPath $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower() } else { "no such file" } }
        return $shaOf[$path]
    }
    # what cppparse --sha256 prints for the files of a folder, by file name
    function Get-CppparseSha([string]$pattern) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $lines = @(& $exe --sha256 $pattern 2>&1 | ForEach-Object { "$_" })
        $ErrorActionPreference = $eap
        $map = @{}
        foreach ($l in $lines) { if ($l -match '^([0-9a-f]{64})  (.+)$') { $map[(Split-Path $Matches[2] -Leaf)] = $Matches[1] } }
        return $map
    }
    # the hash: the standard's examples (FIPS 180-4 and its example file: "abc", the empty message, the messages of 448
    # and 896 bits, a million "a") against their digests
    $ascii = New-Object System.Text.ASCIIEncoding
    $known = @(
        @("empty.bin", "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
        @("abc.bin", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        @("bits448.bin", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
        @("bits896.bin", "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"),
        @("million-a.bin", ("a" * 1000000), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")
    )
    foreach ($k in $known) { [IO.File]::WriteAllBytes("$sdir\len\$($k[0])", $ascii.GetBytes($k[1])) }
    # ... and files of every length around a block's end and the padding's limit (a message of 55 bytes is the longest
    # that fits one block with its padding) and around the 65,521 bytes --sha256 reads at a time (no multiple of a
    # block, so the larger files go through a block that is begun), with bytes of every value, against Get-FileHash
    $lengths = @(1, 2, 54, 55, 56, 57, 62, 63, 64, 65, 118, 119, 120, 121, 127, 128, 129, 4095, 4096, 4097, 65520, 65521, 65522, 65535, 65536, 65537, 131042, 131079, 1048576)
    foreach ($n in $lengths) { $b = New-Object byte[] $n; (New-Object System.Random $n).NextBytes($b); [IO.File]::WriteAllBytes("$sdir\len\len$n.bin", $b) }
    $got = Get-CppparseSha "$sdir\len\*.bin"
    $wrong = @()
    foreach ($k in $known) { if ($got[$k[0]] -ne $k[2]) { $wrong += "$($k[0]): $($got[$k[0]]), the standard has $($k[2])" } }
    foreach ($f in (Get-ChildItem "$sdir\len\*.bin")) { if ($got[$f.Name] -ne (Get-Sha $f.FullName)) { $wrong += "$($f.Name): $($got[$f.Name]), Get-FileHash has $(Get-Sha $f.FullName)" } }
    if ($wrong.Count -eq 0 -and $got.Count -eq $known.Count + $lengths.Count) { Write-Host "  ok   cppparse --sha256: the $($known.Count) messages of the standard's examples have their digests, and $($lengths.Count) files of 1 to 1,048,576 bytes (55, 56, 63, 64, 65 among them) the ones of Get-FileHash" }
    else { Write-Host "  FAIL cppparse --sha256 ($($got.Count) of $($known.Count + $lengths.Count) files answered):"; $wrong | Select-Object -First 6 | ForEach-Object { Write-Host "         $_" }; $failed++ }
    # 2^29 + 1 bytes: 2^32 + 8 bits, a length that needs the upper half of the 64-bit field the padding ends with
    $big = "$sdir\len\big.raw"
    $fs = [IO.File]::Create($big); $fs.SetLength(536870913); $fs.Position = 536870912; $fs.WriteByte(0x61); $fs.Close()
    $got = Get-CppparseSha $big
    if ($got["big.raw"] -and $got["big.raw"] -eq (Get-Sha $big)) { Write-Host "  ok   cppparse --sha256: a file of 536,870,913 bytes (its length in bits is beyond 32 bits) has the digest of Get-FileHash" }
    else { Write-Host "  FAIL cppparse --sha256 on a file of 536,870,913 bytes: $($got['big.raw']), Get-FileHash has $(Get-Sha $big)"; $failed++ }
    Remove-Item $big
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $none = & $exe --sha256 "$sdir\len\no-such-file.bin" 2>&1 | Out-String; $rc = $LASTEXITCODE
    $ErrorActionPreference = $eap
    if ($rc -eq 0 -or $none -match '[0-9a-f]{64}') { Write-Host "  FAIL cppparse --sha256 of a file that does not exist: exit $rc, $none"; $failed++ }

    # the file table of an object, read from its bytes: the first .debug$S that has a DEBUG_S_FILECHKSMS (F4h) and a
    # DEBUG_S_STRINGTABLE (F3h) subsection; an entry is {the name's offset in the string table (4 bytes), the checksum's
    # length (1), its kind (1), the checksum, zeros to four bytes}. Returns the entries in order: Name, Kind, Sum, Size
    function Read-FileTable([string]$objPath) {
        $b = [IO.File]::ReadAllBytes($objPath)
        $count = [BitConverter]::ToUInt16($b, 2)
        $first = 20 + [BitConverter]::ToUInt16($b, 16)
        $latin = [Text.Encoding]::GetEncoding(28591)
        for ($s = 0; $s -lt $count; $s++) {
            $h = $first + 40 * $s
            if ($b[$h] -ne 0x2E -or $latin.GetString($b, $h, 8) -ne '.debug$S') { continue }
            $size = [BitConverter]::ToUInt32($b, $h + 16); $at = [BitConverter]::ToUInt32($b, $h + 20)
            $sums = $null; $names = $null
            $p = $at + 4
            while ($p + 8 -le $at + $size) {
                $kind = [BitConverter]::ToUInt32($b, $p); $len = [BitConverter]::ToUInt32($b, $p + 4)
                if ($kind -eq 0xF4) { $sums = @(($p + 8), $len) } elseif ($kind -eq 0xF3) { $names = @(($p + 8), $len) }
                $p += 8 + $len; $p += (4 - ($p - $at) % 4) % 4
            }
            if ($null -eq $sums -or $null -eq $names) { continue }
            $out = New-Object System.Collections.ArrayList
            $q = $sums[0]; $end = $sums[0] + $sums[1]
            while ($q + 6 -le $end) {
                $nameAt = $names[0] + [BitConverter]::ToUInt32($b, $q); $n = $b[$q + 4]; $kind = $b[$q + 5]
                $z = $nameAt; while ($z -lt $names[0] + $names[1] -and $b[$z] -ne 0) { $z++ }
                $sum = ""; for ($i = 0; $i -lt $n; $i++) { $sum += $b[$q + 6 + $i].ToString("x2") }
                $entry = 6 + $n; $entry += (4 - $entry % 4) % 4
                $pad = 0; for ($i = 6 + $n; $i -lt $entry; $i++) { $pad += $b[$q + $i] }
                $null = $out.Add([pscustomobject]@{ Name = $latin.GetString($b, $nameAt, $z - $nameAt); Kind = [int]$kind; Sum = $sum; Size = $entry; Padding = $pad })
                $q += $entry
            }
            return ,@($out)
        }
        return ,@()
    }
    # what is wrong with a table whose every entry should carry its file's SHA-256, in cl's form (40 bytes an entry)
    function Test-Sums($table) {
        $bad = @()
        foreach ($e in $table) {
            if ($e.Kind -ne 3 -or $e.Sum.Length -ne 64 -or $e.Size -ne 40 -or $e.Padding -ne 0) { $bad += "$($e.Name): kind $($e.Kind), $($e.Sum.Length / 2) bytes, an entry of $($e.Size)" }
            elseif ($e.Sum -ne (Get-Sha $e.Name)) { $bad += "$($e.Name): $($e.Sum), the file has $(Get-Sha $e.Name)" }
        }
        return $bad
    }
    function Build-Sums([string]$opt, [int]$lvl, [string]$dir) {
        New-Item -ItemType Directory -Force $dir | Out-Null
        if (Test-Path "$dir\sums6.obj") { Remove-Item "$dir\sums6.obj" }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $c6 = & $exe --compile6 "-O$lvl" $opt --obj --msvc-mangling -q -I ..\include -o $dir -n sums "$sdir\src\sums.cpp" 2>&1 | Out-String
        $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
        if ($rc -ne 0 -or -not (Test-Path "$dir\sums6.obj")) { Write-Host "  FAIL --compile6 -O$lvl $opt of sample-dbg\sums.cpp:"; Write-Host $c6; return $false }
        return $true
    }
    # links the object with /DEBUG and runs the program; the linker has to say nothing
    function Link-Sums([string]$what, [string]$linker, [string]$obj, [string]$out) {
        if (Test-Path $out) { Remove-Item $out }
        $lo = & $linker /nologo /DEBUG /INCREMENTAL:NO /SUBSYSTEM:CONSOLE "/OUT:$out" $obj "out\x64\cppparse_rt.obj" @libs 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out) -or $lo.Trim() -ne "") { Write-Host "  FAIL $what, link /DEBUG (it has to say nothing):"; Write-Host $lo; return $false }
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $run = & $out 2>&1 | Out-String; $rc = $LASTEXITCODE
        $ErrorActionPreference = $eap
        if ($rc -ne 0 -or $run -notmatch 'sum 400') { Write-Host "  FAIL ${what}: the program exits with $rc and prints $run"; return $false }
        return $true
    }
    # the checksums a program's PDB holds, as dbghelp gives them: the file's path in lower case -> "<kind> <digits>"
    function Get-PdbSums([string]$program) {
        $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
        $lines = @(& "$sdir\dbgsum.exe" $program 2>&1 | ForEach-Object { "$_".TrimEnd() }); $rc = $LASTEXITCODE
        $ErrorActionPreference = $eap
        $map = @{}
        if ($rc -ne 0) { return $map }
        foreach ($l in $lines) { if ($l -match '^(\S+) (\S+) (.+)$') { $map[$Matches[3].ToLower()] = "$($Matches[1]) $($Matches[2])" } }
        return $map
    }
    # the files of a table whose checksum in the PDB is not the given one ("sha256 <the file's own>" where none is given)
    function Compare-PdbSums($table, $pdb, [string]$want = "") {
        $bad = @()
        foreach ($e in $table) {
            $w = if ($want -ne "") { $want } else { "sha256 " + (Get-Sha $e.Name) }
            if ($pdb[$e.Name.ToLower()] -ne $w) { $bad += "$($e.Name): the PDB has '$($pdb[$e.Name.ToLower()])', expected '$w'" }
        }
        return $bad
    }
    Copy-Item "..\examples\sample-dbg\sums.cpp", "..\examples\sample-dbg\sums.h" "$sdir\src"
    $unit = "$sdir\src\sums.cpp"; $header = "$sdir\src\sums.h"
    $clOut = Build-Program @("/EHsc", "/W3", "/Fo$sdir\dbgsum.obj", "..\examples\sample-dbg-driver\dbgsum.cpp") @("$sdir\dbgsum.obj") "$sdir\dbgsum.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg-driver\dbgsum.cpp" }
    $clOut = & $cl /nologo /c /EHsc /W3 /Od /Z7 /X /I..\include "/Fo$sdir\sums-cl.obj" $unit 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $clOut; throw "cl could not compile sample-dbg\sums.cpp" }
    $clTable = Read-FileTable "$sdir\sums-cl.obj"
    $programs = @()          # the -O0 -g programs, for the edit below
    foreach ($lvl in @(0, 2)) {
        $at = "-O$lvl -g"
        $plain = "$sdir\plain$lvl"; $withG = "$sdir\g$lvl"; $withG2 = "$sdir\g2-$lvl"
        if (-not ((Build-Sums "-g0" $lvl $plain) -and (Build-Sums "-g" $lvl $withG) -and (Build-Sums "-g2" $lvl $withG2))) { $failed++; continue }
        $violations = @()
        foreach ($d in $withG, $withG2) {
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $same = & powershell -NoProfile -ExecutionPolicy Bypass -File "$Sandbox\scripts\cvsame.ps1" -A $plain -B $d -Listings 2>&1 | Out-String
            $rc = $LASTEXITCODE; $ErrorActionPreference = $eap
            if ($rc -ne 0 -or $same -notmatch '1 pair\(s\): 0 the same \(listings too\), 1 differ in debug information only, 0 violate') { $violations += $same }
        }
        if ($violations.Count -eq 0) { Write-Host "  ok   ${at}: the object and listing are what they are without it but for CodeView, with -g2 too (cvsame)" }
        else { Write-Host "  FAIL ${at}: cvsame on the objects without and with -g, -g2:"; Write-Host ($violations -join "`n"); $failed++ }
        $tPlain = Read-FileTable "$plain\sums6.obj"; $tG = Read-FileTable "$withG\sums6.obj"; $tG2 = Read-FileTable "$withG2\sums6.obj"
        $names = @($tG | ForEach-Object { $_.Name })
        $bad = @(Test-Sums $tG)
        if ($names -notcontains $unit) { $bad += "the table does not name $unit" }
        if ($names -notcontains $header) { $bad += "the table does not name $header" }
        if ($tG.Count -lt 3) { $bad += "the table names $($tG.Count) file(s): the unit, its header and a header of the library are expected" }
        if ((($tG2 | ForEach-Object { "$($_.Name) $($_.Kind) $($_.Sum)" }) -join "|") -ne (($tG | ForEach-Object { "$($_.Name) $($_.Kind) $($_.Sum)" }) -join "|")) { $bad += "-g2's table is not -g's" }
        # without the option: the files a line table names (the unit, its header, one of the library at least), each of
        # them in -g's table too, no checksum, 8 bytes an entry (what every object had before stage 5)
        $plainNames = @($tPlain | ForEach-Object { $_.Name })
        if ($plainNames -notcontains $unit -or $plainNames -notcontains $header -or $tPlain.Count -lt 3) { $bad += "the object without -g names $($plainNames -join ', ')" }
        if (@($plainNames | Where-Object { $names -notcontains $_ }).Count -ne 0) { $bad += "the object without -g names a file -g's table does not" }
        if ($tPlain.Count -ge $tG.Count) { $bad += "-g's table ($($tG.Count) files) should name more than the $($tPlain.Count) files that have code" }
        $withSum = @($tPlain | Where-Object { $_.Kind -ne 0 -or $_.Sum -ne "" -or $_.Size -ne 8 })
        if ($withSum.Count -ne 0) { $bad += "the object without -g has $($withSum.Count) entries that are not {kind 0, no checksum, 8 bytes}" }
        if ($bad.Count -eq 0) { Write-Host "  ok   ${at}: the file table's $($tG.Count) entries (the unit, sums.h and $($tG.Count - 2) headers of the library: every file the unit reads) each carry kind 3 and the 32 bytes of the file's SHA-256, 40 bytes an entry; -g2's table is the same; without the option the table has the $($tPlain.Count) files with code and no checksum" }
        else { Write-Host "  FAIL ${at}: the object's file table:"; $bad | Select-Object -First 8 | ForEach-Object { Write-Host "         $_" }; $failed++ }
        if (Test-Path $readobj) {
            # a second reader of the same bytes: llvm-readobj's FileChecksum blocks, in order
            $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
            $dump = @(& $readobj --codeview "$withG\sums6.obj" 2>&1 | ForEach-Object { "$_" }); $rrc = $LASTEXITCODE
            $ErrorActionPreference = $eap
            $theirs = @(); $cur = $null; $inSum = $false
            foreach ($l in $dump) {
                if ($l -match '^\s*FileChecksum \{') { $cur = @{ Name = ""; Kind = ""; Sum = "" }; $inSum = $false }
                elseif ($null -eq $cur) { continue }
                elseif ($l -match '^\s*Filename: (.+) \(0x[0-9A-Fa-f]+\)\s*$') { $cur.Name = $Matches[1] }
                elseif ($l -match '^\s*ChecksumKind: \S+ \(0x([0-9A-Fa-f]+)\)') { $cur.Kind = [Convert]::ToInt32($Matches[1], 16) }
                elseif ($l -match '^\s*ChecksumBytes \(') { $inSum = $true }
                elseif ($inSum -and $l -match '^\s*[0-9A-Fa-f]{4}: ((?:[0-9A-Fa-f]{2,8} ?)+)') { $cur.Sum += ($Matches[1] -replace ' ', '').ToLower() }
                elseif ($l -match '^\s*\}') { $theirs += "$($cur.Name) $($cur.Kind) $($cur.Sum)"; $cur = $null }
            }
            $ours = @($tG | ForEach-Object { "$($_.Name) $($_.Kind) $($_.Sum)" })
            $notSha = @($theirs | Where-Object { $_ -notmatch ' 3 [0-9a-f]{64}$' })
            if ($rrc -eq 0 -and $theirs.Count -ne 0 -and $theirs.Count -eq $ours.Count -and $notSha.Count -eq 0 -and @(Compare-Object $ours $theirs).Count -eq 0) { Write-Host "  ok   ${at}: llvm-readobj reads the same $($theirs.Count) entries from the object, each with its name, kind 3 (SHA256) and those 32 bytes" }
            else { Write-Host "  FAIL ${at}: llvm-readobj (exit $rrc) reads $($theirs.Count) entries, $($notSha.Count) of them without a SHA-256; this test reads $($ours.Count):"; Compare-Object $ours $theirs | Select-Object -First 6 | ForEach-Object { Write-Host "         $($_.SideIndicator) $($_.InputObject)" }; $failed++ }
        } else { Write-Host "  note ${at}: llvm-readobj is not found, the table is read by this test alone" }
        # cl's table of the same unit: the same files, every one the unit includes (cl in its own order), with the same bytes
        $clBy = @{}; foreach ($e in $clTable) { $clBy[$e.Name.ToLower()] = $e }
        $bad = @()
        $lower = @($names | ForEach-Object { $_.ToLower() })
        foreach ($e in $clTable) { if ($lower -notcontains $e.Name.ToLower()) { $bad += "$($e.Name) is in cl's table alone" } }
        foreach ($e in $tG) {
            $c = $clBy[$e.Name.ToLower()]
            if ($null -eq $c) { $bad += "$($e.Name) is not in cl's table" }
            elseif ($c.Kind -ne $e.Kind -or $c.Sum -ne $e.Sum -or $c.Size -ne $e.Size) { $bad += "$($e.Name): cl has kind $($c.Kind), $($c.Sum), an entry of $($c.Size)" }
        }
        $clBad = @(Test-Sums $clTable)
        if ($bad.Count -eq 0 -and $clBad.Count -eq 0 -and $clTable.Count -eq $tG.Count) { Write-Host "  ok   ${at}: the table names the $($clTable.Count) files cl /Z7's object names, and each entry is cl's for that file (kind, checksum, 40 bytes)" }
        else { Write-Host "  FAIL ${at}: against the file table of cl /Z7's object ($($clTable.Count) entries, $($clBad.Count) of them not their file's SHA-256):"; ($bad + $clBad) | Select-Object -First 8 | ForEach-Object { Write-Host "         $_" }; $failed++ }
        # through the linkers: the PDB's checksum of every file of the table is the file's own
        $linkers = @(,@("Microsoft's link", $linkExe, "ms"))
        if ($grideLink -ne "") { $linkers += ,@("GRIDE's link", $grideLink, "gr") } else { Write-Host "  note ${at}: GRIDE not found, its link is not tried" }
        foreach ($lk in $linkers) {
            $program = "$sdir\sums$lvl-$($lk[2]).exe"
            if (-not (Link-Sums "$at, $($lk[0])" $lk[1] "$withG\sums6.obj" $program)) { $failed++; continue }
            $pdb = Get-PdbSums $program
            $bad = @(Compare-PdbSums $tG $pdb)
            if ($pdb.Count -ne 0 -and $bad.Count -eq 0) { Write-Host "  ok   ${at}, $($lk[0]): it says nothing; dbghelp returns for sums.cpp, sums.h and the other $($tG.Count - 2) files of the table the SHA-256 of the file (SymGetSourceFileChecksum)" }
            else { Write-Host "  FAIL ${at}, $($lk[0]): the PDB's checksums ($($pdb.Count) source files) are not the files':"; $bad | Select-Object -First 6 | ForEach-Object { Write-Host "         $_" }; $failed++; continue }
            if ($lvl -eq 0) { $programs += ,@($lk[0], $lk[1], $program, $lk[2]) }
            if ($lvl -eq 0) {
                # without -g the PDB has the files and no checksum for them
                $noSum = "$sdir\sums$lvl-plain-$($lk[2]).exe"
                if (-not (Link-Sums "-O$lvl without -g, $($lk[0])" $lk[1] "$plain\sums6.obj" $noSum)) { $failed++; continue }
                $bad = @(Compare-PdbSums $tPlain (Get-PdbSums $noSum) "none -")
                if ($bad.Count -eq 0) { Write-Host "  ok   -O$lvl without -g, $($lk[0]): dbghelp finds the same $($tPlain.Count) files and no checksum for them" }
                else { Write-Host "  FAIL -O$lvl without -g, $($lk[0]):"; $bad | Select-Object -First 6 | ForEach-Object { Write-Host "         $_" }; $failed++ }
            }
        }
    }
    # a name the unit read no file by: #line gives a function the file "sums-gone.y", which does not exist, and another
    # one sums.h, which exists and which this unit does not include. Neither has a checksum, in cl's table or in
    # cppparse's: a checksum is of bytes that were compiled, never of a file that merely has the name. And a header
    # the unit includes twice, the second time with its name in capitals, each time for a function: one entry
    $lineUnit = "$sdir\src\sumsline.cpp"; $twice = "$sdir\src\sumstwice.h"
    [IO.File]::WriteAllText($twice, "inline int SUMS_FN(int x) { return x + 1; }`r`n")
    [IO.File]::WriteAllText($lineUnit, "#define SUMS_FN first`r`n#include `"sumstwice.h`"`r`n#undef SUMS_FN`r`n#define SUMS_FN second`r`n#include `"SUMSTWICE.H`"`r`nint plain(int x) { return first(x) + second(x); }`r`n#line 100 `"sums-gone.y`"`r`nint generated(int x) { return x * 2; }`r`n#line 200 `"$($header -replace '\\', '/')`"`r`nint named(int x) { return x * 3; }`r`n")
    New-Item -ItemType Directory -Force "$sdir\line" | Out-Null
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $c6 = & $exe --compile6 -O0 -g --obj --msvc-mangling -q -I ..\include -o "$sdir\line" -n sumsline $lineUnit 2>&1 | Out-String
    $clOut = & $cl /nologo /c /W3 /Od /Z7 "/Fo$sdir\line\sumsline-cl.obj" $lineUnit 2>&1 | Out-String
    $ErrorActionPreference = $eap
    if (-not (Test-Path "$sdir\line\sumsline6.obj") -or -not (Test-Path "$sdir\line\sumsline-cl.obj")) { Write-Host "  FAIL the unit with #line is not compiled:"; Write-Host $c6; Write-Host $clOut; $failed++ }
    else {
        $bad = @()
        foreach ($who in @(,@("cppparse", "$sdir\line\sumsline6.obj")) + @(,@("cl", "$sdir\line\sumsline-cl.obj"))) {
            $t = Read-FileTable $who[1]
            $own = @($t | Where-Object { $_.Name -eq $lineUnit }); $gone = @($t | Where-Object { $_.Name -like "*\sums-gone.y" }); $other = @($t | Where-Object { $_.Name -eq $header })
            $both = @($t | Where-Object { $_.Name -eq $twice })       # (-eq compares without regard to case)
            if ($t.Count -ne 4 -or $own.Count -ne 1 -or $gone.Count -ne 1 -or $other.Count -ne 1 -or $both.Count -ne 1) { $bad += "$($who[0])'s table names $(@($t | ForEach-Object { $_.Name }) -join ', ')"; continue }
            if ($own[0].Kind -ne 3 -or $own[0].Sum -ne (Get-Sha $lineUnit)) { $bad += "$($who[0]): the unit's entry is not its SHA-256" }
            if ($both[0].Kind -ne 3 -or $both[0].Sum -ne (Get-Sha $twice)) { $bad += "$($who[0]): the entry of the header included twice is not its SHA-256" }
            foreach ($e in $gone[0], $other[0]) { if ($e.Kind -ne 0 -or $e.Sum -ne "" -or $e.Size -ne 8) { $bad += "$($who[0]): $($e.Name) has kind $($e.Kind), $($e.Sum)" } }
        }
        if ($bad.Count -eq 0) { Write-Host "  ok   a file that only #line names (one that does not exist, and sums.h, which the unit does not read) has an entry without a checksum beside the unit's SHA-256, and a header included under two spellings of its name one entry, in cl's table as in cppparse's" }
        else { Write-Host "  FAIL the file table of a unit with #line:"; $bad | ForEach-Object { Write-Host "         $_" }; $failed++ }
    }
    # the sources edited: the checksums the PDBs hold are those of the files as they were, no longer theirs
    if ($programs.Count -ne 0) {
        $old = @{ $unit = (Get-Sha $unit); $header = (Get-Sha $header) }
        foreach ($f in $unit, $header) { [IO.File]::AppendAllText($f, "// a line test 22 adds: the file is no longer the one the program was built from`r`n"); $shaOf.Remove($f) }
        if ((Get-Sha $unit) -eq $old[$unit] -or (Get-Sha $header) -eq $old[$header]) { Write-Host "  FAIL the edited sources have the SHA-256 they had"; $failed++ }
        foreach ($pr in $programs) {
            $pdb = Get-PdbSums $pr[2]
            $bad = @()
            foreach ($f in $unit, $header) {
                $has = $pdb[$f.ToLower()]
                if ($has -ne "sha256 $($old[$f])") { $bad += "$(Split-Path $f -Leaf): the PDB has '$has', not the checksum of the file as it was built" }
                if ($has -eq "sha256 $(Get-Sha $f)") { $bad += "$(Split-Path $f -Leaf): the PDB's checksum is the edited file's" }
            }
            if ($bad.Count -eq 0) { Write-Host "  ok   sums.cpp and sums.h edited, $($pr[0]): the checksums its PDB holds are no longer the files' SHA-256 (they are those of the files as they were built)" }
            else { Write-Host "  FAIL sums.cpp and sums.h edited, $($pr[0]):"; $bad | ForEach-Object { Write-Host "         $_" }; $failed++ }
        }
        # ... and built again they are the files' once more
        if (Build-Sums "-g" 0 "$sdir\g0-edited") {
            $tE = Read-FileTable "$sdir\g0-edited\sums6.obj"
            $bad = @(Test-Sums $tE)
            foreach ($pr in $programs) {
                $program = "$sdir\sums0-edited-$($pr[3]).exe"
                if (Link-Sums "the edited sources, $($pr[0])" $pr[1] "$sdir\g0-edited\sums6.obj" $program) { $bad += @(Compare-PdbSums $tE (Get-PdbSums $program)) } else { $bad += "$($pr[0]) did not link" }
            }
            if ($bad.Count -eq 0 -and $tE.Count -ge 3) { Write-Host "  ok   built again from the edited sources: the object's table and the PDB(s) of $($programs.Count) linker(s) hold the new SHA-256 of sums.cpp and sums.h" }
            else { Write-Host "  FAIL built again from the edited sources:"; $bad | Select-Object -First 6 | ForEach-Object { Write-Host "         $_" }; $failed++ }
        } else { $failed++ }
    }
    # the compiler's own sources: test 20's -g objects (test 19's own under CPPPARSE_G), every entry of every table
    $ownG = if ($env:CPPPARSE_G -eq "1" -or $env:CPPPARSE_G -eq "2") { $selfDir } else { "$Sandbox\out\self6g" }
    if (Test-Path "$ownG\main6.obj") {
        $entries = 0; $bad = @(); $files = @{}; $objects = @(Get-ChildItem "$ownG\*.obj")
        foreach ($o in $objects) {
            $t = Read-FileTable $o.FullName
            if ($t.Count -eq 0) { $bad += "$($o.Name) has no file table" }
            $entries += $t.Count
            foreach ($e in $t) { $files[$e.Name.ToLower()] = $true }
            $bad += @(Test-Sums $t | ForEach-Object { "$($o.Name): $_" })
        }
        if ($bad.Count -eq 0 -and $entries -gt $objects.Count) { Write-Host ("  ok   the own sources and libcpp with -g: all {0:N0} entries of the {1} objects' file tables carry the SHA-256 of their file ({2} files)" -f $entries, $objects.Count, $files.Count) }
        else { Write-Host "  FAIL the own sources with -g: $($bad.Count) of $entries file table entries are not their file's SHA-256:"; $bad | Select-Object -First 8 | ForEach-Object { Write-Host "         $_" }; $failed++ }
        if ($ownG -ne $selfDir) {
            # ... and test 19's objects, without the option, have none
            $withSum = 0; $entries = 0
            foreach ($o in (Get-ChildItem "$selfDir\*.obj")) { $t = Read-FileTable $o.FullName; $entries += $t.Count; $withSum += @($t | Where-Object { $_.Kind -ne 0 -or $_.Sum -ne "" -or $_.Size -ne 8 }).Count }
            if ($withSum -eq 0 -and $entries -ne 0) { Write-Host ("  ok   the own sources without -g: none of the {0:N0} entries has a checksum" -f $entries) }
            else { Write-Host "  FAIL the own sources without -g: $withSum of $entries file table entries have a checksum"; $failed++ }
        }
    }
    if ($failed -ne 0) { throw "$failed CodeView checksum check(s) failed" }
    Write-Host ""
    if ($KnownDifferences -ne 0) { Write-Host "=== $KnownDifferences known difference(s) under GRIDE's link, each printed above (docs\design-selfhost.md 4.6) ===" }
    Write-Host "=== cppparse: build and all tests succeeded ==="
    Write-Host "Reports: $Sandbox\out\sample\Sample-report.{txt,html,pdf}, Sample-diagram.{svg,dot}, Sample-health.txt"
    $global:LASTEXITCODE = 0
}
finally {
    Pop-Location
}
exit 0
