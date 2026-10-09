# links the reference (cl against cppparse's headers, out\refobj) and the self-built (out\selfobj) compiler with
# /MAP into out\maps, then compares the two map files unit by unit (mapdiff.py)
. "$PSScriptRoot\toolset.ps1"
New-Item -ItemType Directory -Force "$Sandbox\out\maps" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$units = @(); foreach ($f in $OwnSources) { $units += $f.BaseName }
$ref = @(); foreach ($u in $units) { $ref += "out\refobj\$u.obj" }
$ours = @(); foreach ($u in $units) { $ours += "out\selfobj\${u}6.obj" }
& $link /nologo /SUBSYSTEM:CONSOLE /MAP:out\maps\ref.map "/OUT:out\maps\ref.exe" @ref "out\refobj\iostream.obj" "out\refobj\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Select-String -Pattern "error" | ForEach-Object { $_.Line }
& $link /nologo /SUBSYSTEM:CONSOLE /MAP:out\maps\ours.map "/OUT:out\maps\ours.exe" @ours "out\selfhost\iostream6.obj" "out\refobj\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Select-String -Pattern "error" | ForEach-Object { $_.Line }
if ((Test-Path "out\maps\ref.map") -and (Test-Path "out\maps\ours.map")) {
    python "$PSScriptRoot\mapdiff.py" "out\maps\ref.map" "out\maps\ours.map"
    # one file per unit on each side (out\maps\ref, out\maps\ours) for a directory comparison
    python "$PSScriptRoot\mapsplit.py" "out\maps\ref.map" "out\maps\ours.map" "out\maps"
    # folder comparison of the two directories: per-unit summary and out\maps\compare\<unit>.txt side by side
    python "$PSScriptRoot\mapcompare.py" "out\maps"
}
else { Write-Host "map files missing" }
