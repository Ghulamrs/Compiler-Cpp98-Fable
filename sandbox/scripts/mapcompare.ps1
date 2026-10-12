# compilation exercise on the matrix sample (examples\sample-matrix, driver in examples\sample-matrix-driver):
#   1. cl /O2 builds the program and link /MAP writes its map (out\mapex\cl\matrix.map)
#   2. cppparse --compile6 -O<n> --obj --msvc-mangling compiles matrix.cpp and solver.cpp, writing an object map
#      for each (out\mapex\cpp\matrix6.map, solver6.map); the objects are linked with the cl-compiled driver and
#      link /MAP writes the linker's map of cppparse's code (out\mapex\cpp\linked.map)
#   3. A: cppparse's object maps are checked against link's map of the same objects: one constant shift between
#      .text offsets and link addresses, functions on 16-byte boundaries, link's gap = size + padding under 16,
#      link's f / f i flags equal to ours
#      B: the functions both compilers emit are listed side by side: address, size, flags
# Both programs must print ALL OK. Exit code 1 when a build fails or part A finds a disagreement.
# (mapcompare.py is a different tool: maps.ps1 uses it to compare the reference and self-built compilers' maps.)
param([string]$Lvl = "2", [string]$Compiler = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$w = "out\mapex"
New-Item -ItemType Directory -Force "$w\cl", "$w\cpp" | Out-Null
$src = @("..\examples\sample-matrix\matrix.cpp", "..\examples\sample-matrix\solver.cpp")
$drv = "..\examples\sample-matrix-driver\driver-matrix.cpp"
$libs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
$failed = 0

function Run([string]$exe) {
    if (-not (Test-Path $exe)) { return "not built" }
    $text = & $exe 2>&1 | Out-String
    return $(if ($LASTEXITCODE -eq 0 -and $text -match 'ALL OK') { "ALL OK" } else { "FAILED (exit $LASTEXITCODE)" })
}

# ---- 1. cl /O2 + link /MAP --------------------------------------------------------------------------------
foreach ($f in Get-ChildItem "$w\cl", "$w\cpp" -File -ErrorAction SilentlyContinue) { Remove-Item $f.FullName }
$out = & $cl /nologo /c /O2 /EHsc /I.. /I..\examples\sample-matrix "/Fo$w\cl\" @src $drv 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { Write-Host $out }
$null = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$w\cl\matrix.exe" "/MAP:$w\cl\matrix.map" "$w\cl\matrix.obj" "$w\cl\solver.obj" "$w\cl\driver-matrix.obj" @libs 2>&1
$r = Run "$w\cl\matrix.exe"; Write-Host "cl /O2 build:          $r"; if ($r -ne "ALL OK") { $failed++ }

# ---- 2. cppparse, linked with the cl-compiled driver + link /MAP --------------------------------------------
foreach ($s in $src) {
    $out = & $Exe --compile6 "-O$Lvl" --obj --msvc-mangling -q -I ..\include -I .. -I ..\examples\sample-matrix -o "$w\cpp" -n ([IO.Path]::GetFileNameWithoutExtension($s)) $s 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $out }
}
& $cl /nologo /c /EHsc /W3 "/Fo$w\cpp\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
$null = & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$w\cpp\matrix.exe" "/MAP:$w\cpp\linked.map" "$w\cl\driver-matrix.obj" "$w\cpp\matrix6.obj" "$w\cpp\solver6.obj" "$w\cpp\cppparse_rt.obj" @libs 2>&1
$r = Run "$w\cpp\matrix.exe"; Write-Host "cppparse -O$Lvl build:   $r"; if ($r -ne "ALL OK") { $failed++ }
if ($failed -ne 0 -or -not (Test-Path "$w\cl\matrix.map") -or -not (Test-Path "$w\cpp\linked.map")) { Write-Host "mapcompare: a build failed"; exit 1 }

