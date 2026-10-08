# self-hosting step: compiles every own source with the version 6 pipeline to out\selfobj, links the objects
# with the exception runtime and libcpp into out\selfhost\cppparse-self.exe, then runs that on a sample
param([string]$Lvl = "0", [switch]$SkipCompile)
. "$PSScriptRoot\toolset.ps1"
New-Item -ItemType Directory -Force "$Sandbox\out\selfobj", "$Sandbox\out\selfhost" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$units = @(); foreach ($f in (Get-ChildItem "..\cppparse\*.cpp") + (Get-ChildItem "..\cppparse\legacy\*.cpp")) { $units += $f }
if (-not $SkipCompile) {
    $failed = 0
    foreach ($f in $units) {
        $n = $f.BaseName
        $out = & $Exe --compile6 "-O$Lvl" --obj --msvc-mangling -q -I ..\cppparse -I ..\cppparse\legacy -I ..\cppparse\include -o out\selfobj -n $n $f.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "out\selfobj\${n}6.obj")) { $failed++; Write-Host "FAIL $n"; Write-Host (($out.Split("`n") | Select-String -Pattern "error|note|  line" | Select-Object -First 4 | ForEach-Object { $_.Line }) -join "`n") }
    }
    Write-Host "own sources compiled: $($units.Count - $failed) of $($units.Count)"
}
& $Exe --compile6 "-O$Lvl" --obj --msvc-mangling -q -I ..\cppparse\include -o out\selfhost -n iostream "..\cppparse\libcpp\iostream.cpp" | Out-Null
& $cl /nologo /c /EHsc /W3 "/Foout\selfhost\cppparse_rt.obj" "..\cppparse\runtime\cppparse_rt.cpp" | Out-Null
$objs = @(); foreach ($f in $units) { $objs += "out\selfobj\$($f.BaseName)6.obj" }
Write-Host "linking $($objs.Count) objects"
& $link /nologo /SUBSYSTEM:CONSOLE "/OUT:out\selfhost\cppparse-self.exe" @objs "out\selfhost\iostream6.obj" "out\selfhost\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | ForEach-Object { "$_" } | Select-Object -First 40
if (-not (Test-Path "out\selfhost\cppparse-self.exe")) { Write-Host "link failed"; exit 1 }
Write-Host "=== running the self-built compiler on examples\sample-opt ==="
if (Test-Path "out\selfhost\self-opt6.obj") { Remove-Item "out\selfhost\self-opt6.obj" }
& "out\selfhost\cppparse-self.exe" --compile6 -O0 --obj --msvc-mangling -I ..\cppparse\include -o out\selfhost -n self-opt "..\cppparse\examples\sample-opt\opt.cpp" 2>&1 | ForEach-Object { "$_" } | Select-Object -First 20
$rc = $LASTEXITCODE
Write-Host "self-built compiler exit code $rc"
if ($rc -ne 0 -or -not (Test-Path "out\selfhost\self-opt6.obj")) { Write-Host "no object written"; exit 1 }
# the same compilation by the MSVC-built compiler, under the same output name in its own directory so
# that the files can be compared byte for byte: every output must be identical
New-Item -ItemType Directory -Force "$Sandbox\out\selfhost\ref" | Out-Null
& $Exe --compile6 -O0 --obj --msvc-mangling -q -I ..\cppparse\include -o out\selfhost\ref -n self-opt "..\cppparse\examples\sample-opt\opt.cpp" | Out-Null
$same = $true
foreach ($suffix in @("6.obj", "6.asm", "-ir6.txt", "-asm6.txt", "-sym6.txt")) {
    $a = [System.IO.File]::ReadAllBytes("$Sandbox\out\selfhost\self-opt$suffix")
    $b = [System.IO.File]::ReadAllBytes("$Sandbox\out\selfhost\ref\self-opt$suffix")
    $equal = $a.Length -eq $b.Length
    if ($equal) { for ($i = 0; $i -lt $a.Length; $i++) { if ($a[$i] -ne $b[$i]) { $equal = $false; break } } }
    if (-not $equal) { Write-Host "differs from the MSVC-built compiler's output: $suffix"; $same = $false }
}
if ($same) { Write-Host "object and listings identical to the MSVC-built compiler's" } else { exit 1 }
