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
#   1. sample\Sample.sln  (Visual Studio 2010 solution)  -> sandbox\out\sample\
#      the text report is checked for expected findings
#   2. the cpp folder itself (folder input, includes cppparse's own sources)
#   3. Vector.hpp + VectorTest.cpp given as plain files

$ErrorActionPreference = "Stop"

$Sandbox = $PSScriptRoot
$Src     = Split-Path $Sandbox -Parent

$VsRoot  = "D:\Microsoft Visual Studio 2026"
$MsvcVer = (Get-ChildItem "$VsRoot\VC\Tools\MSVC" | Sort-Object Name | Select-Object -Last 1).Name
$Msvc    = "$VsRoot\VC\Tools\MSVC\$MsvcVer"
$SdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$SdkVer  = (Get-ChildItem "$SdkRoot\Include" | Sort-Object Name | Select-Object -Last 1).Name

$env:PATH    = "$Msvc\bin\Hostx64\x64;$env:PATH"
$env:INCLUDE = "$Msvc\include;$SdkRoot\Include\$SdkVer\ucrt;$SdkRoot\Include\$SdkVer\um;$SdkRoot\Include\$SdkVer\shared"
$env:LIB     = "$Msvc\lib\x64;$SdkRoot\Lib\$SdkVer\ucrt\x64;$SdkRoot\Lib\$SdkVer\um\x64"

$cl = "$Msvc\bin\Hostx64\x64\cl.exe"

Write-Host "=== Toolset: MSVC $MsvcVer, Windows SDK $SdkVer ==="

Push-Location $Sandbox
try {
    New-Item -ItemType Directory -Force "obj\cppparse" | Out-Null
    New-Item -ItemType Directory -Force "out\sample"   | Out-Null
    New-Item -ItemType Directory -Force "out\folder"   | Out-Null
    New-Item -ItemType Directory -Force "out\files"    | Out-Null

    Write-Host "=== Building cppparse.exe ==="
    $sources = Get-ChildItem "..\cppparse\*.cpp" | ForEach-Object { "..\cppparse\" + $_.Name }
    $output = & $cl /nologo /EHsc /W4 /permissive- /O2 /I..\cppparse "/Foobj\cppparse\" "/Fecppparse.exe" @sources 2>&1 | Out-String
    Write-Host $output
    if ($LASTEXITCODE -ne 0) { throw "cppparse failed to compile (exit code $LASTEXITCODE)" }
    if ($output -match "warning C") { throw "cppparse compiled with warnings (W4 must be clean)" }

    # ---- test 1: Visual Studio 2010 solution --------------------------------
    Write-Host "=== Test 1: sample\Sample.sln ==="
    & ".\cppparse.exe" -o "out\sample" "..\cppparse\sample\Sample.sln"
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
        '[E100] src/main.cpp:9',
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
    if ($failed -ne 0) { throw "$failed check(s) failed on Sample.sln" }

    # ---- test 2: folder input (the cpp folder itself) ------------------------
    Write-Host "=== Test 2: folder input (cpp folder, includes cppparse's own sources) ==="
    & ".\cppparse.exe" -o "out\folder" -n "cpp-folder" ".."
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on the folder" }
    $report2 = Get-Content "out\folder\cpp-folder-report.txt" -Raw
    $failed = 0
    foreach ($e in @('class cppparse::Parser', 'class cppparse::CodeBase', 'class Vector', 'class Matrix', 'struct cppparse::ClassInfo', 'folder scan (recursive)')) {
        if ($report2.Contains($e)) { Write-Host "  ok   $e" } else { Write-Host "  FAIL missing: $e"; $failed++ }
    }
    if ($failed -ne 0) { throw "$failed check(s) failed on the folder input" }

    # ---- test 3: plain files -------------------------------------------------
    Write-Host "=== Test 3: plain files (Vector.hpp, VectorTest.cpp) ==="
    & ".\cppparse.exe" -o "out\files" -n "vector" "..\Vector.hpp" "..\VectorTest.cpp"
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on plain files" }
    $report3 = Get-Content "out\files\vector-report.txt" -Raw
    if (-not $report3.Contains('template <typename T, int N>')) { throw "Vector template not found in the plain-files report" }
    Write-Host "  ok   Vector template found"

    # ---- test 4: health checker on the deliberately broken folder ----------------
    Write-Host "=== Test 4: health checker (sample-bad folder) ==="
    New-Item -ItemType Directory -Force "out\bad" | Out-Null
    & ".\cppparse.exe" -o "out\bad" -n "bad" --text-only "..\cppparse\sample-bad"
    if ($LASTEXITCODE -ne 0) { throw "cppparse returned exit code $LASTEXITCODE on sample-bad" }
    $health = Get-Content "out\bad\bad-health.txt" -Raw
    $codes = @('E100', 'E101', 'W102', 'I103', 'E110', 'E120', 'E121', 'W122', 'E123', 'E124', 'W125', 'W126', 'I127', 'W128',
               'E130', 'E131', 'W132', 'W133', 'E134', 'E135', 'E136', 'W137', 'W138', 'W140', 'W141', 'E142', 'E143', 'E144',
               'E145', 'I146', 'W147', 'W150', 'W151', 'W152', 'W153', 'W154', 'W155', 'E156', 'W157', 'E160', 'W161', 'E162',
               'E163', 'W164', 'W165')
    $failed = 0
    foreach ($code in $codes) {
        if ($health.Contains("[$code]")) { Write-Host "  ok   $code" } else { Write-Host "  FAIL code not reported: $code"; $failed++ }
    }
    & ".\cppparse.exe" --strict --health "..\cppparse\sample-bad" | Out-Null
    if ($LASTEXITCODE -eq 3) { Write-Host "  ok   --strict exit code 3" } else { Write-Host "  FAIL --strict exit code was $LASTEXITCODE"; $failed++ }
    if ($failed -ne 0) { throw "$failed check(s) failed on sample-bad" }

    # the good sample must produce only the intended findings
    $goodHealth = Get-Content "out\sample\Sample-health.txt" -Raw
    if ($goodHealth -match "errors (\d+), warnings (\d+), info (\d+)") {
        Write-Host "  sample (good) project: errors $($Matches[1]), warnings $($Matches[2]), info $($Matches[3])"
        if ([int]$Matches[1] -gt 1) { throw "the good sample reports more than the one intended error (Missing.h)" }
    }

    Write-Host ""
    Write-Host "=== cppparse: build and all tests succeeded ==="
    Write-Host "Reports: $Sandbox\out\sample\Sample-report.{txt,html,pdf}, Sample-diagram.{svg,dot}, Sample-health.txt"
    $global:LASTEXITCODE = 0
}
finally {
    Pop-Location
}
exit 0
