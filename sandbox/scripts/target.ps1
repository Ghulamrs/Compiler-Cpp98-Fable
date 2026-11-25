# target.ps1 - what the Windows x64 target adds to the language (row P1, docs\design-selfhost.md 4.1 to 4.3; the
# sources are examples\sample-target). Stage 1, thread-locals (__declspec(thread), cppparse/target TargetTls):
#   1. tls5.cpp, five accesses, at -O0 and -O2: the listing cppparse writes, assembled by ml64, gives --obj's code
#      bytes and relocations, function by function (ml64 is the second oracle beside cl)
#   2. the same object's .tls$ against cl's object of tls5.cpp: every symbol in it with its storage class and its
#      initial bytes, every relocation in it with its type and target (cl lays the variables out in its own order and
#      aligns the section by its contents; both are listed, not compared)
#   3. tlsthreads.cpp: eight threads counting in thread-locals at once (one initialised by an address constant) and
#      64 short-lived threads one after another print what cl's build prints, at -O0 and -O2, linked by Microsoft's
#      link and by GRIDE's on Microsoft's libraries, and by GRIDE's link on GRIDE's runtime
#   4. tlsdef.cpp and tlsuse.cpp: a cl object and a cppparse object share thread-locals, either way round, under
#      both linkers
#   5. tlsrefuse.cpp: each refusal of 4.1 is the error S164 with its own words, and no object
# Where GRIDE is not found the script prints "skipped: GRIDE not found" and exits with 77, which is not a pass.
# -Linker ms|gride (toolset.ps1): the linker of cl's reference programs and of section 9's builds; the links of
# sections 3, 4, 6 and 8 name their linkers (both, and GRIDE's runtime) whatever it says.
# Usage: powershell -ExecutionPolicy Bypass -File cpp\sandbox\scripts\target.ps1 [-Compiler <cppparse.exe>] [-Linker ms|gride]
param([string]$Compiler = "", [string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Show-Gride
if ($GrideRoot -eq "") { Write-Host "skipped: GRIDE not found"; exit $GrideSkipCode }
$o = "out\target"
New-Item -ItemType Directory -Force "$Sandbox\$o" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$ml64 = "$Msvc\bin\Hostx64\x64\ml64.exe"
$dumpbin = "$Msvc\bin\Hostx64\x64\dumpbin.exe"
$src = "..\examples\sample-target"
$failed = 0
function Fail([string]$what) { $script:failed++; Write-Host "  FAIL $what" }
function Ok([string]$what) { Write-Host "  ok   $what" }

# a COFF object read as dumpbin shows it: sections (name, flags, bytes, relocations) and symbols (name, value,
# section, type, storage class); a relocation names its symbol
function Read-Coff([string]$path) {
    $b = [IO.File]::ReadAllBytes((Resolve-Path $path).Path)
    $nsec = [BitConverter]::ToUInt16($b, 2); $symAt = [BitConverter]::ToUInt32($b, 8); $nsym = [BitConverter]::ToUInt32($b, 12)
    $strAt = $symAt + 18 * $nsym
    function Str([int]$at) { $e = $at; while ($b[$e] -ne 0) { $e++ }; return [Text.Encoding]::ASCII.GetString($b, $at, $e - $at) }
    $syms = @{}
    for ($i = 0; $i -lt $nsym; ) {
        $p = $symAt + 18 * $i
        if ([BitConverter]::ToUInt32($b, $p) -eq 0) { $name = Str ($strAt + [BitConverter]::ToUInt32($b, $p + 4)) } else { $name = [Text.Encoding]::ASCII.GetString($b, $p, 8).TrimEnd([char]0) }
        $syms[$i] = [pscustomobject]@{ Index = $i; Name = $name; Value = [BitConverter]::ToUInt32($b, $p + 8); Section = [BitConverter]::ToInt16($b, $p + 12); Type = [BitConverter]::ToUInt16($b, $p + 14); Class = $b[$p + 16]; Aux = $b[$p + 17] }
        $i += 1 + $b[$p + 17]
    }
    $secs = @()
    for ($s = 0; $s -lt $nsec; $s++) {
        $h = 20 + 40 * $s
        $name = [Text.Encoding]::ASCII.GetString($b, $h, 8).TrimEnd([char]0)
        if ($name.StartsWith("/")) { $name = Str ($strAt + [int]$name.Substring(1)) }
        $size = [BitConverter]::ToUInt32($b, $h + 16); $raw = [BitConverter]::ToUInt32($b, $h + 20); $relAt = [BitConverter]::ToUInt32($b, $h + 24); $nrel = [BitConverter]::ToUInt16($b, $h + 32)
        $bytes = New-Object byte[] $size
        if ($raw -ne 0) { [Array]::Copy($b, $raw, $bytes, 0, $size) }
        $rels = @()
        for ($r = 0; $r -lt $nrel; $r++) { $q = $relAt + 10 * $r; $rels += [pscustomobject]@{ Offset = [BitConverter]::ToUInt32($b, $q); Symbol = $syms[[int][BitConverter]::ToUInt32($b, $q + 4)].Name; Type = [BitConverter]::ToUInt16($b, $q + 8) } }
        $secs += [pscustomobject]@{ Number = $s + 1; Name = $name; Flags = [BitConverter]::ToUInt32($b, $h + 36); Bytes = $bytes; Relocs = $rels }
    }
    return [pscustomobject]@{ Sections = $secs; Symbols = @($syms.Values | Sort-Object Index) }
}
function Hex([byte[]]$bytes, [int]$at, [int]$n) { return (($bytes[$at..($at + $n - 1)] | ForEach-Object { $_.ToString("X2") }) -join " ") }
# the functions of an object as dumpbin disassembles them: each instruction's text and bytes, and the relocations
# that fall in it (its offset in the instruction, type, symbol). The int 3 between functions are left out (ml64 packs
# the functions, cppparse's writer aligns each to 16), a jump is compared by its distance (test 6 does the same)
function Get-Disasm([string]$obj) {
    $coff = Read-Coff $obj
    $rel = @{}
    foreach ($sec in $coff.Sections | Where-Object { $_.Name -like ".text*" }) { foreach ($r in $sec.Relocs) { $rel["$($sec.Number):$($r.Offset)"] = $r } }
    $secOf = @{}; foreach ($s in $coff.Symbols) { if ($s.Type -eq 0x20) { $secOf[$s.Name] = $s.Section } }
    $fns = [ordered]@{}; $cur = $null; $last = $null
    foreach ($l in (& $dumpbin /nologo /disasm $obj)) {
        if ($l -match '^(\S+) \(.*\):$' -or $l -match '^(\S+):$') { $cur = @(); $fns[$Matches[1]] = $cur; $curName = $Matches[1]; continue }
        if ($null -eq $curName) { continue }
        if ($l -match '^\s+([0-9A-F]{16}): ((?:[0-9A-F]{2} )+)\s*(.*)$') {
            $at = [Convert]::ToInt64($Matches[1], 16); $text = $Matches[3] -replace '\s+', ' '
            if ($text -match '^(j\w+) ([0-9A-F]{16})$') { $text = "$($Matches[1]) .{0:+#;-#;+0}" -f ([Convert]::ToInt64($Matches[2], 16) - $at) }
            $last = [pscustomobject]@{ At = $at; Text = $text; Bytes = $Matches[2].Trim() }
            $fns[$curName] += $last
        } elseif ($l -match '^\s{20}((?:[0-9A-F]{2} ?)+)$' -and $last) { $last.Bytes += " " + $Matches[1].Trim() }
    }
    $out = @{}
    foreach ($name in $fns.Keys) {
        $sec = $secOf[$name]
        $list = @($fns[$name] | Where-Object { $_.Text -ne "int 3" } | ForEach-Object {
            $n = @($_.Bytes -split " ").Count; $rs = @()
            for ($k = 0; $k -lt $n; $k++) { $r = $rel["${sec}:$($_.At + $k)"]; if ($r) { $rs += "+$k type $('{0:X}' -f $r.Type) $($r.Symbol)" } }
            [pscustomobject]@{ Text = $_.Text; Bytes = $_.Bytes; Relocs = ($rs -join ", ") } })
        $out[$name] = $list
    }
    return $out
}

Write-Host "=== 1. tls5.cpp: the listing assembled by ml64 against --obj (five accesses) ==="
foreach ($lvl in @(0, 2)) {
    $nm = "tls5L$lvl"
    # (--eh=table whatever CPPPARSE_EH says: ml64 stops at a listing of cl's model, which only cppparse's writer encodes)
    $msg = & $Exe --compile6 "-O$lvl" --eh=table --obj --msvc-mangling -q -o $o -n $nm "$src\tls5.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$o\${nm}6.obj")) { Fail "--compile6 -O$lvl of tls5.cpp"; Write-Host $msg; continue }
    $mlMsg = & $ml64 /nologo /c "/Fo$o\${nm}_ml64.obj" "$o\${nm}6.asm" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Fail "ml64 rejected ${nm}6.asm"; Write-Host $mlMsg; continue }
    $own = Get-Disasm "$o\${nm}6.obj"; $ml = Get-Disasm "$o\${nm}_ml64.obj"
    $insts = 0; $relocs = 0; $newForms = 0; $newBytes = 0; $secrel = 0; $bad = @()
    foreach ($f in $ml.Keys) {
        if (-not $own.ContainsKey($f)) { $bad += "$f is not in --obj's object"; continue }
        $a = $ml[$f]; $c = $own[$f]
        if ($a.Count -ne $c.Count) { $bad += "$f has $($a.Count) instructions from ml64, $($c.Count) in --obj's" }
        for ($k = 0; $k -lt [Math]::Min($a.Count, $c.Count); $k++) {
            if ($a[$k].Text -ne $c[$k].Text) { $bad += "$f [$k]: ml64 '$($a[$k].Text)', own '$($c[$k].Text)'" }
            if ($a[$k].Relocs -ne $c[$k].Relocs) { $bad += "$f [$k] '$($a[$k].Text)' relocation: ml64 '$($a[$k].Relocs)', own '$($c[$k].Relocs)'" }
            # the new forms (a gs: operand, the load of _tls_index, the section-relative immediate): byte for byte
            if ($a[$k].Text -match 'gs:\[|\[_tls_index\]' -or $a[$k].Relocs -match 'type B ') {
                $newForms++; $newBytes += @($a[$k].Bytes -split " ").Count
                if ($a[$k].Bytes -ne $c[$k].Bytes) { $bad += "$f [$k] '$($a[$k].Text)': ml64 $($a[$k].Bytes), own $($c[$k].Bytes)" }
            }
            if ($a[$k].Relocs) { $relocs += @($a[$k].Relocs -split ", ").Count }
            if ($a[$k].Relocs -match 'type B ') { $secrel++ }
        }
        $insts += $a.Count
    }
    if ($bad.Count -eq 0 -and $ml.Count -ge 6 -and $secrel -ge 5) { Ok "-O${lvl}: $($ml.Count) functions, $insts instructions and $relocs relocations as ml64 assembles the listing ($secrel SECREL); the $newForms instructions of the new forms byte for byte ($newBytes bytes)" }
    else { Fail "-O${lvl}: the listing assembled by ml64 differs from --obj ($($ml.Count) functions, $secrel SECREL)"; $bad | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" } }
}
Write-Host "=== 2. tls5.cpp: .tls`$, its symbols and relocations against cl's object (dumpbin's fields) ==="
& $cl /nologo /c /O2 /GS- "/Fo$o\tls5_cl.obj" "$src\tls5.cpp" | Out-Null
if (-not (Test-Path "$o\tls5_cl.obj")) { Fail "cl could not compile tls5.cpp" }
else {
    foreach ($lvl in @(0, 2)) {
        $mine = Read-Coff "$o\tls5L${lvl}6.obj"; $theirs = Read-Coff "$o\tls5_cl.obj"
        $view = @{}
        foreach ($who in @(@("cl", $theirs), @("own", $mine))) {
            $c = $who[1]; $sec = @($c.Sections | Where-Object { $_.Name -eq ".tls`$" })
            if ($sec.Count -ne 1) { $view[$who[0]] = $null; continue }
            $sec = $sec[0]
            $syms = @($c.Symbols | Where-Object { $_.Section -eq $sec.Number -and $_.Class -ne 3 -or ($_.Section -eq $sec.Number -and $_.Class -eq 3 -and $_.Aux -eq 0) } | Sort-Object Value)
            $entries = @{}
            for ($k = 0; $k -lt $syms.Count; $k++) {
                $end = if ($k + 1 -lt $syms.Count) { [int]$syms[$k + 1].Value } else { $sec.Bytes.Length }
                $entries[$syms[$k].Name] = [pscustomobject]@{ Class = $syms[$k].Class; Start = [int]$syms[$k].Value; Length = $end - [int]$syms[$k].Value }
            }
            $rel = @{}; foreach ($r in $sec.Relocs) { foreach ($n in $entries.Keys) { $e = $entries[$n]; if ($r.Offset -ge $e.Start -and $r.Offset -lt $e.Start + $e.Length) { $rel["$n+$($r.Offset - $e.Start)"] = "type $('{0:X}' -f $r.Type) $($r.Symbol)" } } }
            $view[$who[0]] = [pscustomobject]@{ Section = $sec; Entries = $entries; Relocs = $rel; Undefined = @($c.Symbols | Where-Object { $_.Section -eq 0 -and $_.Class -eq 2 } | ForEach-Object { $_.Name }) }
        }
        if (-not $view.cl -or -not $view.own) { Fail "-O${lvl}: no .tls`$ section (cl $([bool]$view.cl), cppparse $([bool]$view.own))"; continue }
        $bad = @()
        foreach ($n in $view.cl.Entries.Keys) {
            if (-not $view.own.Entries.ContainsKey($n)) { $bad += "$n is not in cppparse's .tls`$"; continue }
            $a = $view.cl.Entries[$n]; $c = $view.own.Entries[$n]
            if ($a.Class -ne $c.Class) { $bad += "$n storage class cl $($a.Class), cppparse $($c.Class)" }
            $len = [Math]::Min($a.Length, $c.Length)
            $x = Hex $view.cl.Section.Bytes $a.Start $len; $y = Hex $view.own.Section.Bytes $c.Start $len
            if ($x -ne $y) { $bad += "$n initial bytes cl [$x] cppparse [$y]" }
        }
        foreach ($n in $view.own.Entries.Keys) { if (-not $view.cl.Entries.ContainsKey($n)) { $bad += "$n is in cppparse's .tls`$ only" } }
        $ra = ($view.cl.Relocs.Keys | Sort-Object | ForEach-Object { "$_ $($view.cl.Relocs[$_])" }) -join "; "; $rb = ($view.own.Relocs.Keys | Sort-Object | ForEach-Object { "$_ $($view.own.Relocs[$_])" }) -join "; "
        if ($ra -ne $rb) { $bad += "relocations cl [$ra] cppparse [$rb]" }
        if (-not ($view.own.Undefined -contains "_tls_index")) { $bad += "cppparse's object does not name _tls_index" }
        foreach ($n in @("_tls_index", "_tls_used")) { if (@($mine.Symbols | Where-Object { $_.Name -eq $n -and $_.Section -gt 0 }).Count) { $bad += "cppparse's object defines $n" } }
        if ($bad.Count -eq 0) { Ok ("-O${lvl}: .tls`$ holds cl's {0} symbols ({1}) with their storage classes and initial bytes, and cl's relocation ({2}); _tls_index undefined" -f $view.cl.Entries.Count, (($view.cl.Entries.Keys | Sort-Object) -join ", "), $ra) }
        else { Fail "-O${lvl}: .tls`$ differs from cl's"; $bad | ForEach-Object { Write-Host "    $_" } }
        if ($lvl -eq 2) {
            Write-Host ("  (the layouts, not compared: cl's .tls`$ {0} bytes, flags {1:X8}, {2}; cppparse's {3} bytes, flags {4:X8}, {5})" -f $view.cl.Section.Bytes.Length, $view.cl.Section.Flags, (($view.cl.Entries.Keys | Sort-Object { $view.cl.Entries[$_].Start } | ForEach-Object { "$_ at $($view.cl.Entries[$_].Start)" }) -join ", "), $view.own.Section.Bytes.Length, $view.own.Section.Flags, (($view.own.Entries.Keys | Sort-Object { $view.own.Entries[$_].Start } | ForEach-Object { "$_ at $($view.own.Entries[$_].Start)" }) -join ", "))
            Write-Host "  dumpbin's view of cppparse's .tls`$ (-O2):"
            & $dumpbin /nologo /section:.tls`$ /rawdata:1 /relocations "$o\tls5L26.obj" | Select-String '^\s+[0-9A-F]{8}[: ]' | ForEach-Object { Write-Host "    $($_.Line.TrimEnd())" }
        }
    }
}

Write-Host "=== 3. tlsthreads.cpp: 8 threads at once, 64 one after another, against cl's build ==="
$grt = @("$GrideLib\grtstart.lib", "$GrideLib\grtcxx.lib", "$GrideLib\grtc.lib", "$GrideLib\grtwin.lib", "$GrideLib\kernel32.lib")
$msLibs = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
function Run-Exe([string]$exe) { $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"; $t = & ".\$exe" 2>&1 | Out-String; $c = $LASTEXITCODE; $ErrorActionPreference = $eap; return @($t, $c) }
# Microsoft's link and GRIDE's on Microsoft's libraries, GRIDE's link on GRIDE's runtime
function Link-Three([string[]]$objs, [string]$stem) {
    $r = @()
    foreach ($how in @("ms", "gride", "grt")) {
        $exe = "$o\$stem-$how.exe"
        if (Test-Path $exe) { Remove-Item $exe }
        if ($how -eq "ms") { $m = @(& $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe" @objs @msLibs 2>&1 | ForEach-Object { "$_" }) }
        elseif ($how -eq "gride") { $m = @(& $GrideLink /nologo /SUBSYSTEM:CONSOLE /timestamp:0 "/OUT:$exe" @objs @msLibs 2>&1 | ForEach-Object { "$_" }) }
        else { $m = @(& $GrideLink /nologo /SUBSYSTEM:CONSOLE /timestamp:0 /NODEFAULTLIB "/OUT:$exe" @objs @grt 2>&1 | ForEach-Object { "$_" }) }
        $r += [pscustomobject]@{ How = $how; Exe = $exe; Linked = (Test-Path $exe); Message = (($m | Select-Object -First 2) -join " | ") }
    }
    return $r
}
$names = @{ ms = "Microsoft's link"; gride = "GRIDE's link"; grt = "GRIDE's link on GRIDE's runtime" }
$null = Build-Program @("/O2", "/Fo$o\threads_cl.obj", "$src\tlsthreads.cpp") @("$o\threads_cl.obj") "$o\threads_cl.exe"
$want, $wantCode = Run-Exe "$o\threads_cl.exe"
if ($wantCode -ne 0 -or $want -notmatch "64 of 64") { Fail "cl's build of tlsthreads.cpp (exit $wantCode)"; Write-Host $want }
else {
    Write-Host ("  cl's build: " + (($want -split "`r?`n" | Where-Object { $_ -match "short-lived|^main" }) -join " / "))
    foreach ($lvl in @(0, 2)) {
        $nm = "threadsL$lvl"
        $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n $nm "$src\tlsthreads.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Fail "--compile6 -O$lvl of tlsthreads.cpp"; Write-Host $msg; continue }
        foreach ($r in (Link-Three @("$o\${nm}6.obj") $nm)) {
            if (-not $r.Linked) { Fail "-O${lvl}, $($names[$r.How]): no exe ($($r.Message))"; continue }
            $got, $code = Run-Exe $r.Exe
            if ($code -eq 0 -and $got -eq $want) { Ok "-O${lvl}, $($names[$r.How]): prints what cl's build prints ($(@($got -split "`n" | Where-Object { $_.Trim() }).Count) lines)" }
            else { Fail "-O${lvl}, $($names[$r.How]): exit $code"; Write-Host $got }
        }
    }
}

