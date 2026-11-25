# ehbench.ps1 - the throw-and-catch path with cppparse's object of the runtime against cl's (row P1 stage 3,
# docs\design-selfhost.md 5 and the risk row of 6): examples\sample-target\ehbench.cpp compiled by cppparse at -O2 in
# the table and sjlj models, linked once with each object of the runtime (cl's, cppparse's: Build-Runtime), each exe
# run five times; the median of the nanoseconds per throw is reported. Not a gate: it exits 1 only when a program
# fails. -Linker ms|gride: the linker of every program (toolset.ps1, Link-Program).
param([string]$Compiler = "", [string]$Linker = "", [int]$Runs = 5)
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Assert-Linker
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$o = "out\ehbench"
New-Item -ItemType Directory -Force $o | Out-Null
$failed = 0
# the two objects of the runtime, as the scripts build them (CPPPARSE_RT chooses; set here for each in turn)
$saved = $env:CPPPARSE_RT
$env:CPPPARSE_RT = "cl"; $msg = Build-Runtime "$o\rt-cl.obj" @("/O2", "/EHsc", "/W3")
if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL cl's object of the runtime: $msg"; $failed++ }
$env:CPPPARSE_RT = "cppparse"; $msg = Build-Runtime "$o\rt-cppparse.obj"
if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL cppparse's object of the runtime: $msg"; $failed++ }
$env:CPPPARSE_RT = $saved
Write-Host "=== ehbench: nanoseconds per throw and rethrow, the median of $Runs runs (linker: $Linker) ==="
foreach ($m in @("table", "sjlj")) {
    $null = & $Exe --compile6 -O2 "--eh=$m" --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n "bench-$m" ..\examples\sample-target\ehbench.cpp 2>&1
    $line = "  --eh=${m}:"
    foreach ($rt in @(@("cl's", "$o\rt-cl.obj"), @("cppparse's", "$o\rt-cppparse.obj"))) {
        $exe2 = "$o\bench-$m-$($rt[0] -replace "'s", '').exe"
        $null = Link-Program @("$o\bench-${m}6.obj", $rt[1]) $exe2
        if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link of $exe2"; $failed++; continue }
        $ns = @()
        for ($k = 0; $k -lt $Runs; $k++) {
            $out = & ".\$exe2" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0 -or $out -notmatch '(\d+) ns per throw') { Write-Host "  FAIL $exe2 (exit $LASTEXITCODE): $out"; $failed++; break }
            $ns += [int]$Matches[1]
        }
        if ($ns.Count -eq $Runs) { $sorted = $ns | Sort-Object; $line += " $($rt[0]) object $($sorted[[int][Math]::Floor($Runs / 2)]) ns ($($ns -join ', '));" }
    }
    Write-Host $line
}
if ($failed) { Write-Host "ehbench: $failed program(s) failed"; exit 1 }
Write-Host "ehbench: reported (not a gate)$(Get-KnownDifferences)"
exit 0
