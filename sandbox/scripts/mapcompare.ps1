# compilation exercise on the matrix sample (examples\sample-matrix, driver in examples\sample-matrix-driver):
#   1. cl /O2 builds the program and link /MAP writes its map (out\mapex\cl\matrix.map)
#   2. cppparse --compile6 -O<n> --obj --msvc-mangling compiles matrix.cpp and solver.cpp, writing an object map
#      for each (out\mapex\cpp\matrix6.map, solver6.map); the objects are linked with the cl-compiled driver and
#      link /MAP writes the linker's map of cppparse's code (out\mapex\cpp\linked.map)
#   3. A: cppparse's object maps are checked against link's map of the same objects: one constant shift between
#      .text offsets and link addresses, functions on 16-byte boundaries, link's gap = size + padding under 16,
#      link's f / f i flags equal to ours; and every function's map size against the bytes dumpbin /disasm
#      finds for it in the object (int 3 padding excluded)
#      B: the functions both compilers emit are listed side by side: address, exact code size and flags. cl's
#      functions are the ones its objects define (dumpbin /symbols of matrix.obj and solver.obj), not only those
#      in link's map: a function link /OPT:REF dropped (an inline method inlined everywhere, unreferenced code) is
#      listed as "removed by link". link /MAP has no size column, so cl's sizes are counted the same way, from
#      dumpbin /disasm of cl's objects; cl's map gap (padding included) is shown next to them
#      C: per class of the sample: every member function either compiler emits, both sizes, the ratio, whether
#      each linker kept it, and the class totals
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

# the code bytes of every function in an object, from dumpbin /disasm: symbol -> bytes. A label line is the
# decorated name (and the undecorated one in brackets); a long instruction continues on the next line; the int 3
# padding between functions is not counted.
function Get-CodeBytes([string[]]$objs) {
    $bytes = @{}
    foreach ($obj in $objs) {
        $cur = $null
        $int3 = 0   # int 3 bytes since the last other instruction: inside the function (after a call that does not
                    # return) when more code follows, padding otherwise; the first int 3 right after a call is the
                    # function's own (cl and cppparse end a call that does not return with one), also at its end
        $afterCall = $false
        foreach ($l in (& "$Msvc\bin\Hostx64\x64\dumpbin.exe" /nologo /disasm $obj)) {
            if ($l -match '^(\S+)( \(.*\))?:\s*$') { $cur = $Matches[1]; $int3 = 0; $afterCall = $false; if (-not $bytes.ContainsKey($cur)) { $bytes[$cur] = 0 } else { $cur = $null }; continue }
            if (-not $cur) { continue }
            if ($l -match '^\s+[0-9A-F]{16}: ([0-9A-F]{2}(?: [0-9A-F]{2})*)(?:\s{2,}(.*))?$') {
                $mn = ($Matches[2] -replace '\s+', ' ').Trim()
                if ($mn -eq 'int 3') { if ($afterCall) { $bytes[$cur] += 1; $afterCall = $false } else { $int3++ } }
                else { $bytes[$cur] += ($Matches[1] -split ' ').Count + $int3; $int3 = 0; $afterCall = $mn -like 'call *' }
            } elseif ($l -match '^\s+([0-9A-F]{2}(?: [0-9A-F]{2})*)\s*$') { $bytes[$cur] += ($Matches[1] -split ' ').Count }
        }
    }
    return $bytes
}

# the functions an object defines, from dumpbin /symbols: decorated name -> undecorated name (first definition)
function Get-DefinedFunctions([string[]]$objs) {
    $fns = @{}
    foreach ($obj in $objs) {
        foreach ($l in (& "$Msvc\bin\Hostx64\x64\dumpbin.exe" /nologo /symbols $obj)) {
            if ($l -match '^\w+ \w{8} SECT\w+\s+notype \(\)\s+(External|Static)\s+\| (\S+)(?: \((.*)\))?\s*$' -and -not $fns.ContainsKey($Matches[2])) {
                $fns[$Matches[2]] = $(if ($Matches[3]) { $Matches[3].Trim() } else { $Matches[2] })
            }
        }
    }
    return $fns
}
# the class of a member function, from its undecorated name: "public: int __cdecl mx::Matrix::rows(void)const "
# -> mx::Matrix; "" for a free function (no access specifier) or anything not parsed. The qualified name runs from
# after "__cdecl " to its parameter list; the class is everything before its last "::" outside template arguments
# (or before "::operator").
function Get-ClassOf([string]$und) {
    if ($und -notmatch '^(public|private|protected): ') { return "" }
    $i = $und.IndexOf('__cdecl '); if ($i -lt 0) { return "" }
    $name = $und.Substring($i + 8)
    $op = $name.IndexOf('::operator')
    if ($op -gt 0) { return $name.Substring(0, $op) }
    $p = $name.IndexOf('('); if ($p -gt 0) { $name = $name.Substring(0, $p) }
    $depth = 0; $last = -1
    for ($k = 0; $k -lt $name.Length - 1; $k++) {
        $c = $name[$k]
        if ($c -eq '<') { $depth++ } elseif ($c -eq '>') { $depth-- } elseif ($c -eq ':' -and $name[$k + 1] -eq ':' -and $depth -eq 0) { $last = $k; $k++ }
    }
    if ($last -le 0) { return "" }
    return $name.Substring(0, $last)
}

