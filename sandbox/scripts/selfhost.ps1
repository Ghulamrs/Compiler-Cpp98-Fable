# self-hosting step: compiles every own source with the version 6 pipeline to out\selfobj, links the objects
# with the exception runtime and libcpp (both compiled by the same compiler) into out\selfhost\cppparse-self.exe, then runs that on a sample.
# The bootstrap stage (skipped with -SkipBootstrap) then has the self-built compiler compile the same sources
# to out\selfobj2, links out\selfhost2\cppparse-self2.exe from them and requires every object and listing to
# be byte-identical to the MSVC-built compiler's. -Compiler starts from another MSVC-built cppparse.exe
# (e.g. sandbox\vs\Debug\cppparse.exe from the solution) instead of sandbox\cppparse.exe
#
# -Linker gride links both generations with GRIDE's link and /timestamp:0 in place of Microsoft's link (row P1,
# docs\design-selfhost.md, level 2a; Microsoft's link stays the default, -Linker ms). Its folders carry the linker's
# name, so that the default's stay as they are: out\selfobj-gride, out\selfhost-gride, out\selfobj2-gride,
# out\selfhost2-gride. Beside the objects and listings the two exes must then be byte-identical, which Microsoft's
# link, which stamps the time into an image, does not allow. Each link writes a map, and the script reads from it
# whose operator new the compiler got: libcpp's is the one meant (4.6's list of what differs under GRIDE's link).
# Where GRIDE is not found the script prints "skipped: GRIDE not found" and exits with 77, which is not a pass.
# -Linker gride -Relink compiles nothing: it takes the objects of the two generations as the run with Microsoft's
# link at the same -Lvl left them (out\selfobj, out\selfobj2 and the two iostream6.obj; they must be identical),
# links each set with GRIDE's link and compares the exes. That is harness.ps1's check, a matter of seconds; the run
# without -Relink is the whole self-host under GRIDE's link, where the GRIDE-linked compiler compiles generation two.
#
# The runtime (runtime\cppparse_rt.cpp) is compiled by the compiler of each generation, at the same -O and always
# --eh=table (its object carries no model mark, so it serves every model), into that generation's folder beside libcpp's
# object (row P1 stage 3, docs\design-selfhost.md level 1): every object of cppparse-self.exe is cppparse's, the script
# runs cl nowhere, and generation two's runtime object and listing must equal generation one's as the others must.
#
# -G is the self-host with CodeView types and variables (row M10, docs\design-codeview.md 5.5/2): every compilation of
# both generations runs with -g (through the environment variable CPPPARSE_G, which the self-built compiler reads as
# the starting one does), in folders of its own (out\selfobj-g, out\selfhost-g, out\selfobj2-g, out\selfhost2-g).
# Generation two's objects and listings must be generation one's: the type indices and every record are a function
# of the source, whichever compiler built the compiler. -G2 is the same with -g2 (the member functions in the classes,
# CPPPARSE_G=2), in folders out\selfobj-g2 and so on.
param([string]$Lvl = "0", [switch]$SkipCompile, [switch]$SkipBootstrap, [string]$Compiler = "", [string]$Linker = "", [switch]$Relink, [switch]$G, [switch]$G2)
if ($G2) { $env:CPPPARSE_G = "2" } elseif ($G) { $env:CPPPARSE_G = "1" }
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
$gride = ($Linker -eq "gride")
if ($Relink -and -not $gride) { Write-Host "-Relink goes with -Linker gride"; exit 1 }
if ($gride) {
    Show-Gride
    if ($GrideRoot -eq "") { Write-Host "skipped: GRIDE not found"; exit $GrideSkipCode }
}
Write-Host "=== starting compiler: $Exe ==="
# the folders: generation one's objects and its exe (with libcpp's object, the runtime's and the reference of
# CheckSample), generation two's. With -Relink the objects are read where the default run left them
$debugTag = if ($G2) { "-g2" } elseif ($G) { "-g" } else { "" }
$tag = $(if ($gride) { "-gride" } else { "" }) + $debugTag
$obj1 = "out\selfobj$tag"; $host1 = "out\selfhost$tag"; $obj2 = "out\selfobj2$tag"; $host2 = "out\selfhost2$tag"
$from1 = $obj1; $lib1 = $host1; $from2 = $obj2; $lib2 = $host2
if ($Relink) { $from1 = "out\selfobj$debugTag"; $lib1 = "out\selfhost$debugTag"; $from2 = "out\selfobj2$debugTag"; $lib2 = "out\selfhost2$debugTag" }
$exe1 = "$host1\cppparse-self.exe"; $exe2 = "$host2\cppparse-self2.exe"
New-Item -ItemType Directory -Force "$Sandbox\$from1", "$Sandbox\$host1" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$started = Get-Date
$units = $OwnSources
# what a folder of objects was compiled as; -Relink requires it of both generations
$stamp = "-O$Lvl" + $(if ($env:CPPPARSE_EH) { " --eh=$env:CPPPARSE_EH" } else { "" }) + $(if ($env:CPPPARSE_G -eq "1") { " -g" } elseif ($env:CPPPARSE_G -eq "2") { " -g2" } else { "" })
function SameBytes([string]$a, [string]$b) {
    if (-not (Test-Path $a) -or -not (Test-Path $b)) { return $false }
    return [System.Linq.Enumerable]::SequenceEqual([byte[]][System.IO.File]::ReadAllBytes("$Sandbox\$a"), [byte[]][System.IO.File]::ReadAllBytes("$Sandbox\$b"))
}
function CompileOwn([string]$compiler, [string]$objDir, [string]$libDir) {
    $failed = 0
    if (Test-Path "$objDir\selfhost-level.txt") { Remove-Item "$objDir\selfhost-level.txt" }
    foreach ($f in $units) {
        $n = $f.BaseName
        if (Test-Path "$objDir\${n}6.obj") { Remove-Item "$objDir\${n}6.obj" }
        $out = & $compiler --compile6 "-O$Lvl" --obj --msvc-mangling -q @OwnIncludes -o $objDir -n $n $f.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$objDir\${n}6.obj")) { $failed++; Write-Host "FAIL $n"; Write-Host (($out.Split("`n") | Select-String -Pattern "error|note|  line" | Select-Object -First 4 | ForEach-Object { $_.Line }) -join "`n") }
    }
    Write-Host "own sources compiled: $($units.Count - $failed) of $($units.Count)"
    & $compiler --compile6 "-O$Lvl" --obj --msvc-mangling -q -I ..\include -o $libDir -n iostream "..\libcpp\iostream.cpp" | Out-Null
    # the runtime, by the same compiler: --eh=table whatever CPPPARSE_EH says (it has no construct of a model)
    if (Test-Path "$libDir\cppparse_rt6.obj") { Remove-Item "$libDir\cppparse_rt6.obj" }
    $rtOut = & $compiler --compile6 "-O$Lvl" --eh=table --obj --msvc-mangling -q -I ..\include -I ..\platform -o $libDir -n cppparse_rt "..\runtime\cppparse_rt.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$libDir\cppparse_rt6.obj") -or $rtOut.Trim() -ne "") { $failed++; Write-Host "FAIL runtime\cppparse_rt.cpp"; Write-Host $rtOut }
    else { Write-Host "runtime\cppparse_rt.cpp compiled by the same compiler" }
    if ($failed -eq 0) { Set-Content -Encoding ascii "$objDir\selfhost-level.txt" $stamp }
    return $failed
}
function LinkOwn([string]$objDir, [string]$libDir, [string]$exe) {
    $objs = @(); foreach ($f in $units) { $objs += "$objDir\$($f.BaseName)6.obj" }
    Write-Host "linking $($objs.Count) objects"
    $options = @(); if ($gride) { $options = @("/MAP:" + [IO.Path]::ChangeExtension($exe, ".map")) }
    Link-Program ($objs + @("$libDir\iostream6.obj", "$libDir\cppparse_rt6.obj")) $exe $options | Select-Object -First 40 | ForEach-Object { Write-Host "$_" }
    return (Test-Path $exe)
}
# runs a self-built compiler on sample-opt and compares every output with that of the MSVC-built compiler
# (made once, under the same output name in <generation one's folder>\ref so that the files can be compared byte for byte)
function CheckSample([string]$exe, [string]$dir) {
    Write-Host "=== running $exe on examples\sample-opt ==="
    if (Test-Path "$dir\self-opt6.obj") { Remove-Item "$dir\self-opt6.obj" }
    & $exe --compile6 -O0 --obj --msvc-mangling -I ..\include -o $dir -n self-opt "..\examples\sample-opt\opt.cpp" 2>&1 | Select-Object -First 20 | ForEach-Object { Write-Host "$_" }
    $rc = $LASTEXITCODE
    Write-Host "self-built compiler exit code $rc"
    if ($rc -ne 0 -or -not (Test-Path "$dir\self-opt6.obj")) { Write-Host "no object written"; return $false }
    $same = $true
    foreach ($suffix in @("6.obj", "6.asm", "-ir6.txt", "-asm6.txt", "-sym6.txt")) {
        if (-not (SameBytes "$dir\self-opt$suffix" "$host1\ref\self-opt$suffix")) { Write-Host "differs from the MSVC-built compiler's output: $suffix"; $same = $false }
    }
    if ($same) { Write-Host "object and listings identical to the MSVC-built compiler's" }
    return $same
}
# the two generations' objects and listings, unit by unit; fills $script:identical and $script:differ
function CompareGenerations([string[]]$suffixes) {
    $script:identical = 0; $script:differ = @()
    foreach ($f in $units) {
        $n = $f.BaseName
        $bad = @(); foreach ($suffix in $suffixes) { if (-not (SameBytes "$from1\$n$suffix" "$from2\$n$suffix")) { $bad += $suffix } }
        if ($bad.Count -eq 0) { $script:identical++ } else { $script:differ += "$n ($($bad -join ', '))" }
    }
    $script:libBad = @(); foreach ($suffix in $suffixes) { if (-not (SameBytes "$lib1\iostream$suffix" "$lib2\iostream$suffix")) { $script:libBad += $suffix } }
    if ($script:libBad.Count -ne 0) { $script:differ += "libcpp\iostream ($($script:libBad -join ', '))" }
    $script:rtBad = @(); foreach ($suffix in $suffixes) { if (-not (SameBytes "$lib1\cppparse_rt$suffix" "$lib2\cppparse_rt$suffix")) { $script:rtBad += $suffix } }
    if ($script:rtBad.Count -ne 0) { $script:differ += "runtime\cppparse_rt ($($script:rtBad -join ', '))" }
}
# under GRIDE's link: the two exes byte for byte, and from generation one's map the object that supplies operator new
function CompareExes {
    $a = [System.IO.File]::ReadAllBytes("$Sandbox\$exe1"); $b = [System.IO.File]::ReadAllBytes("$Sandbox\$exe2")
    $n = 0; $first = -1
    for ($i = 0; $i -lt [Math]::Min($a.Length, $b.Length); $i++) { if ($a[$i] -ne $b[$i]) { $n++; if ($first -lt 0) { $first = $i } } }
    if ($a.Length -eq $b.Length -and $n -eq 0) {
        Write-Host ("GRIDE's link: cppparse-self.exe and cppparse-self2.exe are byte-identical ({0:N0} bytes, SHA-256 {1})" -f $a.Length, (Get-FileHash "$Sandbox\$exe1" -Algorithm SHA256).Hash.ToLower())
        return $true
    }
    Write-Host ("GRIDE's link: the exes DIFFER: {0:N0} and {1:N0} bytes, {2} byte(s) differ, the first at offset 0x{3:X}" -f $a.Length, $b.Length, $n, $first)
    return $false
}
function ReportOperatorNew {
    $map = [IO.Path]::ChangeExtension("$Sandbox\$exe1", ".map")
    $who = ""
    foreach ($l in (Get-Content $map -ErrorAction SilentlyContinue)) { if ($l -match '^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+\?\?2@YAPEAX_K@Z\s+[0-9a-f]{16}\s+(?:f\s+)?(?:i\s+)?(\S+)\s*$') { $who = $Matches[1]; break } }
    if ($who -eq "iostream6.obj") { Write-Host "operator new of the GRIDE-linked compiler: libcpp's (iostream6.obj, by the map)" }
    elseif ($who -eq "") { Write-Host "operator new of the GRIDE-linked compiler: not found in $map"; return $false }
    else { Write-Host "not under GRIDE's link: libcpp's replaceable operator new; the map gives ??2@YAPEAX_K@Z to $who (GRIDE's link searches the libraries for a weak external of the alias kind, Microsoft's does not; request G7 of docs\design-selfhost.md)" }
    return $true
}

