# self-hosting step: compiles every own source with the version 6 pipeline to out\selfobj, links the objects
# with the exception runtime and libcpp into out\selfhost\cppparse-self.exe, then runs that on a sample.
# The bootstrap stage (skipped with -SkipBootstrap) then has the self-built compiler compile the same sources
# to out\selfobj2, links out\selfhost2\cppparse-self2.exe from them and requires every object and listing to
# be byte-identical to the MSVC-built compiler's. -Compiler starts from another MSVC-built cppparse.exe
# (e.g. sandbox\vs\Debug\cppparse.exe from the solution) instead of sandbox\cppparse.exe
param([string]$Lvl = "0", [switch]$SkipCompile, [switch]$SkipBootstrap, [string]$Compiler = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Write-Host "=== starting compiler: $Exe ==="
New-Item -ItemType Directory -Force "$Sandbox\out\selfobj", "$Sandbox\out\selfhost" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$started = Get-Date
$units = $OwnSources
function SameBytes([string]$a, [string]$b) {
    if (-not (Test-Path $a) -or -not (Test-Path $b)) { return $false }
    return [System.Linq.Enumerable]::SequenceEqual([byte[]][System.IO.File]::ReadAllBytes("$Sandbox\$a"), [byte[]][System.IO.File]::ReadAllBytes("$Sandbox\$b"))
}
function CompileOwn([string]$compiler, [string]$objDir, [string]$libDir) {
    $failed = 0
    foreach ($f in $units) {
        $n = $f.BaseName
        if (Test-Path "$objDir\${n}6.obj") { Remove-Item "$objDir\${n}6.obj" }
        $out = & $compiler --compile6 "-O$Lvl" --obj --msvc-mangling -q @OwnIncludes -o $objDir -n $n $f.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$objDir\${n}6.obj")) { $failed++; Write-Host "FAIL $n"; Write-Host (($out.Split("`n") | Select-String -Pattern "error|note|  line" | Select-Object -First 4 | ForEach-Object { $_.Line }) -join "`n") }
    }
    Write-Host "own sources compiled: $($units.Count - $failed) of $($units.Count)"
    & $compiler --compile6 "-O$Lvl" --obj --msvc-mangling -q -I ..\include -o $libDir -n iostream "..\libcpp\iostream.cpp" | Out-Null
    return $failed
}
function LinkOwn([string]$objDir, [string]$libDir, [string]$exe) {
    $objs = @(); foreach ($f in $units) { $objs += "$objDir\$($f.BaseName)6.obj" }
    Write-Host "linking $($objs.Count) objects"
    if (Test-Path $exe) { Remove-Item $exe }
    & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe" @objs "$libDir\iostream6.obj" "out\selfhost\cppparse_rt.obj" libucrt.lib libvcruntime.lib libcmt.lib kernel32.lib legacy_stdio_definitions.lib 2>&1 | Select-Object -First 40 | ForEach-Object { Write-Host "$_" }
    return (Test-Path $exe)
}
# runs a self-built compiler on sample-opt and compares every output with that of the MSVC-built compiler
# (made once, under the same output name in out\selfhost\ref so that the files can be compared byte for byte)
function CheckSample([string]$exe, [string]$dir) {
    Write-Host "=== running $exe on examples\sample-opt ==="
    if (Test-Path "$dir\self-opt6.obj") { Remove-Item "$dir\self-opt6.obj" }
    & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o $dir -n self-opt "..\examples\sample-opt\opt.cpp" 2>&1 | Select-Object -First 20 | ForEach-Object { Write-Host "$_" }
    $rc = $LASTEXITCODE
    Write-Host "self-built compiler exit code $rc"
    if ($rc -ne 0 -or -not (Test-Path "$dir\self-opt6.obj")) { Write-Host "no object written"; return $false }
    $same = $true
    foreach ($suffix in @("6.obj", "6.asm", "-ir6.txt", "-asm6.txt", "-sym6.txt")) {
        if (-not (SameBytes "$dir\self-opt$suffix" "out\selfhost\ref\self-opt$suffix")) { Write-Host "differs from the MSVC-built compiler's output: $suffix"; $same = $false }
    }
    if ($same) { Write-Host "object and listings identical to the MSVC-built compiler's" }
    return $same
}
if (-not $SkipCompile) { $null = CompileOwn $Exe "out\selfobj" "out\selfhost" }
& $cl /nologo /c /EHsc /W3 "/Foout\selfhost\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
if (-not (LinkOwn "out\selfobj" "out\selfhost" "out\selfhost\cppparse-self.exe")) { Write-Host "link failed"; exit 1 }
New-Item -ItemType Directory -Force "$Sandbox\out\selfhost\ref" | Out-Null
& $Exe --compile6 -O0 --obj --msvc-mangling -q -I ..\include -o out\selfhost\ref -n self-opt "..\examples\sample-opt\opt.cpp" | Out-Null
if (-not (CheckSample "out\selfhost\cppparse-self.exe" "out\selfhost")) { exit 1 }
Write-Host ("stage 1 (MSVC-built compiler builds and checks cppparse-self.exe): {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
if ($SkipBootstrap) { exit 0 }

# generation two: cppparse-self.exe compiles the same sources. Its objects must equal the MSVC-built
# compiler's bit for bit, which also makes cppparse-self2.exe the same program as cppparse-self.exe
Write-Host "=== bootstrap: cppparse-self.exe compiles the own sources to out\selfobj2 ==="
$stage2 = Get-Date
New-Item -ItemType Directory -Force "$Sandbox\out\selfobj2", "$Sandbox\out\selfhost2" | Out-Null
$failed = CompileOwn "out\selfhost\cppparse-self.exe" "out\selfobj2" "out\selfhost2"
if ($failed -ne 0) { exit 1 }
if (-not (LinkOwn "out\selfobj2" "out\selfhost2" "out\selfhost2\cppparse-self2.exe")) { Write-Host "link of cppparse-self2.exe failed"; exit 1 }
$identical = 0; $differ = @()
foreach ($f in $units) {
    $n = $f.BaseName
    $bad = @(); foreach ($suffix in @("6.obj", "6.asm")) { if (-not (SameBytes "out\selfobj\$n$suffix" "out\selfobj2\$n$suffix")) { $bad += $suffix } }
    if ($bad.Count -eq 0) { $identical++ } else { $differ += "$n ($($bad -join ', '))" }
}
$libBad = @(); foreach ($suffix in @("6.obj", "6.asm")) { if (-not (SameBytes "out\selfhost\iostream$suffix" "out\selfhost2\iostream$suffix")) { $libBad += $suffix } }
if ($libBad.Count -ne 0) { $differ += "libcpp\iostream ($($libBad -join ', '))" }
$sampleOk = CheckSample "out\selfhost2\cppparse-self2.exe" "out\selfhost2"
Write-Host ("stage 2 (cppparse-self.exe builds and checks cppparse-self2.exe): {0:N0} s" -f ((Get-Date) - $stage2).TotalSeconds)
Write-Host "=== summary ==="
Write-Host "own units the self-built compiler compiles identically to the MSVC-built one (object and listing): $identical of $($units.Count)"
if ($libBad.Count -eq 0) { Write-Host "libcpp\iostream: identical" }
foreach ($d in $differ) { Write-Host "  differs: $d" }
if ($differ.Count -ne 0 -or -not $sampleOk) { Write-Host "bootstrap FAILED: generation two differs from generation one"; exit 1 }
Write-Host "bootstrap: generation two is identical to generation one"
exit 0
