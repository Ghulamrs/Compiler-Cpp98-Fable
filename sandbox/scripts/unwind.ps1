# unwind.ps1 - reads the Windows x64 unwind tables of a COFF object: for every .pdata entry, the function it covers
# and the bytes of its UNWIND_INFO in .xdata (header and unwind codes; with a handler, its address field and data).
# Dot-source it, then:  Get-UnwindTable obj  ->  hashtable  function name -> "01 13 07 00 13 68 ..."
# Used by build-cppparse.ps1 to compare cppparse's tables with the ones ml64 builds from the same assembly text.

function Get-UnwindTable([string]$Path) {
    $b = [IO.File]::ReadAllBytes((Resolve-Path $Path).Path)
    $u16 = { param($o) [int]$b[$o] -bor ([int]$b[$o + 1] -shl 8) }
    $u32 = { param($o) [long]$b[$o] -bor ([long]$b[$o + 1] -shl 8) -bor ([long]$b[$o + 2] -shl 16) -bor ([long]$b[$o + 3] -shl 24) }
    $nsec = & $u16 2; $symPtr = & $u32 8; $nsym = & $u32 12; $opt = & $u16 16
    $strTab = $symPtr + 18 * $nsym
    $sections = @()
    for ($s = 0; $s -lt $nsec; $s++) {
        $h = 20 + $opt + 40 * $s
        $name = [Text.Encoding]::ASCII.GetString($b, $h, 8).TrimEnd([char]0)
        $sections += [pscustomobject]@{ Name = $name; Raw = (& $u32 ($h + 20)); Size = (& $u32 ($h + 16)); Rel = (& $u32 ($h + 24)); NRel = (& $u16 ($h + 32)) }
    }
    $symbols = @{}
    $i = 0
    while ($i -lt $nsym) {
        $o = $symPtr + 18 * $i
        if ((& $u32 $o) -eq 0) { $so = $strTab + (& $u32 ($o + 4)); $e = $so; while ($b[$e] -ne 0) { $e++ }; $name = [Text.Encoding]::ASCII.GetString($b, $so, $e - $so) }
        else { $name = [Text.Encoding]::ASCII.GetString($b, $o, 8).TrimEnd([char]0) }
        $sec = & $u16 ($o + 12); if ($sec -ge 0x8000) { $sec -= 0x10000 }
        $symbols[$i] = [pscustomobject]@{ Name = $name; Value = (& $u32 ($o + 8)); Section = $sec; Class = $b[$o + 16]; Aux = $b[$o + 17] }
        $i += 1 + $b[$o + 17]
    }
    # a named definition at a section offset (a function), for relocations made against a section symbol
    $byPlace = @{}
    foreach ($y in $symbols.Values) { if ($y.Aux -eq 0 -and $y.Section -gt 0) { $k = "$($y.Section):$($y.Value)"; if (-not $byPlace.ContainsKey($k)) { $byPlace[$k] = $y.Name } } }
    $table = @{}
    for ($s = 0; $s -lt $nsec; $s++) {
        $sec = $sections[$s]
        if ($sec.Name -ne '.pdata') { continue }
        $rel = @{}
        for ($r = 0; $r -lt $sec.NRel; $r++) { $ro = $sec.Rel + 10 * $r; $rel[(& $u32 $ro)] = $symbols[[int](& $u32 ($ro + 4))] }
        for ($e = 0; $e + 12 -le $sec.Size; $e += 12) {
            $fs = $rel[[long]$e]; $xs = $rel[[long]($e + 8)]
            if (-not $fs -or -not $xs) { continue }
            $fOff = $fs.Value + (& $u32 ($sec.Raw + $e)); $fn = $byPlace["$($fs.Section):$fOff"]; if (-not $fn) { $fn = "$($fs.Name)+$fOff" }
            $xsec = $sections[$xs.Section - 1]; $x = $xsec.Raw + $xs.Value + (& $u32 ($sec.Raw + $e + 8))
            $count = $b[$x + 2]; $len = 4 + 2 * ($count + ($count % 2))
            if ($b[$x] -band 0x18) { $len += 4 }   # EHANDLER / UHANDLER: the handler's address follows the codes
            $table[$fn] = (($b[$x..($x + $len - 1)] | ForEach-Object { $_.ToString('X2') }) -join ' ')
        }
    }
    $table
}
