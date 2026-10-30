# bisection of the self-built compiler: cl compiles the own sources against cppparse's own headers (so the
# objects share the library layout with cppparse's), the reference is linked and run, then one unit at a
# time is replaced by cppparse's object (from out\selfobj, see selfhost.ps1) and the result is run.
# Exit code 1 when a reference object does not compile or any build fails to link, times out or exits non-zero.
param([string]$Stage = "all")
. "$PSScriptRoot\toolset.ps1"
New-Item -ItemType Directory -Force "$Sandbox\out\refobj", "$Sandbox\out\mix" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$units = $OwnSources
# cl with /X sees only cppparse's headers: the same include folders as the self-build, in cl's spelling
$clIncludes = @(); for ($k = 1; $k -lt $OwnIncludes.Count; $k += 2) { $clIncludes += "/I" + $OwnIncludes[$k] }
$failures = 0
if ($Stage -eq "all" -or $Stage -eq "ref") {
    $bad = 0
    foreach ($f in $units) {
        $n = $f.BaseName
        $out = & $cl /nologo /c /EHsc /W3 /Od /X @clIncludes "/Foout\refobj\$n.obj" $f.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { $bad++; Write-Host "cl FAILED on $n :"; Write-Host ($out.Split("`n") | Select-String -Pattern "error" | Select-Object -First 4 | ForEach-Object { $_.Line }) }
    }
    & $cl /nologo /c /EHsc /W3 /Od /X /I..\include "/Foout\refobj\iostream.obj" "..\libcpp\iostream.cpp" | Out-Null
    & $cl /nologo /c /EHsc /W3 "/Foout\refobj\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
    Write-Host "reference objects with cppparse's headers: $($units.Count - $bad) ok, $bad failed"
    $failures += $bad
}
function LinkAndRun([string]$name, [string[]]$objs) {
    $exe = "out\mix\$name.exe"
    if (Test-Path $exe) { Remove-Item $exe }
    $lo = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe" @objs "out\refobj\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Out-String
    if (-not (Test-Path $exe)) { return "link failed: " + (($lo.Split("`n") | Select-String -Pattern "error" | Select-Object -First 2 | ForEach-Object { $_.Line }) -join " | ") }
    $p = Start-Process -FilePath $exe -ArgumentList "--compile6 -O0 -q -I ..\include -o out\mix -n probe ..\examples\sample-opt\opt.cpp" -NoNewWindow -PassThru -RedirectStandardOutput "out\mix\$name.out" -RedirectStandardError "out\mix\$name.err"
    $null = $p.Handle   # keeps the process handle so that ExitCode is available after the exit
    if (-not $p.WaitForExit(120000)) { $p.Kill(); return "timeout" }
    return "exit $($p.ExitCode)"
}
if ($Stage -eq "all" -or $Stage -eq "mix") {
    $ref = @(); foreach ($f in $units) { $ref += "out\refobj\$($f.BaseName).obj" }
    $r = LinkAndRun "ref" ($ref + @("out\refobj\iostream.obj"))
    Write-Host ("all reference objects: " + $r); if ($r -ne "exit 0") { $failures++ }
    $ours = @(); foreach ($f in $units) { $ours += "out\selfobj\$($f.BaseName)6.obj" }
    $r = LinkAndRun "ours" ($ours + @("out\selfhost\iostream6.obj"))
    Write-Host ("all cppparse objects:  " + $r); if ($r -ne "exit 0") { $failures++ }
    Write-Host "=== one cppparse object at a time among reference objects ==="
    for ($i = 0; $i -lt $units.Count; $i++) {
        $mixed = @(); for ($k = 0; $k -lt $units.Count; $k++) { if ($k -eq $i) { $mixed += $ours[$k] } else { $mixed += $ref[$k] } }
        $r = LinkAndRun "mix" ($mixed + @("out\refobj\iostream.obj"))
        Write-Host ("{0,-14} {1}" -f $units[$i].BaseName, $r); if ($r -ne "exit 0") { $failures++ }
    }
}
if ($failures -ne 0) { Write-Host "mix: $failures failure(s)"; exit 1 }
Write-Host "mix: all builds linked and ran (exit 0)"
exit 0
