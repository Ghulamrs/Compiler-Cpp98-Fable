# the matrix and linear-equation sample (examples\sample-matrix, driver in examples\sample-matrix-driver):
#   1. qualifies every unit as strict C++98: clang++ -std=c++98 -pedantic-errors -Wall -Wextra -Werror against
#      cppparse's own headers (MSVC's library needs C++14, so it cannot take part in a C++98 check)
#   2. builds the whole program with clang (-O0, -O2) and with cl (/Od, /O2) and runs it: the outputs of each
#      compiler must agree across its levels and with each other. -UpdateOracle stores them as
#      oracle-clang-cpp98.txt and oracle-cl.txt next to the driver; otherwise they are compared with those files
#   3. compiles matrix.cpp and solver.cpp with cppparse --compile6 --msvc-mangling at -O0..-O3, links them with
#      the cl-built driver and the exception runtime, and requires the output to equal both oracles
param([switch]$UpdateOracle, [string]$Compiler = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$clang = "$VsRoot\VC\Tools\Llvm\x64\bin\clang++.exe"
$out = "out\matrix"
New-Item -ItemType Directory -Force $out | Out-Null
$lib = @("..\examples\sample-matrix\matrix.cpp", "..\examples\sample-matrix\solver.cpp")
$driver = "..\examples\sample-matrix-driver\driver-matrix.cpp"
$oracleDir = "..\examples\sample-matrix-driver"
$failed = 0

function Run([string]$exe) {
    $text = & $exe 2>&1 | Out-String
    return @{ Code = $LASTEXITCODE; Text = ($text -replace "`r`n", "`n").TrimEnd() + "`n" }
}
function Report([string]$name, [bool]$ok, [string]$detail = "") {
    if ($ok) { Write-Host "  ok   $name" } else { Write-Host "  FAIL $name"; if ($detail) { Write-Host $detail }; $script:failed++ }
}
function FirstDifference([string]$a, [string]$b) {
    $la = $a.Split("`n"); $lb = $b.Split("`n")
    for ($i = 0; $i -lt [Math]::Max($la.Count, $lb.Count); $i++) {
        if ($i -ge $la.Count -or $i -ge $lb.Count -or $la[$i] -ne $lb[$i]) { return "       line $($i + 1):`n         got:      $($la[$i])`n         expected: $($lb[$i])" }
    }
    return ""
}

Write-Host "=== strict C++98 (clang++ -std=c++98 -pedantic-errors -Werror) ==="
$c98 = @("-std=c++98", "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-nostdinc", "-isystem", "..\include", "-I", "..", "-I", "..\examples\sample-matrix")
foreach ($f in $lib + $driver) {
    $msg = & $clang @c98 -fsyntax-only $f 2>&1 | Out-String
    Report "$(Split-Path $f -Leaf) is C++98" ($LASTEXITCODE -eq 0) $msg
}

Write-Host "=== reference builds ==="
$outputs = @{}
foreach ($lvl in @("O0", "O2")) {
    $msg = & $clang @c98 "-$lvl" @lib $driver -o "$out\matrix-clang-$lvl.exe" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "clang -$lvl build" $false $msg; continue }
    $r = Run "$out\matrix-clang-$lvl.exe"
    Report "clang -$lvl build runs, ALL OK" ($r.Code -eq 0 -and $r.Text -match 'ALL OK') $r.Text
    $outputs["clang-$lvl"] = $r.Text
}
foreach ($lvl in @("Od", "O2")) {
    $msg = & $cl /nologo /EHsc /W4 /WX "/$lvl" /I.. /I..\examples\sample-matrix "/Fo$out\" "/Fe$out\matrix-cl-$lvl.exe" @lib $driver 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "cl /$lvl build" $false $msg; continue }
    $r = Run "$out\matrix-cl-$lvl.exe"
    Report "cl /$lvl build runs, ALL OK" ($r.Code -eq 0 -and $r.Text -match 'ALL OK') $r.Text
    $outputs["cl-$lvl"] = $r.Text
}
Report "clang -O0 and -O2 print the same" ($outputs["clang-O0"] -ceq $outputs["clang-O2"]) (FirstDifference $outputs["clang-O2"] $outputs["clang-O0"])
Report "cl /Od and /O2 print the same" ($outputs["cl-Od"] -ceq $outputs["cl-O2"]) (FirstDifference $outputs["cl-O2"] $outputs["cl-Od"])
Report "clang and cl print the same" ($outputs["clang-O2"] -ceq $outputs["cl-O2"]) (FirstDifference $outputs["cl-O2"] $outputs["clang-O2"])

$oracles = [ordered]@{ "oracle-clang-cpp98.txt" = $outputs["clang-O2"]; "oracle-cl.txt" = $outputs["cl-O2"] }
if ($UpdateOracle) {
    if ($failed -ne 0) { Write-Host "oracles NOT written: the reference builds disagree or fail"; exit 1 }
    foreach ($k in $oracles.Keys) { [System.IO.File]::WriteAllText("$Sandbox\$oracleDir\$k", $oracles[$k]); Write-Host "  wrote $oracleDir\$k" }
}
$expected = [ordered]@{}
foreach ($k in $oracles.Keys) {
    $expected[$k] = ([System.IO.File]::ReadAllText("$Sandbox\$oracleDir\$k") -replace "`r`n", "`n")
    Report "today's reference output equals $k" ($oracles[$k] -ceq $expected[$k]) (FirstDifference $oracles[$k] $expected[$k])
}

Write-Host "=== cppparse ($Exe) ==="
& $cl /nologo /c /EHsc /W3 "/Fo$out\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
& $cl /nologo /c /EHsc /W4 /WX "/Fo$out\driver-matrix.obj" $driver | Out-Null
foreach ($lvl in 0..3) {
    $objs = @(); $ok = $true
    foreach ($f in $lib) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($f) + "M$lvl"
        if (Test-Path "$out\${name}6.obj") { Remove-Item "$out\${name}6.obj" }
        $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -I .. -I ..\examples\sample-matrix -o $out -n $name $f 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$out\${name}6.obj")) { Report "cppparse -O$lvl compiles $(Split-Path $f -Leaf)" $false $msg; $ok = $false }
        $objs += "$out\${name}6.obj"
    }
    if (-not $ok) { continue }
    $msg = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$out\matrix-cppparse-O$lvl.exe" "$out\driver-matrix.obj" @objs "$out\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "cppparse -O$lvl link" $false $msg; continue }
    $r = Run "$out\matrix-cppparse-O$lvl.exe"
    Report "cppparse -O${lvl}: runs, ALL OK (exit $($r.Code))" ($r.Code -eq 0 -and $r.Text -match 'ALL OK') $r.Text
    foreach ($k in $expected.Keys) { Report "cppparse -O$lvl output equals $k" ($r.Text -ceq $expected[$k]) (FirstDifference $r.Text $expected[$k]) }
}

if ($failed -ne 0) { Write-Host "matrix sample: $failed check(s) FAILED"; exit 1 }
Write-Host "matrix sample: all checks passed"
exit 0