$linked = Read-LinkMap "$w\cpp\linked.map"; $linkedExt = Get-Extents $linked
$clMap = Read-LinkMap "$w\cl\matrix.map"; $clExt = Get-Extents $clMap
$clBytes = Get-CodeBytes @("$w\cl\matrix.obj", "$w\cl\solver.obj")   # an inline function: its first copy
$clDefined = Get-DefinedFunctions @("$w\cl\matrix.obj", "$w\cl\solver.obj")

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
    $allFn = @($ours | Where-Object Type -eq 'function')
    $ourBytes = Get-CodeBytes @("$w\cpp\${u}6.obj")
    $sizeOk = @($allFn | Where-Object { $ourBytes.ContainsKey($_.Sym) -and $ourBytes[$_.Sym] -eq $_.Size }).Count
    Write-Host "  $u`6.obj: $($ours.Count) symbols in our map, $($kept.Count) kept by the linker (the others: duplicate inline/template copies, unreferenced code)"
    Write-Host "    map size = dumpbin's code bytes:  $sizeOk of $($allFn.Count) functions"
    foreach ($s in @($allFn | Where-Object { -not $ourBytes.ContainsKey($_.Sym) -or $ourBytes[$_.Sym] -ne $_.Size } | Select-Object -First 3)) { Write-Host "      differs: $($s.Name): map $($s.Size), dumpbin $($ourBytes[$s.Sym])" }
    Write-Host ("    .text offsets vs link addresses:  {0} distinct shift(s){1}" -f $shift.Count, $(if ($shift.Count -eq 1) { ": every offset agrees" } else { "" }))
    Write-Host "    functions on 16-byte boundaries:  $aligned of $($plainFn.Count)"
    Write-Host "    link's gap = our size + padding:  $extOk of $($withExt.Count)"
    Write-Host "    flags equal to link's (f / f i):  $flagOk of $($kept.Count)"
    if ($shift.Count -ne 1 -or $aligned -ne $plainFn.Count -or $extOk -ne $withExt.Count -or $flagOk -ne $kept.Count -or $sizeOk -ne $allFn.Count) { Write-Host "    FAIL"; $failed++ }
}

