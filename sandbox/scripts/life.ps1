# compiles examples\sample-life with the version 6 pipeline at the given level, links the native driver, runs it.
# -Linker ms|gride: the linker (toolset.ps1, Link-Program)
param([string]$Lvl = "0", [string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
Assert-Linker
New-Item -ItemType Directory -Force "$Sandbox\out\life" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
& $Exe --compile6 "-O$Lvl" --obj --msvc-mangling -o out\life -n "life$Lvl" "..\examples\sample-life\life.cpp" 2>&1 | Select-String -Pattern "error|sema|compile6|note" | ForEach-Object { $_.Line }
if (-not (Test-Path "out\life\life${Lvl}6.obj")) { Write-Host "no object"; exit 1 }
& $cl /nologo /c /EHsc /W3 "/Foout\life\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
Build-Program @("/EHsc", "/W3", "/DLIFE_NRVO=$([int]($Lvl -ge 1))", "/Foout\life\driver-life.obj", "..\examples\sample-life-driver\driver-life.cpp") @("out\life\driver-life.obj", "out\life\life${Lvl}6.obj", "out\life\cppparse_rt.obj") "out\life\driver$Lvl.exe" | ForEach-Object { "$_" }
if (-not (Test-Path "out\life\driver$Lvl.exe")) { Write-Host "link failed"; exit 1 }
& "out\life\driver$Lvl.exe" 2>&1 | ForEach-Object { "$_" }
Write-Host "driver exit code $LASTEXITCODE"