Write-Host "=== 4. tlsdef.cpp / tlsuse.cpp: a cl object and a cppparse object share thread-locals ==="
& $cl /nologo /c /O2 "/Fo$o\def_cl.obj" "$src\tlsdef.cpp" | Out-Null
& $cl /nologo /c /O2 "/Fo$o\use_cl.obj" "$src\tlsuse.cpp" | Out-Null
foreach ($p in @("def", "use")) {
    $msg = & $Exe --compile6 -O2 --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n "${p}_pp" "$src\tls$p.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Fail "--compile6 of tls$p.cpp"; Write-Host $msg }
}
$link2 = { param($objs, $exe, $gride) if (Test-Path $exe) { Remove-Item $exe }; if ($gride) { & $GrideLink /nologo /SUBSYSTEM:CONSOLE /timestamp:0 "/OUT:$exe" @objs @msLibs 2>&1 | Out-Null } else { & $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe" @objs @msLibs 2>&1 | Out-Null } }
& $link2 @("$o\use_cl.obj", "$o\def_cl.obj") "$o\share_cl.exe" $false
$want, $wantCode = Run-Exe "$o\share_cl.exe"
if ($wantCode -ne 0 -or $want -notmatch "first 7") { Fail "cl's build of tlsuse.cpp + tlsdef.cpp (exit $wantCode)" }
else {
    foreach ($pair in @(@("cl's tlsdef, cppparse's tlsuse", "$o\use_pp6.obj", "$o\def_cl.obj"), @("cppparse's tlsdef, cl's tlsuse", "$o\use_cl.obj", "$o\def_pp6.obj"))) {
        foreach ($g in @($false, $true)) {
            $shareExe = "$o\share-$(if ($g) { 'gride' } else { 'ms' }).exe"   # (not $exe: PowerShell's names ignore case, $Exe is the compiler)
            & $link2 @($pair[1], $pair[2]) $shareExe $g
            if (-not (Test-Path $shareExe)) { Fail "$($pair[0]), $(if ($g) { "GRIDE's link" } else { "Microsoft's link" }): no exe"; continue }
            $got, $code = Run-Exe $shareExe
            if ($code -eq 0 -and $got -eq $want) { Ok "$($pair[0]), $(if ($g) { "GRIDE's link" } else { "Microsoft's link" }): what cl's objects print" }
            else { Fail "$($pair[0]), $(if ($g) { "GRIDE's link" } else { "Microsoft's link" }): exit $code"; Write-Host $got }
        }
    }
}

Write-Host "=== 5. tlsrefuse.cpp: the refusals of 4.1, each the error S164 and no object ==="
$refusals = @(
    @(1, "a static local", "variable of block scope"), @(2, "an automatic local", "variable of block scope"),
    @(3, "a static data member", "data member of a class"), @(4, "a function", "applies to data only"),
    @(5, "a type and no variable", "on a type"), @(6, "a parameter", "on a parameter"),
    @(7, "a redeclaration that adds it", "redeclared with '__declspec\(thread\)'"), @(8, "a redeclaration that drops it", "redeclared without"),
    @(9, "a constructor", "constructor or destructor"), @(10, "a destructor", "constructor or destructor"),
    @(11, "an initialiser that needs code", "constant initialiser"), @(12, "alignment 32", "16 bytes at most, .* asks for 32"),
    @(13, "its address initialising a static", "address of the thread-local 't13' is not a constant"), @(14, "its address initialising a thread-local", "address of the thread-local 't14' is not a constant"),
    @(15, "a typedef", "on a typedef"), @(16, "a member of a class", "data member of a class"))
$refused = 0
foreach ($c in $refusals) {
    $n = $c[0]
    if (Test-Path "$o\refuse${n}6.obj") { Remove-Item "$o\refuse${n}6.obj" }
    $msg = & $Exe --compile6 -O0 --obj --msvc-mangling -q -o $o -n "refuse$n" -D "CASE=$n" "$src\tlsrefuse.cpp" 2>&1 | Out-String
    $line = @($msg -split "`r?`n" | Where-Object { $_ -match "error S164" })
    if ($LASTEXITCODE -ne 0 -and $line.Count -ge 1 -and $line[0] -match $c[2] -and -not (Test-Path "$o\refuse${n}6.obj")) { $refused++ }
    else { Fail "case ${n}, $($c[1]): exit $LASTEXITCODE, wanted S164 '$($c[2])'"; Write-Host $msg }
}
if ($refused -eq $refusals.Count) { Ok "$refused of $($refusals.Count) refusals: S164 with their own words, no object" }
$msg = & $Exe --compile6 -O0 --obj --msvc-mangling -q -o $o -n "refuse0" -D "CASE=0" "$src\tlsrefuse.cpp" 2>&1 | Out-String
if ($LASTEXITCODE -eq 0 -and (Test-Path "$o\refuse06.obj")) { Ok "the same unit without a case compiles" } else { Fail "tlsrefuse.cpp without a case: exit $LASTEXITCODE"; Write-Host $msg }

Write-Host "=== 6. layout.cpp: the unwind structures of platform\windows.h against the SDK's (4.3) ==="
$null = Build-Program @("/O2", "/Fo$o\layout_cl.obj", "$src\layout.cpp") @("$o\layout_cl.obj") "$o\layout_cl.exe"
$want, $wantCode = Run-Exe "$o\layout_cl.exe"
$msg = & $Exe --compile6 -O0 --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n layout "$src\layout.cpp" 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { Fail "--compile6 of layout.cpp"; Write-Host $msg }
elseif (@(Link-Three @("$o\layout6.obj") "layout")[0].Linked) {
    $got, $code = Run-Exe "$o\layout-ms.exe"
    $lines = @($want -split "`r?`n" | Where-Object { $_.Trim() }).Count
    if ($wantCode -eq 0 -and $got -eq $want -and $lines -ge 20) { Ok "$lines sizes and offsets through cppparse with platform\ equal cl's with the SDK (sizeof(CONTEXT) $(if ($want -match 'sizeof\(CONTEXT\)\s+(\d+)') { $Matches[1] }))" }
    else { Fail "layout differs"; Write-Host "cl:`n$want`ncppparse:`n$got" }
} else { Fail "layout.cpp did not link" }

Write-Host "=== 7. the intrinsics (4.2): atomics.cpp, retaddr.cpp, the refusals ==="
foreach ($prog in @("atomics", "retaddr")) {
    $null = Build-Program @("/O2", "/Fo$o\${prog}_cl.obj", "$src\$prog.cpp") @("$o\${prog}_cl.obj") "$o\${prog}_cl.exe"
    $want, $wantCode = Run-Exe "$o\${prog}_cl.exe"
    if ($wantCode -ne 0 -or $want -match "NO") { Fail "cl's build of $prog.cpp (exit $wantCode)"; Write-Host $want; continue }
    foreach ($lvl in @(0, 2)) {
        $nm = "${prog}L$lvl"
        $msg = & $Exe --compile6 "-O$lvl" --eh=table --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n $nm "$src\$prog.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Fail "--compile6 -O$lvl of $prog.cpp"; Write-Host $msg; continue }
        $asm = Get-Content "$o\${nm}6.asm" -Raw
        $calls = [regex]::Matches($asm, '(?m)^\s+call\s+_(Interlocked\w+|AddressOfReturnAddress)').Count
        foreach ($r in (Link-Three @("$o\${nm}6.obj") $nm)) {
            if ($r.How -eq "gride") { continue }
            if ($r.How -eq "grt" -and $prog -eq "retaddr") { if ($lvl -eq 0) { Write-Host "  (retaddr.cpp is not run on GRIDE's runtime: its kernel32.lib lacks RtlCaptureContext, request G2; stage 4)" }; continue }
            $got, $code = Run-Exe $r.Exe
            if ($r.Linked -and $code -eq $wantCode -and $got -eq $want -and $calls -eq 0) { Ok "$prog.cpp -O${lvl}, $($names[$r.How]): prints what cl's build prints, no call of an intrinsic ($(($got -split "`r?`n" | Select-Object -First 1)))" }
            else { Fail "$prog.cpp -O${lvl}, $($names[$r.How]): linked $($r.Linked), exit $code, $calls call(s) of an intrinsic"; Write-Host $got }
        }
        if ($prog -eq "atomics") {
            # the new encodings (lock xadd, lock cmpxchg, xchg with memory) against ml64's of the same listing, byte for byte
            $mlMsg = & $ml64 /nologo /c "/Fo$o\${nm}_ml64.obj" "$o\${nm}6.asm" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Fail "ml64 rejected ${nm}6.asm"; Write-Host $mlMsg }
            else {
                $own = Get-Disasm "$o\${nm}6.obj"; $ml = Get-Disasm "$o\${nm}_ml64.obj"; $forms = 0; $bad = @()
                foreach ($fn in $ml.Keys) {
                    if (-not $own.ContainsKey($fn) -or $own[$fn].Count -ne $ml[$fn].Count) { $bad += "$fn"; continue }
                    for ($k = 0; $k -lt $ml[$fn].Count; $k++) {
                        if ($ml[$fn][$k].Text -ne $own[$fn][$k].Text) { $bad += "$fn [$k] ml64 '$($ml[$fn][$k].Text)' own '$($own[$fn][$k].Text)'" }
                        if ($ml[$fn][$k].Text -match '^(lock |xchg )') { $forms++; if ($ml[$fn][$k].Bytes -ne $own[$fn][$k].Bytes) { $bad += "$fn '$($ml[$fn][$k].Text)': ml64 $($ml[$fn][$k].Bytes), own $($own[$fn][$k].Bytes)" } }
                    }
                }
                if ($bad.Count -eq 0 -and $forms -ge 6) { Ok "atomics.cpp -O${lvl}: the listing assembled by ml64 disassembles as --obj's, the $forms lock / xchg instructions byte for byte" }
                else { Fail "atomics.cpp -O${lvl}: ml64 against --obj ($forms lock / xchg forms)"; $bad | Select-Object -First 6 | ForEach-Object { Write-Host "    $_" } }
            }
        }        if ($prog -eq "retaddr") {
            # the function with the intrinsic keeps its frame: the frameless candidate leafSlot pushes rbp
            $leaf = [regex]::Match($asm, '(?s)\?leafSlot@@YAPEAXXZ PROC(.*?)ENDP').Groups[1].Value
            if ($leaf -match 'push rbp' -and $leaf -match 'lea rax, \[rbp\+8\]') { Ok "retaddr.cpp -O${lvl}: leafSlot keeps a frame and reads [rbp+8]" } else { Fail "retaddr.cpp -O${lvl}: leafSlot has no frame of its own" }
        }
    }
}
$checks = @(@(1, 1, "error S165: '_InterlockedIncrement': the address of an intrinsic"), @(2, 0, "only /alternatename:<name>=<name> is passed on"), @(3, 0, ""))
foreach ($c in $checks) {
    $n = $c[0]
    if (Test-Path "$o\irefuse${n}6.obj") { Remove-Item "$o\irefuse${n}6.obj" }
    $msg = & $Exe --compile6 -O2 --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n "irefuse$n" -D "CASE=$n" "$src\intrinsic-refuse.cpp" 2>&1 | Out-String
    $asm = if (Test-Path "$o\irefuse${n}6.asm") { Get-Content "$o\irefuse${n}6.asm" -Raw } else { "" }
    $ok = $LASTEXITCODE -eq $c[1] -and ($c[2] -eq "" -or $msg.Contains($c[2])) -and ((Test-Path "$o\irefuse${n}6.obj") -eq ($c[1] -eq 0))
    if ($n -eq 2) { $ok = $ok -and $asm -match ';@drectve /alternatename:cppparse_probe_a=cppparse_probe_b' -and $asm -notmatch 'include:something' }
    if ($n -eq 3) { $ok = $ok -and $asm -match 'call _InterlockedOr' -and $asm -match 'lock xadd' }
    $what = @("", "the address of an intrinsic: S165, no object", "a linker directive other than /alternatename: a warning, dropped; /alternatename passed on", "_InterlockedOr, not an intrinsic: an ordinary call beside an inline lock xadd")[$n]
    if ($ok) { Ok $what } else { Fail $what; Write-Host $msg }
}

Write-Host "=== 8. the runtime compiled by cppparse (4.5, 3.7): no message, no model mark, the three directives ==="
$rtObj = @{}
foreach ($lvl in @(0, 2)) {
    $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling --eh=table -q -I ..\include -I ..\platform -o $o -n "rtL$lvl" ..\runtime\cppparse_rt.cpp 2>&1 | Out-String
    $drectve = ""
    if (Test-Path "$o\rtL${lvl}6.obj") { $sec = @((Read-Coff "$o\rtL${lvl}6.obj").Sections | Where-Object { $_.Name -eq ".drectve" }); if ($sec.Count) { $drectve = [Text.Encoding]::ASCII.GetString($sec[0].Bytes) } }
    $alt = [regex]::Matches($drectve, '/alternatename:(__CxxFrameHandler4|__GSHandlerCheck_EH4?)=cppparse_no_handler').Count
    if ($LASTEXITCODE -eq 0 -and $msg.Trim() -eq "" -and $drectve -notmatch "FAILIFMISMATCH" -and $alt -eq 3) { Ok "-O${lvl}: compiled without a message, .drectve '$($drectve.Trim())'"; $rtObj[$lvl] = "$o\rtL${lvl}6.obj" }
    else { Fail "-O${lvl}: exit $LASTEXITCODE, messages '$($msg.Trim())', .drectve '$drectve'" }
}
& $cl /nologo /c /EHsc /W3 "/Fo$o\rt_cl.obj" ..\runtime\cppparse_rt.cpp | Out-Null
if ($rtObj[2]) {
    Write-Host "  probe n1 with cppparse's object of the runtime (alias.cpp; GRIDE's runtime with Microsoft's kernel32.lib for the three Rtl functions its own lacks, request G2):"
    $msg = & $Exe --compile6 -O2 --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n alias "$src\alias.cpp" 2>&1 | Out-String
    $bridge = "$SdkRoot\Lib\$SdkVer\um\x64\kernel32.lib"
    foreach ($how in @("ms/ms", "gride/ms", "ms/grt", "gride/grt")) {
        $exe2 = "$o\alias-$($how -replace '/', '-').exe"
        if (Test-Path $exe2) { Remove-Item $exe2 }
        $objs = @("$o\alias6.obj", $rtObj[2])
        $libs = if ($how.EndsWith("grt")) { @("/NODEFAULTLIB") + $grt + @($bridge) } else { $msLibs }
        if ($how.StartsWith("gride")) { $m = @(& $GrideLink /nologo /SUBSYSTEM:CONSOLE /timestamp:0 "/OUT:$exe2" @objs @libs 2>&1 | ForEach-Object { "$_" }) } else { $m = @(& $link /nologo /SUBSYSTEM:CONSOLE "/OUT:$exe2" @objs @libs 2>&1 | ForEach-Object { "$_" }) }
        if (-not (Test-Path $exe2)) { Fail "$how (linker/runtime): no exe ($(($m | Select-Object -First 2) -join ' | '))"; continue }
        $got, $code = Run-Exe $exe2
        $own = [regex]::Matches($got, "the runtime's cppparse_no_handler").Count
        $fh3 = $got -match "__CxxFrameHandler3: a library's"
        $want2 = if ($how.EndsWith("grt")) { 3 } else { 0 }
        if ($code -eq 0 -and $fh3 -and $own -eq $want2) { Ok "$how (linker/runtime): __CxxFrameHandler3 a library's, the other three $(if ($want2) { "the runtime's default" } else { "the library's" })" }
        else { Fail "$how (linker/runtime): exit $code"; Write-Host $got }
    }
}

Write-Host "=== 9. ehthreads.cpp: eight threads throwing and catching at once, three models, either object of the runtime ==="
$null = Build-Program @("/O2", "/EHsc", "/Fo$o\ehthreads_cl.obj", "$src\ehthreads.cpp") @("$o\ehthreads_cl.obj") "$o\ehthreads_cl.exe"
$want, $wantCode = Run-Exe "$o\ehthreads_cl.exe"
$runs = 0; $same = 0
foreach ($m in @("table", "sjlj", "cl")) {
    foreach ($lvl in @(0, 2)) {
        $nm = "ehthreads-$m$lvl"
        $msg = & $Exe --compile6 "-O$lvl" "--eh=$m" --obj --msvc-mangling -q -I ..\include -I ..\platform -o $o -n $nm "$src\ehthreads.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Fail "--compile6 -O$lvl --eh=$m of ehthreads.cpp"; Write-Host $msg; continue }
        foreach ($rt in @(@("cl's", "$o\rt_cl.obj"), @("cppparse's", $rtObj[2]))) {
            if (-not $rt[1]) { continue }
            $exe2 = "$o\$nm-$($rt[0] -replace "'s", '').exe"
            $null = Link-Program @("$o\${nm}6.obj", $rt[1]) $exe2
            $runs++
            $got, $code = Run-Exe $exe2
            if ((Test-Path $exe2) -and $code -eq 0 -and $got -eq $want) { $same++ } else { Fail "--eh=$m -O$lvl with $($rt[0]) object of the runtime: exit $code"; Write-Host $got }
        }
    }
}
if ($runs -eq 12 -and $same -eq $runs) { Ok "$same of $runs builds (table, sjlj, cl; -O0, -O2; cl's and cppparse's object of the runtime) print what cl's build prints" }
Write-Host "=== summary ==="
if ($failed -ne 0) { Write-Host "target: $failed check(s) failed"; exit 1 }
Write-Host "target: all checks passed"
exit 0