# ---- 3B. the functions both compilers emit ---------------------------------------------------------------------
Write-Host ""
Write-Host "=== B. the same functions: cl /O2 (its objects, link /MAP) vs cppparse -O$Lvl (object maps)"
$ours = @(Read-OurMap "$w\cpp\matrix6.map") + @(Read-OurMap "$w\cpp\solver6.map")
$seen = @{}; $common = @(); $ourFns = @()
foreach ($s in $ours) {
    if ($s.Type -ne 'function' -or $seen.ContainsKey($s.Sym)) { continue }
    $seen[$s.Sym] = 1; $ourFns += $s
    if ($clDefined.ContainsKey($s.Sym)) { $common += $s }
}
$inMap = @($common | Where-Object { $clMap.ContainsKey($_.Sym) })
$dropped = @($common | Where-Object { -not $clMap.ContainsKey($_.Sym) })
Write-Host ("{0,-38} | {1,-13} {2,6} {3,6} {4,-3} | {5,-13} {6,6} {7,-3} | {8,5}" -f "function", "cl address", "bytes", "gap*", "fl", "our address", "bytes", "fl", "ratio")
Write-Host ('-' * 110)
foreach ($s in @($inMap | Sort-Object { $clMap[$_.Sym].Rva }) + @($dropped | Sort-Object Name)) {
    $n = $s.Name -replace 'mx::', ''; if ($n.Length -gt 38) { $n = $n.Substring(0, 35) + '...' }
    $cb = if ($clBytes.ContainsKey($s.Sym)) { $clBytes[$s.Sym] } else { 0 }
    $ratio = if ($cb) { "{0:0.0}" -f ($s.Size / $cb) } else { "" }
    $our = "| {0,-13} {1,6} {2,-3} | {3,5}" -f ("{0:x4}:{1:x8}" -f $s.Sec, $s.Off), $s.Size, $s.Flags, $ratio
    if ($clMap.ContainsKey($s.Sym)) {
        $c = $clMap[$s.Sym]
        $folded = @($clMap.GetEnumerator() | Where-Object { $_.Value.Rva -eq $c.Rva }).Count -gt 1
        $gap = if ($clExt.ContainsKey($s.Sym)) { $clExt[$s.Sym] } else { 0 }
        Write-Host ("{0,-38} | {1,-13} {2,6} {3,6} {4,-3} {5}" -f $n, ("{0}:{1:x8}" -f $c.Sec, $c.Off), $cb, $(if ($folded) { "$gap+" } else { $gap }), $c.Flags, $our)
    } else {
        Write-Host ("{0,-38} | {1,-13} {2,6} {3,-10} {4}" -f $n, "removed", $cb, "by link", $our)
    }
}
Write-Host "bytes: exact code size, counted in each compiler's object with dumpbin /disasm (int 3 padding excluded)"
Write-Host "* gap: what cl's link /MAP alone implies, the distance to the next symbol (padding included; '+': a body"
Write-Host "  link /OPT:ICF shares between identical functions)"
Write-Host "removed by link: inlined/unreferenced, defined in cl's object (a COMDAT) but not kept by link /OPT:REF"
$clFn = @($clMap.GetEnumerator() | Where-Object { $_.Value.Flags -like 'f*' -and $_.Value.Obj -match '^(matrix|solver)\.obj$' }).Count
$ourFn = $ourFns.Count
$flagsAgree = @($inMap | Where-Object { $_.Flags -eq $clMap[$_.Sym].Flags }).Count
$sumCl = ($common | ForEach-Object { $clBytes[$_.Sym] } | Measure-Object -Sum).Sum; $sumOur = ($common | Measure-Object Size -Sum).Sum
$sumGap = ($inMap | ForEach-Object { $clExt[$_.Sym] } | Measure-Object -Sum).Sum
$ratios = @($common | Where-Object { $clBytes[$_.Sym] } | ForEach-Object { $_.Size / $clBytes[$_.Sym] } | Sort-Object)
$inOurs = @($common | Where-Object { $linked.ContainsKey($_.Sym) })
Write-Host ""
Write-Host "functions: cl's objects $($clDefined.Count) defined (matrix.obj, solver.obj, library inlines included), cl's map $clFn of theirs,"
Write-Host "  cppparse's maps $ourFn distinct, in both: $($common.Count) ($($dropped.Count) of them removed by cl's link)"
Write-Host "flags agree on the functions in both and in cl's map: $flagsAgree of $($inMap.Count)"
Write-Host ("their code: cl {0} bytes, cppparse {1} bytes: {2:0.00} times cl's (cl's map gaps of the {3} kept add up to {4})" -f $sumCl, $sumOur, ($sumOur / $sumCl), $inMap.Count, $sumGap)
if ($ratios.Count) { Write-Host ("size ratio per function: smallest {0:0.0}, median {1:0.0}, largest {2:0.0}; cppparse smaller for {3} of {4}" -f $ratios[0], $ratios[[int][Math]::Floor($ratios.Count / 2)], $ratios[$ratios.Count - 1], @($ratios | Where-Object { $_ -lt 1 }).Count, $ratios.Count) }
Write-Host ("16-byte aligned: cl {0} of {1} kept; cppparse as linked {2} of {3} (the others were removed by the linker as unreferenced)" -f
            @($inMap | Where-Object { $clMap[$_.Sym].Rva % 16 -eq 0 }).Count, $inMap.Count, @($inOurs | Where-Object { $linked[$_.Sym].Rva % 16 -eq 0 }).Count, $inOurs.Count)

