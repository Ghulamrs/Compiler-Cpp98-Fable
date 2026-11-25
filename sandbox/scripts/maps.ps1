# links the reference (cl against cppparse's headers, out\refobj) and the self-built (out\selfobj) compiler with
# /MAP into out\maps, then compares the two map files unit by unit (mapdiff.py). The links are Microsoft's under
# -Linker gride too (printed as "not under GRIDE's link"): the comparison reads link.exe's map format.
param([string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
Assert-Linker
if ($Linker -eq "gride") { Write-NotUnderGride "maps.ps1 links with Microsoft's link: mapdiff.py, mapsplit.py and mapcompare.py read link.exe's map format" }
New-Item -ItemType Directory -Force "$Sandbox\out\maps" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$units = @(); foreach ($f in $OwnSources) { $units += $f.BaseName }
$ref = @(); foreach ($u in $units) { $ref += "out\refobj\$u.obj" }
$ours = @(); foreach ($u in $units) { $ours += "out\selfobj\${u}6.obj" }
Link-Program ($ref + @("out\refobj\iostream.obj", "out\refobj\cppparse_rt.obj")) "out\maps\ref.exe" @("/MAP:out\maps\ref.map") $null "ms" | Select-String -Pattern "error" | ForEach-Object { $_.Line }
Link-Program ($ours + @("out\selfhost\iostream6.obj", "out\refobj\cppparse_rt.obj")) "out\maps\ours.exe" @("/MAP:out\maps\ours.map") $null "ms" | Select-String -Pattern "error" | ForEach-Object { $_.Line }
if ((Test-Path "out\maps\ref.map") -and (Test-Path "out\maps\ours.map")) {
    python "$PSScriptRoot\mapdiff.py" "out\maps\ref.map" "out\maps\ours.map"
    # one file per unit on each side (out\maps\ref, out\maps\ours) for a directory comparison
    python "$PSScriptRoot\mapsplit.py" "out\maps\ref.map" "out\maps\ours.map" "out\maps"
    # folder comparison of the two directories: per-unit summary and out\maps\compare\<unit>.txt side by side
    python "$PSScriptRoot\mapcompare.py" "out\maps"
}
else { Write-Host "map files missing" }
