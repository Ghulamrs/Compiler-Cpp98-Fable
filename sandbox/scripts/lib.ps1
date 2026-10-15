# compiles examples\sample-lib\*.cpp and libcpp\iostream.cpp with the version 6 pipeline, links the native
# driver with cl and runs it: the standard-library check outside the test suite
param([string]$Lvl = "0")
. "$PSScriptRoot\toolset.ps1"
New-Item -ItemType Directory -Force "$Sandbox\out\lib" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$objs = @()
foreach ($unit in @("examples\sample-lib\lib1.cpp", "examples\sample-lib\lib2.cpp", "examples\sample-lib\lib3.cpp", "examples\sample-lib\lib4.cpp", "libcpp\iostream.cpp")) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($unit) + $Lvl
    & $Exe --compile6 "-O$Lvl" --obj --msvc-mangling -I ..\include -o out\lib -n $name "..\$unit" 2>&1 | Select-String -Pattern "error|sema|compile6|note" | ForEach-Object { $_.Line }
    if (-not (Test-Path "out\lib\${name}6.obj")) { Write-Host "no object for $unit"; exit 1 }
    $objs += "out\lib\${name}6.obj"
}
& $cl /nologo /c /EHsc /W3 "/Foout\lib\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
& $cl /nologo /EHsc /W3 "/Foout\lib\driver-lib.obj" "/Feout\lib\libdriver$Lvl.exe" "..\examples\sample-lib-driver\driver-lib.cpp" @objs "out\lib\cppparse_rt.obj" legacy_stdio_definitions.lib 2>&1 | ForEach-Object { "$_" }
if (-not (Test-Path "out\lib\libdriver$Lvl.exe")) { Write-Host "link failed"; exit 1 }
& "out\lib\libdriver$Lvl.exe" 2>&1 | ForEach-Object { "$_" }
Write-Host "driver exit code $LASTEXITCODE"