# ---- 3C. per class: the member functions either compiler emits ----------------------------------------------
Write-Host ""
Write-Host "=== C. per class of the sample: member functions emitted by cl /O2 or cppparse -O$Lvl"
$ourBySym = @{}; foreach ($s in $ourFns) { $ourBySym[$s.Sym] = $s }
$ourDefined = Get-DefinedFunctions @("$w\cpp\matrix6.obj", "$w\cpp\solver6.obj")
$members = @{}   # class -> list of symbols
foreach ($sym in @($clDefined.Keys) + @($ourBySym.Keys)) {
    $cls = if ($clDefined.ContainsKey($sym)) { Get-ClassOf $clDefined[$sym] } elseif ($ourDefined.ContainsKey($sym)) { Get-ClassOf $ourDefined[$sym] } else { "" }
    if ($cls -eq "" -or $cls -like 'std::*' -or $cls -match '(^|::)_') { continue }
    if (-not $members.ContainsKey($cls)) { $members[$cls] = @{} }
    $members[$cls][$sym] = 1
}
foreach ($cls in ($members.Keys | Sort-Object)) {
    Write-Host ""
    Write-Host "class $cls"
    Write-Host ("  {0,-44} {1,6} {2,-7} {3,6} {4,-7} {5,5}" -f "member function", "cl", "kept", "ours", "kept", "ratio")
    Write-Host ("  " + '-' * 80)
    $tc = 0; $to = 0; $nc = 0; $no = 0; $kc = 0; $ko = 0; $bothC = 0; $bothO = 0
    $rows = @()
    foreach ($sym in $members[$cls].Keys) {
        $name = if ($ourBySym.ContainsKey($sym)) { $ourBySym[$sym].Name } else { $u = $clDefined[$sym]; $i = $u.IndexOf("${cls}::"); if ($i -ge 0) { $u.Substring($i) } else { $u } }
        $rows += [pscustomobject]@{ Sym = $sym; Name = $name }
    }
    foreach ($r in ($rows | Sort-Object Name)) {
        $hasC = $clDefined.ContainsKey($r.Sym); $hasO = $ourBySym.ContainsKey($r.Sym)
        $cb = if ($hasC -and $clBytes.ContainsKey($r.Sym)) { $clBytes[$r.Sym] } else { 0 }
        $ob = if ($hasO) { $ourBySym[$r.Sym].Size } else { 0 }
        $keptC = if (-not $hasC) { "-" } elseif ($clMap.ContainsKey($r.Sym)) { "yes" } else { "removed" }
        $keptO = if (-not $hasO) { "-" } elseif ($linked.ContainsKey($r.Sym)) { "yes" } else { "removed" }
        if ($hasC) { $tc += $cb; $nc++; if ($keptC -eq "yes") { $kc++ } }
        if ($hasO) { $to += $ob; $no++; if ($keptO -eq "yes") { $ko++ } }
        if ($hasC -and $hasO) { $bothC += $cb; $bothO += $ob }
        $ratio = if ($hasC -and $hasO -and $cb) { "{0:0.0}" -f ($ob / $cb) } else { "" }
        $n = $r.Name -replace 'mx::', ''; if ($n.Length -gt 44) { $n = $n.Substring(0, 41) + '...' }
        Write-Host ("  {0,-44} {1,6} {2,-7} {3,6} {4,-7} {5,5}" -f $n, $(if ($hasC) { $cb } else { "-" }), $keptC, $(if ($hasO) { $ob } else { "-" }), $keptO, $ratio)
    }
    Write-Host ("  " + '-' * 80)
    Write-Host ("  {0,-44} {1,6} {2,-7} {3,6} {4,-7} {5,5}" -f "total ($nc cl, $no cppparse)", $tc, "$kc kept", $to, "$ko kept", $(if ($tc) { "{0:0.00}" -f ($to / $tc) } else { "" }))
    if ($bothC) { Write-Host ("  {0,-44} {1,6} {2,-7} {3,6} {4,-7} {5,5}" -f "in both compilers", $bothC, "", $bothO, "", ("{0:0.00}" -f ($bothO / $bothC))) }
}
Write-Host "kept: in the linked program's map (cl's link /MAP, link /MAP of cppparse's objects); removed: dropped by"
Write-Host "  link /OPT:REF (inlined everywhere or unreferenced); '-': not emitted by that compiler"

if ($failed -ne 0) { Write-Host "mapcompare: $failed check(s) FAILED"; exit 1 }
Write-Host "mapcompare: cppparse's maps agree with link /MAP"
exit 0