# a cppparse object map: the address-ordered rows (Symbols by Name repeats them)
function Read-OurMap([string]$path) {
    $rows = @()
    foreach ($l in Get-Content $path) {
        if ($l -match '^ Symbols by Name') { break }
        if ($l -match '^ (?<sec>[0-9a-f]{4}):(?<off>[0-9a-f]{8})   (?<size>[0-9a-f]{8})H   (?<flags>.{3})    (?<type>\w+)\s+(?<sym>\S+)\s*(?<name>.*)$') {
            $rows += [pscustomobject]@{ Sec = [Convert]::ToInt32($Matches.sec, 16); Off = [Convert]::ToInt64($Matches.off, 16); Size = [Convert]::ToInt64($Matches.size, 16)
                                        Flags = $Matches.flags.Trim(); Type = $Matches.type; Sym = $Matches.sym; Name = $Matches.name.Trim() }
        }
    }
    return $rows
}
# a link /MAP: symbol -> section, offset, flat address, flags, object
function Read-LinkMap([string]$path) {
    $map = @{}
    foreach ($l in Get-Content $path) {
        if ($l -match '^ (\w{4}):(\w{8})\s+(\S+)\s+([0-9a-f]{16})\s+(f )?(i )?\s*(\S+)\s*$' -and -not $map.ContainsKey($Matches[3])) {
            $map[$Matches[3]] = [pscustomobject]@{ Sec = $Matches[1]; Off = [Convert]::ToInt64($Matches[2], 16); Rva = [Convert]::ToInt64($Matches[4], 16); Flags = ("$($Matches[5])$($Matches[6])").Trim(); Obj = $Matches[7] }
        }
    }
    return $map
}
# link /MAP has no size column: a code symbol's extent is the distance to the next address in .text (padding
# included). Symbols that share an address were folded into one body by link /OPT:ICF; they all get that extent.
function Get-Extents($map) {
    $code = @($map.GetEnumerator() | Where-Object { $_.Value.Sec -eq '0001' } | Group-Object { $_.Value.Rva } | Sort-Object { [int64]$_.Name })
    $ext = @{}
    for ($i = 0; $i + 1 -lt $code.Count; $i++) { foreach ($e in $code[$i].Group) { $ext[$e.Key] = [int64]$code[$i + 1].Name - [int64]$code[$i].Name } }
    return $ext
}

$linked = Read-LinkMap "$w\cpp\linked.map"; $linkedExt = Get-Extents $linked
$clMap = Read-LinkMap "$w\cl\matrix.map"; $clExt = Get-Extents $clMap

# ---- 3A. cppparse's object maps against link's map of the same objects ------------------------------------------
Write-Host ""
Write-Host "=== A. cppparse's object maps against link /MAP of the same objects"
foreach ($u in "matrix", "solver") {
    $ours = @(Read-OurMap "$w\cpp\${u}6.map")
    $kept = @($ours | Where-Object { $linked.ContainsKey($_.Sym) })
    $plainFn = @($ours | Where-Object { $_.Sec -eq 1 -and $_.Type -eq 'function' -and $linked.ContainsKey($_.Sym) })
    $shift = @{}; foreach ($s in $plainFn) { $shift[$linked[$s.Sym].Rva - $s.Off] = 1 }
    $aligned = @($plainFn | Where-Object { $linked[$_.Sym].Rva % 16 -eq 0 }).Count
    $withExt = @($plainFn | Where-Object { $linkedExt.ContainsKey($_.Sym) })
    $extOk = @($withExt | Where-Object { $linkedExt[$_.Sym] -ge $_.Size -and $linkedExt[$_.Sym] - $_.Size -lt 16 }).Count
    $flagOk = @($kept | Where-Object { $_.Flags -eq $linked[$_.Sym].Flags }).Count
    Write-Host "  $u`6.obj: $($ours.Count) symbols in our map, $($kept.Count) kept by the linker (the others: duplicate inline/template copies, unreferenced code)"
    Write-Host ("    .text offsets vs link addresses:  {0} distinct shift(s){1}" -f $shift.Count, $(if ($shift.Count -eq 1) { ": every offset agrees" } else { "" }))
    Write-Host "    functions on 16-byte boundaries:  $aligned of $($plainFn.Count)"
    Write-Host "    link's gap = our size + padding:  $extOk of $($withExt.Count)"
    Write-Host "    flags equal to link's (f / f i):  $flagOk of $($kept.Count)"
    if ($shift.Count -ne 1 -or $aligned -ne $plainFn.Count -or $extOk -ne $withExt.Count -or $flagOk -ne $kept.Count) { Write-Host "    FAIL"; $failed++ }
}

