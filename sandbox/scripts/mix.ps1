# bisection of the self-built compiler: cl compiles the own sources against cppparse's own headers (so the
# objects share the library layout with cppparse's), the reference is linked and run, then one unit at a
# time is replaced by cppparse's object (from out\selfobj, see selfhost.ps1) and the result is run
param([string]$Stage = "all")
. "$PSScriptRoot\toolset.ps1"
New-Item -ItemType Directory -Force "$Sandbox\out\refobj", "$Sandbox\out\mix" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$units = @(); foreach ($f in (Get-ChildItem "..\cppparse\*.cpp") + (Get-ChildItem "..\cppparse\legacy\*.cpp")) { $units += $f }
if ($Stage -eq "all" -or $Stage -eq "ref") {
    $bad = 0
    foreach ($f in $units) {
        $n = $f.BaseName
        $out = & $cl /nologo /c /EHsc /W3 /Od /X /I..\cppparse /I..\cppparse\legacy /I..\cppparse\include "/Foout\refobj\$n.obj" $f.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { $bad++; Write-Host "cl FAILED on $n :"; Write-Host ($out.Split("`n") | Select-String -Pattern "error" | Select-Object -First 4 | ForEach-Object { $_.Line }) }
    }
    & $cl /nologo /c /EHsc /W3 /Od /X /I..\cppparse\include "/Foout\refobj\iostream.obj" "..\cppparse\libcpp\iostream.cpp" | Out-Null
    & $cl /nologo /c /EHsc /W3 "/Foout\refobj\cppparse_rt.obj" "..\cppparse\runtime\cppparse_rt.cpp" | Out-Null
    Write-Host "reference objects with cppparse's headers: $($units.Count - $bad) ok, $bad failed"
}
function LinkAndRun([string]$name, [string[]]$objs) {
    $exe = "out\mix\$name.exe"
    if (Test-Path $exe) { Remove-Item $exe }
    $lo = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe" @objs "out\refobj\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Out-String
    if (-not (Test-Path $exe)) { return "link failed: " + (($lo.Split("`n") | Select-String -Pattern "error" | Select-Object -First 2 | ForEach-Object { $_.Line }) -join " | ") }
    $null = & $exe --compile6 -O0 -q -I ..\cppparse\include -o out\mix -n probe "..\cppparse\examples\sample-opt\opt.cpp" 2>&1
    return "exit $LASTEXITCODE"
}
if ($Stage -eq "all" -or $Stage -eq "mix") {
    $ref = @(); foreach ($f in $units) { $ref += "out\refobj\$($f.BaseName).obj" }
    Write-Host ("all reference objects: " + (LinkAndRun "ref" ($ref + @("out\refobj\iostream.obj"))))
    $ours = @(); foreach ($f in $units) { $ours += "out\selfobj\$($f.BaseName)6.obj" }
    Write-Host ("all cppparse objects:  " + (LinkAndRun "ours" ($ours + @("out\selfhost\iostream6.obj"))))
    Write-Host "=== one cppparse object at a time among reference objects ==="
    for ($i = 0; $i -lt $units.Count; $i++) {
        $mixed = @(); for ($k = 0; $k -lt $units.Count; $k++) { if ($k -eq $i) { $mixed += $ours[$k] } else { $mixed += $ref[$k] } }
        Write-Host ("{0,-14} {1}" -f $units[$i].BaseName, (LinkAndRun "mix" ($mixed + @("out\refobj\iostream.obj"))))
    }
}
