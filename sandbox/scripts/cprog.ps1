# builds the multi-file C sample cpp\sample-c with cl as C (/TC) and as C++ (/TP), and with the three generations
# of cppparse: A (sandbox\cppparse.exe, built by cl), B (out\selfhost\cppparse-self.exe, built by A) and C
# (out\selfhost2\cppparse-self2.exe, built by B; see selfhost.ps1), each at -O0 and -O2. Every build runs in its
# own folder under out\cprog; its output must equal the cl C++ build's, and the objects of A, B and C must be
# identical byte for byte
. "$PSScriptRoot\toolset.ps1"
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$srcDir = (Resolve-Path "..\sample-c").Path
$units = @(Get-ChildItem "$srcDir\*.cpp" | ForEach-Object { $_.BaseName })
$out = "$Sandbox\out\cprog"
New-Item -ItemType Directory -Force $out | Out-Null
$libs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
& $cl /nologo /c /EHsc /W3 "/Fo$out\cppparse_rt.obj" "..\cppparse\runtime\cppparse_rt.cpp" | Out-Null
$failed = 0

function RunBuild([string]$name, [string]$program) {
    if (-not (Test-Path $program)) { return $null }
    Push-Location (Split-Path $program)
    $text = & $program 2>&1 | Out-String
    $rc = $LASTEXITCODE
    Pop-Location
    Set-Content -Encoding ascii "$out\$name.out" $text
    Write-Host ("{0,-14} exit {1}" -f $name, $rc)
    return $text
}

function SameBytes([string]$a, [string]$b) {
    if (-not (Test-Path $a) -or -not (Test-Path $b)) { return $false }
    return [System.Linq.Enumerable]::SequenceEqual([byte[]][System.IO.File]::ReadAllBytes($a), [byte[]][System.IO.File]::ReadAllBytes($b))
}

$results = [ordered]@{}
foreach ($mode in @(@("cl-c", "/TC"), @("cl-cpp", "/TP"))) {
    $dir = "$out\$($mode[0])"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $files = @(); foreach ($u in $units) { $files += "$srcDir\$u.cpp" }
    # "/Fo<dir>\\": a single backslash before the closing quote would escape the quote on a path with spaces;
    # _CRT_SECURE_NO_WARNINGS: the standard functions strcpy, sprintf, fopen... are what the sample is about
    $o = & $cl /nologo $mode[1] /W4 /O2 /D_CRT_SECURE_NO_WARNINGS "/Fo$dir\\" "/Fe$dir\cprog.exe" @files 2>&1 | Out-String
    if (-not (Test-Path "$dir\cprog.exe")) { Write-Host "$($mode[0]) build failed:"; Write-Host $o; $failed++; continue }
    if ($o -match "warning") { Write-Host "$($mode[0]) warnings:"; Write-Host $o }
    $results[$mode[0]] = RunBuild $mode[0] "$dir\cprog.exe"
}

$generations = [ordered]@{ "A" = $Exe; "B" = "$Sandbox\out\selfhost\cppparse-self.exe"; "C" = "$Sandbox\out\selfhost2\cppparse-self2.exe" }
foreach ($lvl in @("0", "2")) {
    foreach ($gen in $generations.Keys) {
        $compiler = $generations[$gen]
        $name = "$gen-O$lvl"
        if (-not (Test-Path $compiler)) { Write-Host "$name skipped: $compiler not built (run selfhost.ps1)"; $failed++; continue }
        $dir = "$out\$name"
        New-Item -ItemType Directory -Force $dir | Out-Null
        $objs = @(); $bad = $false
        foreach ($u in $units) {
            [System.IO.File]::Delete("$dir\${u}6.obj")
            $o = & $compiler --compile6 "-O$lvl" --obj --msvc-mangling -q "-I$srcDir" -I ..\cppparse\include -o $dir -n $u "$srcDir\$u.cpp" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$dir\${u}6.obj")) { Write-Host "$name failed on $u :"; Write-Host $o; $bad = $true }
            $objs += "$dir\${u}6.obj"
        }
        if ($bad) { $failed++; continue }
        [System.IO.File]::Delete("$dir\cprog.exe")
        $o = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$dir\cprog.exe" @objs "$out\cppparse_rt.obj" @libs 2>&1 | Out-String
        if (-not (Test-Path "$dir\cprog.exe")) { Write-Host "$name link failed:"; Write-Host $o; $failed++; continue }
        $results[$name] = RunBuild $name "$dir\cprog.exe"
    }
}

Write-Host "=== program output compared with the cl C++ build ==="
$reference = $results["cl-cpp"]
if ($null -eq $reference) { Write-Host "no reference output"; exit 1 }
$lines = ($reference -split "`r?`n").Count
foreach ($k in $results.Keys) {
    if ($k -eq "cl-cpp") { continue }
    if ($results[$k] -eq $reference) { Write-Host ("{0,-14} identical ({1} lines)" -f $k, $lines) }
    else {
        $failed++
        Write-Host ("{0,-14} DIFFERS" -f $k)
        Compare-Object ($reference -split "`r?`n") ($results[$k] -split "`r?`n") | Format-Table -AutoSize | Out-String | Write-Host
    }
}

Write-Host "=== objects of the three generations ==="
foreach ($lvl in @("0", "2")) {
    foreach ($u in $units) {
        $ab = SameBytes "$out\A-O$lvl\${u}6.obj" "$out\B-O$lvl\${u}6.obj"
        $bc = SameBytes "$out\B-O$lvl\${u}6.obj" "$out\C-O$lvl\${u}6.obj"
        if (-not ($ab -and $bc)) { $failed++ }
        Write-Host ("-O{0} {1,-10} A=B {2,-5}  B=C {3}" -f $lvl, $u, $ab, $bc)
    }
}
exit $failed