# ---- 3B. the functions both compilers emit ---------------------------------------------------------------------
Write-Host ""
Write-Host "=== B. the same functions: cl /O2 (link /MAP) vs cppparse -O$Lvl (object maps)"
$ours = @(Read-OurMap "$w\cpp\matrix6.map") + @(Read-OurMap "$w\cpp\solver6.map")
$seen = @{}; $common = @()
foreach ($s in $ours) {
    if ($s.Type -ne 'function' -or $seen.ContainsKey($s.Sym) -or -not $clMap.ContainsKey($s.Sym) -or $clMap[$s.Sym].Obj -notmatch '^(matrix|solver)\.obj$') { continue }
    $seen[$s.Sym] = 1; $common += $s
}
Write-Host ("{0,-38} | {1,-13} {2,7} {3,-3} | {4,-13} {5,6} {6,-3} {7,5}" -f "function", "cl address", "size*", "fl", "our address", "size", "fl", "ratio")
Write-Host ('-' * 104)
foreach ($s in ($common | Sort-Object { $clMap[$_.Sym].Rva })) {
    $c = $clMap[$s.Sym]
    $n = $s.Name -replace 'mx::', ''; if ($n.Length -gt 38) { $n = $n.Substring(0, 35) + '...' }
    $folded = @($clMap.GetEnumerator() | Where-Object { $_.Value.Rva -eq $c.Rva }).Count -gt 1
    $cs = if ($clExt.ContainsKey($s.Sym)) { $clExt[$s.Sym] } else { 0 }
    $ratio = if ($cs) { "{0:0.0}" -f ($s.Size / $cs) } else { "" }
    Write-Host ("{0,-38} | {1,-13} {2,7} {3,-3} | {4,-13} {5,6} {6,-3} {7,5}" -f $n, ("{0}:{1:x8}" -f $c.Sec, $c.Off), $(if ($folded) { "$cs+" } else { $cs }), $c.Flags,
                                                                                ("{0:x4}:{1:x8}" -f $s.Sec, $s.Off), $s.Size, $s.Flags, $ratio)
}
Write-Host "* link /MAP has no size column: cl's size is the distance to the next symbol (padding included);"
Write-Host "  '+' marks a body link /OPT:ICF shares between identical functions"
$clFn = @($clMap.GetEnumerator() | Where-Object { $_.Value.Flags -like 'f*' -and $_.Value.Obj -match '^(matrix|solver)\.obj$' }).Count
$ourFn = @($ours | Where-Object { $_.Type -eq 'function' } | ForEach-Object Sym | Sort-Object -Unique).Count
$flagsAgree = @($common | Where-Object { $_.Flags -eq $clMap[$_.Sym].Flags }).Count
$sumCl = ($common | ForEach-Object { $clExt[$_.Sym] } | Measure-Object -Sum).Sum; $sumOur = ($common | Measure-Object Size -Sum).Sum
$inOurs = @($common | Where-Object { $linked.ContainsKey($_.Sym) })
Write-Host ""
Write-Host "functions: cl's map $clFn (matrix.obj, solver.obj), cppparse's maps $ourFn distinct, in both: $($common.Count)"
Write-Host "flags agree on the functions in both: $flagsAgree of $($common.Count)"
Write-Host "their code: cl $sumCl bytes (padding included), cppparse $sumOur bytes (without)"
Write-Host ("16-byte aligned: cl {0} of {1}; cppparse as linked {2} of {3} (the others were removed by the linker as unreferenced)" -f
            @($common | Where-Object { $clMap[$_.Sym].Rva % 16 -eq 0 }).Count, $common.Count, @($inOurs | Where-Object { $linked[$_.Sym].Rva % 16 -eq 0 }).Count, $inOurs.Count)

if ($failed -ne 0) { Write-Host "mapcompare: $failed check(s) FAILED"; exit 1 }
Write-Host "mapcompare: cppparse's maps agree with link /MAP"
exit 0