if ($Relink) {
    # the objects of both generations are the default run's; nothing is compiled but CheckSample's unit
    foreach ($d in @($from1, $from2)) {
        $have = if (Test-Path "$d\selfhost-level.txt") { (Get-Content "$d\selfhost-level.txt" -TotalCount 1).Trim() } else { "(no selfhost-level.txt)" }
        if ($have -ne $stamp) { Write-Host "relink FAILED: $d was compiled as '$have', not as '$stamp': run selfhost.ps1 -Lvl $Lvl first"; exit 1 }
    }
    CompareGenerations @("6.obj")
    Write-Host "the two generations' objects, as selfhost.ps1 -Lvl $Lvl left them, identical: $identical of $($units.Count)$(if ($libBad.Count -eq 0) { ', and libcpp\iostream' })$(if ($rtBad.Count -eq 0) { ', and the runtime' })"
    foreach ($d in $differ) { Write-Host "  differs: $d" }
    if ($differ.Count -ne 0) { Write-Host "relink FAILED: generation two's objects differ from generation one's"; exit 1 }
    New-Item -ItemType Directory -Force "$Sandbox\$host2", "$Sandbox\$host1\ref" | Out-Null
    if (-not (LinkOwn $from1 $lib1 $exe1)) { Write-Host "link failed"; exit 1 }
    if (-not (LinkOwn $from2 $lib2 $exe2)) { Write-Host "link of cppparse-self2.exe failed"; exit 1 }
    & $Exe --compile6 -O0 --obj --msvc-mangling -q -I ..\include -o "$host1\ref" -n self-opt "..\examples\sample-opt\opt.cpp" | Out-Null
    $sampleOk = (CheckSample $exe1 $host1) -and (CheckSample $exe2 $host2)
    Write-Host "=== summary ==="
    $exesOk = CompareExes
    $newOk = ReportOperatorNew
    Write-Host ("relink with GRIDE's link: {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
    if (-not $sampleOk -or -not $exesOk -or -not $newOk) { Write-Host "relink FAILED"; exit 1 }
    Write-Host "relink: both generations linked by GRIDE's link are one and the same exe"
    exit 0
}

if (-not $SkipCompile) { $null = CompileOwn $Exe $obj1 $host1 }
if (-not (LinkOwn $obj1 $host1 $exe1)) { Write-Host "link failed"; exit 1 }
New-Item -ItemType Directory -Force "$Sandbox\$host1\ref" | Out-Null
& $Exe --compile6 -O0 --obj --msvc-mangling -q -I ..\include -o "$host1\ref" -n self-opt "..\examples\sample-opt\opt.cpp" | Out-Null
if (-not (CheckSample $exe1 $host1)) { exit 1 }
Write-Host ("stage 1 (MSVC-built compiler builds and checks cppparse-self.exe): {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
if ($SkipBootstrap) { exit 0 }

# generation two: cppparse-self.exe compiles the same sources. Its objects must equal the MSVC-built
# compiler's bit for bit, which also makes cppparse-self2.exe the same program as cppparse-self.exe
Write-Host "=== bootstrap: cppparse-self.exe compiles the own sources to $obj2 ==="
$stage2 = Get-Date
New-Item -ItemType Directory -Force "$Sandbox\$obj2", "$Sandbox\$host2" | Out-Null
$failed = CompileOwn $exe1 $obj2 $host2
if ($failed -ne 0) { exit 1 }
if (-not (LinkOwn $obj2 $host2 $exe2)) { Write-Host "link of cppparse-self2.exe failed"; exit 1 }
CompareGenerations @("6.obj", "6.asm")
$sampleOk = CheckSample $exe2 $host2
Write-Host ("stage 2 (cppparse-self.exe builds and checks cppparse-self2.exe): {0:N0} s" -f ((Get-Date) - $stage2).TotalSeconds)
Write-Host "=== summary ==="
Write-Host "own units the self-built compiler compiles identically to the MSVC-built one (object and listing): $identical of $($units.Count)"
if ($libBad.Count -eq 0) { Write-Host "libcpp\iostream: identical" }
if ($rtBad.Count -eq 0) { Write-Host "runtime\cppparse_rt (object and listing, compiled by each generation): identical" }
foreach ($d in $differ) { Write-Host "  differs: $d" }
$exesOk = $true
if ($gride) { $exesOk = CompareExes; if (-not (ReportOperatorNew)) { $exesOk = $false } }
if ($differ.Count -ne 0 -or -not $sampleOk) { Write-Host "bootstrap FAILED: generation two differs from generation one"; exit 1 }
if (-not $exesOk) { Write-Host "bootstrap FAILED: the objects are identical, the exes GRIDE's link made of them are not"; exit 1 }
Write-Host "bootstrap: generation two is identical to generation one"
exit 0
