# cvsame.ps1 - the comparator of invariant 1 of docs\design-codeview.md (5.5/1): debug information changes no code.
# It compares an object A with an object B that may differ from it in CodeView debug information only, which is what
# the same source gives without and with -g, and says whether everything else of B is what A has:
#   - A has N sections and M symbol table entries, B at least as many;
#   - B's first N section headers are A's in name and flags (the alignment is among them) and, outside .debug$S,
#     in size, contents and relocation entries, byte for byte. File pointers are not compared: they move;
#   - B's first M symbol table entries are A's bytes, and A's string table is the start of B's: every symbol index
#     in every relocation of the code then means what it meant. One thing is left out: the auxiliary record of a
#     .debug$S section's own symbol holds that section's length and relocation count, which are debug information;
#   - the sections of B behind the N-th are .debug$T or .debug$S, and the symbols behind the M-th are section symbols
#     of those (5.1: what -g adds comes behind everything an object has without it);
#   - with -Listings: the listings beside the objects (<name>.asm for <name>.obj) are the same lines once the marker
#     lines only -g writes are taken out (5.4: ;@proc, ;@prologend, ;@epilog, ;@block, ;@scope, ;@local, ;@gone,
#     ;@static, ;@var, ;@loc); where neither object has a listing beside it there is nothing to compare.
# -DebugOnly asks for more: B has exactly A's sections and symbols, so nothing but the contents of .debug$S sections
# may differ (two compilers on one source, where only the debug records were changed).
# -DebugNames (with -Listings) also takes the ;@debug lines out of both listings: they hold the name a procedure's
# symbol shows, which is debug information that every object has.
#   cvsame.ps1 -A <object or folder> -B <object or folder> [-Listings] [-DebugOnly] [-DebugNames] [-Recurse] [-Quiet]
# Given folders, every *.obj of A (with -Recurse: of A and the folders under it, by its path under A) is compared
# with the object of that name in B; one that B lacks, or that only B has, is a difference. One line per pair that is not byte for byte the same (none with -Quiet, where only
# violations are named), then the count. Exit code 0 when no pair violates the invariant, 1 otherwise, 2 for a
# file that is no x64 object.
#   cvsame.ps1 -SelfTest -A <object> -B <work folder>
# The comparator against cases with known answers, made from the object given (one of cppparse's, with a .debug$S):
# copies with one byte changed in the code, a relocation, a symbol, the string table, a section's flags, a
# .debug$S; copies with a section added behind the last one, with and without its section symbol, and one added
# in between; listings with and without the markers. Exit code 0 when every verdict is the expected one.
param([Parameter(Mandatory = $true)][string]$A, [Parameter(Mandatory = $true)][string]$B, [switch]$Listings, [switch]$DebugOnly, [switch]$DebugNames, [switch]$Recurse, [switch]$Quiet, [switch]$SelfTest)
$ErrorActionPreference = "Stop"
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Text;
public static class CvSame {
    class Sec { public string name; public uint size, raw, relocAt, flags; public long relocs; }
    class Obj {
        public byte[] d; public int sections; public uint symAt, symCount; public ushort machine, flags;
        public List<Sec> secs = new List<Sec>(); public long stringsAt; public uint stringsSize;
    }
    static ushort U16(byte[] d, long at) { return (ushort)(d[at] | d[at + 1] << 8); }
    static uint U32(byte[] d, long at) { return (uint)(d[at] | d[at + 1] << 8 | d[at + 2] << 16 | d[at + 3] << 24); }
    static Obj Parse(byte[] d) {
        if (d.Length < 20 || U16(d, 0) != 0x8664) return null;
        var o = new Obj(); o.d = d; o.machine = U16(d, 0); o.sections = U16(d, 2); o.symAt = U32(d, 8); o.symCount = U32(d, 12); o.flags = U16(d, 18);
        if (U16(d, 16) != 0) return null;                       // an optional header: not an object
        for (int s = 0; s < o.sections; ++s) {
            long h = 20 + 40L * s;
            if (h + 40 > d.Length) return null;
            var x = new Sec();
            int n = 0; while (n < 8 && d[h + n] != 0) ++n;
            x.name = Encoding.ASCII.GetString(d, (int)h, n);
            x.size = U32(d, h + 16); x.raw = U32(d, h + 20); x.relocAt = U32(d, h + 24); x.flags = U32(d, h + 36);
            x.relocs = U16(d, h + 32);
            // IMAGE_SCN_LNK_NRELOC_OVFL: the count is in the first entry, which is counted
            if ((x.flags & 0x01000000u) != 0 && x.relocAt != 0) x.relocs = U32(d, x.relocAt);
            if (x.raw != 0 && (long)x.raw + x.size > d.Length) return null;
            if (x.relocAt != 0 && (long)x.relocAt + x.relocs * 10 > d.Length) return null;
            o.secs.Add(x);
        }
        o.stringsAt = (long)o.symAt + 18L * o.symCount;
        if (o.stringsAt + 4 > d.Length) return null;
        o.stringsSize = U32(d, o.stringsAt);
        if (o.stringsAt + o.stringsSize > d.Length) return null;
        return o;
    }
    static bool Same(byte[] a, long aAt, byte[] b, long bAt, long n) {
        for (long i = 0; i < n; ++i) if (a[aAt + i] != b[bAt + i]) return false;
        return true;
    }
    static bool Debug(Sec s) { return s.name == ".debug`$S"; }
    // what of B is not A's: empty when the invariant holds. 'notes' gets what differs within the invariant
    public static string[] Compare(byte[] da, byte[] db, bool debugOnly, List<string> notes) {
        var bad = new List<string>();
        Obj a = Parse(da), b = Parse(db);
        if (a == null) return new string[] { "A is not an x64 object" };
        if (b == null) return new string[] { "B is not an x64 object" };
        if (a.flags != b.flags) bad.Add("the file header's flags differ");
        if (b.sections < a.sections) { bad.Add("B has " + b.sections + " sections, A " + a.sections); return bad.ToArray(); }
        if (b.symCount < a.symCount) { bad.Add("B has " + b.symCount + " symbol table entries, A " + a.symCount); return bad.ToArray(); }
        int debugDiffer = 0;
        for (int s = 0; s < a.sections; ++s) {
            Sec x = a.secs[s], y = b.secs[s];
            string id = "section " + (s + 1) + " (" + x.name + ")";
            if (x.name != y.name) { bad.Add(id + " is " + y.name + " in B"); continue; }
            uint mask = Debug(x) ? ~0x01000000u : ~0u;         // a .debug`$S may get more than 65534 relocations
            if ((x.flags & mask) != (y.flags & mask)) bad.Add(id + ": flags " + x.flags.ToString("X8") + " and " + y.flags.ToString("X8"));
            bool same = x.size == y.size && x.relocs == y.relocs && (x.raw == 0) == (y.raw == 0)
                        && (x.raw == 0 || Same(a.d, x.raw, b.d, y.raw, x.size))
                        && (x.relocs == 0 || Same(a.d, x.relocAt, b.d, y.relocAt, x.relocs * 10));
            if (same) continue;
            if (Debug(x)) { ++debugDiffer; continue; }
            if (x.size != y.size) bad.Add(id + ": " + x.size + " bytes in A, " + y.size + " in B");
            else if (x.relocs != y.relocs) bad.Add(id + ": " + x.relocs + " relocations in A, " + y.relocs + " in B");
            else if (x.raw != 0 && y.raw != 0 && !Same(a.d, x.raw, b.d, y.raw, x.size)) bad.Add(id + ": the contents differ");
            else bad.Add(id + ": the relocation entries differ");
        }
        if (debugDiffer != 0) notes.Add(debugDiffer + " .debug`$S differ");
        // the symbol table: A's entries first, the same bytes
        long i = 0; int differing = 0; string first = null;
        while (i < a.symCount) {
            long pa = a.symAt + 18 * i, pb = b.symAt + 18 * i;
            int aux = a.d[pa + 17];
            if (!Same(a.d, pa, b.d, pb, 18)) { if (++differing == 1) first = "entry " + i; i += 1 + aux; continue; }
            int sec = (short)U16(a.d, pa + 12);
            bool sectionSymbol = a.d[pa + 16] == 3 && aux == 1 && sec >= 1 && sec <= a.sections && U32(a.d, pa + 8) == 0;
            for (int k = 1; k <= aux && i + k < a.symCount; ++k) {
                long qa = pa + 18 * k, qb = pb + 18 * k;
                // a section symbol's auxiliary record: length (4), relocations (2), line numbers (2), checksum (4),
                // associated section (2), selection (1), 3 unused
                if (sectionSymbol && Debug(a.secs[sec - 1])) { if (!Same(a.d, qa + 6, b.d, qb + 6, 12)) { if (++differing == 1) first = "the auxiliary record of entry " + i; } }
                else if (!Same(a.d, qa, b.d, qb, 18)) { if (++differing == 1) first = "the auxiliary record of entry " + i; }
            }
            i += 1 + aux;
        }
        if (differing != 0) bad.Add(differing + " of A's symbol table entries differ in B, the first " + first);
        if (b.stringsSize < a.stringsSize || !Same(a.d, a.stringsAt + 4, b.d, b.stringsAt + 4, a.stringsSize - 4)) bad.Add("A's string table is not the start of B's");
        // what B adds: sections and their section symbols, behind everything of A
        int addedSections = b.sections - a.sections; long addedSymbols = 0;
        for (int s = a.sections; s < b.sections; ++s)
            if (b.secs[s].name != ".debug`$T" && b.secs[s].name != ".debug`$S") bad.Add("added section " + (s + 1) + " is " + b.secs[s].name + ", neither .debug`$T nor .debug`$S");
        i = a.symCount;
        while (i < b.symCount) {
            long pb = b.symAt + 18 * i; int aux = b.d[pb + 17];
            int sec = (short)U16(b.d, pb + 12);
            ++addedSymbols;
            if (!(b.d[pb + 16] == 3 && aux == 1 && sec > a.sections && sec <= b.sections && U32(b.d, pb + 8) == 0)) bad.Add("added symbol table entry " + i + " is not the section symbol of an added section");
            i += 1 + aux;
        }
        if (debugOnly && (addedSections != 0 || addedSymbols != 0)) bad.Add("B adds " + addedSections + " section(s) and " + addedSymbols + " symbol(s)");
        if (addedSections != 0 || addedSymbols != 0) notes.Add("B adds " + addedSections + " section(s), " + addedSymbols + " symbol(s)");
        return bad.ToArray();
    }
    static bool Marker(string line) {
        string t = line.TrimStart();
        if (!t.StartsWith(";@")) return false;
        string[] kinds = { "proc", "prologend", "epilog", "block", "scope", "local", "gone", "static", "var", "loc" };
        foreach (string k in kinds) if (t.Length >= 2 + k.Length && string.CompareOrdinal(t, 2, k, 0, k.Length) == 0 && (t.Length == 2 + k.Length || t[2 + k.Length] == ' ' || t[2 + k.Length] == '\r')) return true;
        return false;
    }
    // "" when B's listing is A's once the lines only -g writes are taken out of both (A has them too when it was
    // compiled with -g itself), else where they part. removed: how many more of them B has than A
    // debugNames: the ";@debug" lines are left out of both as well
    public static string Listings(string[] la, string[] lb, bool debugNames, int[] removed) {
        int i = 0, n = 0;
        foreach (string l in lb) {
            if (Marker(l)) { ++n; continue; }
            if (debugNames && l.StartsWith(";@debug ")) continue;
            while (i < la.Length && (Marker(la[i]) || (debugNames && la[i].StartsWith(";@debug ")))) { if (Marker(la[i])) --n; ++i; }
            if (i >= la.Length) return "B's listing has more lines than A's " + la.Length;
            if (!string.Equals(la[i], l, StringComparison.Ordinal)) return "the listings differ at A's line " + (i + 1);
            ++i;
        }
        while (i < la.Length && (Marker(la[i]) || (debugNames && la[i].StartsWith(";@debug ")))) { if (Marker(la[i])) --n; ++i; }
        removed[0] = n;
        return i == la.Length ? "" : "B's listing ends at A's line " + (i + 1);
    }
}
"@
function Pairs {
    if (Test-Path -LiteralPath $A -PathType Container) {
        if (-not (Test-Path -LiteralPath $B -PathType Container)) { throw "-A is a folder, -B is not: $B" }
        $ra = (Resolve-Path -LiteralPath $A).Path.TrimEnd([char]92); $rb = (Resolve-Path -LiteralPath $B).Path.TrimEnd([char]92)
        $names = @(Get-ChildItem -LiteralPath $ra -Filter *.obj -File -Recurse:$Recurse | ForEach-Object { $_.FullName.Substring($ra.Length + 1) }) + @(Get-ChildItem -LiteralPath $rb -Filter *.obj -File -Recurse:$Recurse | ForEach-Object { $_.FullName.Substring($rb.Length + 1) }) | Sort-Object -Unique
        foreach ($n in $names) { [pscustomobject]@{ Name = $n; A = (Join-Path $A $n); B = (Join-Path $B $n) } }
    } else { [pscustomobject]@{ Name = (Split-Path $A -Leaf); A = $A; B = $B } }
}
# what of B is not A's (the object's bytes, and the two listings when they are named); $notes gets what differs
# within the invariant
function Violations([byte[]]$da, [byte[]]$db, [bool]$debugOnly, [string]$la, [string]$lb, $notes) {
    $bad = @([CvSame]::Compare($da, $db, $debugOnly, $notes))
    if ($la -ne "") {
        $hasA = Test-Path -LiteralPath $la; $hasB = Test-Path -LiteralPath $lb
        # an object assembled from a listing of another name has none beside it: then neither side has, and there is
        # nothing to compare
        if ($hasA -ne $hasB) { $bad += "a listing (.asm) is beside one of the objects only" }
        elseif ($hasA) {
            $removed = New-Object int[] 1
            $l = [CvSame]::Listings([IO.File]::ReadAllLines($la), [IO.File]::ReadAllLines($lb), [bool]$DebugNames, $removed)
            if ($l -ne "") { $bad += $l } elseif ($removed[0] -ne 0) { $notes.Add("$($removed[0]) marker line(s) more in B's listing") }
        }
    }
    return $bad
}
function Told([string[]]$bad) { if ($bad.Count -le 4) { return $bad -join '; ' }; return (($bad | Select-Object -First 4) -join '; ') + "; and $($bad.Count - 4) more" }

if ($SelfTest) {
    $bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $A).Path)
    New-Item -ItemType Directory -Force $B | Out-Null
    $n = [BitConverter]::ToUInt16($bytes, 2); $symAt = [BitConverter]::ToUInt32($bytes, 8); $symN = [BitConverter]::ToUInt32($bytes, 12)
    function Field([int]$sec, [int]$at) { return [BitConverter]::ToUInt32($bytes, 20 + 40 * $sec + $at) }
    $dbg = -1; for ($i = 0; $i -lt $n; $i++) { if ($dbg -lt 0 -and [Text.Encoding]::ASCII.GetString($bytes, 20 + 40 * $i, 8) -eq '.debug$S') { $dbg = $i } }
    if ($dbg -lt 0 -or (Field 0 24) -eq 0 -or $symN -lt 8) { Write-Host "cvsame -SelfTest: $A has no .debug`$S, no relocation in its first section or too few symbols"; exit 2 }
    function Damaged([long]$at) { $c = [byte[]]$bytes.Clone(); $c[$at] = $c[$at] -bxor 1; return ,$c }
    # what -g does, made by hand: a section header more (every file pointer moves by 40) and the section's bytes at
    # the end of the file; with $symbol its section symbol behind the last symbol; with $second the header is put
    # second, where it renumbers the sections behind it
    function Added([string]$section, [bool]$symbol, [bool]$second) {
        $hdrEnd = 20 + 40 * $n
        $head = New-Object byte[] $hdrEnd; [Array]::Copy($bytes, $head, $hdrEnd)
        [BitConverter]::GetBytes([uint16]($n + 1)).CopyTo($head, 2)
        [BitConverter]::GetBytes([uint32]($symAt + 40)).CopyTo($head, 8)
        if ($symbol) { [BitConverter]::GetBytes([uint32]($symN + 2)).CopyTo($head, 12) }
        for ($i = 0; $i -lt $n; $i++) { foreach ($f in 20, 24) { $at = 20 + 40 * $i + $f; $v = [BitConverter]::ToUInt32($head, $at); if ($v -ne 0) { [BitConverter]::GetBytes([uint32]($v + 40)).CopyTo($head, $at) } } }
        $data = [byte[]](4, 0, 0, 0, 6, 0, 1, 0x10, 0x74, 0, 1, 0)        # the signature and an LF_MODIFIER: const int
        $new = New-Object byte[] 40
        [Text.Encoding]::ASCII.GetBytes($section).CopyTo($new, 0)
        [BitConverter]::GetBytes([uint32]$data.Length).CopyTo($new, 16)
        [BitConverter]::GetBytes([uint32]($bytes.Length + 40 + $(if ($symbol) { 36 } else { 0 }))).CopyTo($new, 20)
        [BitConverter]::GetBytes([uint32]0x42100040).CopyTo($new, 36)
        $symEnd = $symAt + 18 * $symN
        $ms = New-Object IO.MemoryStream
        if ($second) { $ms.Write($head, 0, 60); $ms.Write($new, 0, 40); $ms.Write($head, 60, $hdrEnd - 60) }
        else { $ms.Write($head, 0, $hdrEnd); $ms.Write($new, 0, 40) }
        $ms.Write($bytes, $hdrEnd, $symEnd - $hdrEnd)
        if ($symbol) {
            $sy = New-Object byte[] 36
            [Text.Encoding]::ASCII.GetBytes($section).CopyTo($sy, 0); [BitConverter]::GetBytes([uint16]($n + 1)).CopyTo($sy, 12); $sy[16] = 3; $sy[17] = 1
            [BitConverter]::GetBytes([uint32]$data.Length).CopyTo($sy, 18)
            $ms.Write($sy, 0, 36)
        }
        $ms.Write($bytes, $symEnd, $bytes.Length - $symEnd); $ms.Write($data, 0, $data.Length)
        return ,$ms.ToArray()
    }
    $lines = @("        .code", "f PROC", ";@debug `"f.cpp`" f", ";@line 1", "        ret", "f ENDP", "        END")
    $marked = @($lines[0..2]) + @(";@proc T1 0 0", ";@prologend") + @($lines[3]) + @(";@epilog") + @($lines[4..6])
    [IO.File]::WriteAllLines("$B\plain.asm", [string[]]$lines); [IO.File]::WriteAllLines("$B\marked.asm", [string[]]$marked)
    $other = @($marked); $other[7] = "        ret 8"; [IO.File]::WriteAllLines("$B\other.asm", [string[]]$other)
    $unknown = @($marked); $unknown[4] = ";@weak a b"; [IO.File]::WriteAllLines("$B\unknown.asm", [string[]]$unknown)
    # each case: what it is, the bytes of B, -DebugOnly, the two listings (or ""), whether a violation is expected
    $cases = @(
        @("the object with itself", $bytes, $false, "", "", $false),
        @("a byte of the code changed", (Damaged ((Field 0 20) + 1)), $false, "", "", $true),
        @("a relocation of the code changed", (Damaged ((Field 0 24) + 4)), $false, "", "", $true),
        @("a symbol's value changed", (Damaged ($symAt + 18 * ($symN - 1) + 8)), $false, "", "", $true),
        @("a byte of the string table changed", (Damaged ($symAt + 18 * $symN + 5)), $false, "", "", $true),
        @("a section's flags changed", (Damaged (20 + 36)), $false, "", "", $true),
        @("a byte of a .debug`$S changed", (Damaged ((Field $dbg 20) + 9)), $false, "", "", $false),
        @("a byte of a .debug`$S changed, -DebugOnly", (Damaged ((Field $dbg 20) + 9)), $true, "", "", $false),
        @("a .debug`$T added behind the last section", (Added '.debug$T' $false $false), $false, "", "", $false),
        @("a .debug`$T added behind, -DebugOnly", (Added '.debug$T' $false $false), $true, "", "", $true),
        @("a .debug`$S added behind with its section symbol", (Added '.debug$S' $true $false), $false, "", "", $false),
        @("a .rdata added behind", (Added '.rdata' $false $false), $false, "", "", $true),
        @("a .debug`$T added as the second section", (Added '.debug$T' $false $true), $false, "", "", $true),
        @("a listing with the markers of -g", $bytes, $false, "$B\plain.asm", "$B\marked.asm", $false),
        @("a listing with another instruction", $bytes, $false, "$B\plain.asm", "$B\other.asm", $true),
        @("a listing with a marker -g does not write", $bytes, $false, "$B\plain.asm", "$B\unknown.asm", $true)
    )
    $wrong = 0
    foreach ($c in $cases) {
        $notes = New-Object 'System.Collections.Generic.List[string]'
        $bad = @(Violations $bytes ([byte[]]$c[1]) $c[2] $c[3] $c[4] $notes)
        if (($bad.Count -ne 0) -ne $c[5]) { $wrong++; Write-Host "cvsame -SelfTest: WRONG on '$($c[0])': $(if ($bad.Count) { Told $bad } else { 'no violation found' })" }
    }
    Write-Host "cvsame -SelfTest: $($cases.Count) cases with known answers, $wrong wrong"
    if ($wrong -ne 0) { exit 1 }
    exit 0
}

$same = 0; $within = 0; $violations = 0; $notObject = $false; $total = 0
foreach ($p in @(Pairs)) {
    $total++
    if (-not (Test-Path -LiteralPath $p.A)) { Write-Host "VIOLATION $($p.Name): only in B"; $violations++; continue }
    if (-not (Test-Path -LiteralPath $p.B)) { Write-Host "VIOLATION $($p.Name): only in A"; $violations++; continue }
    $fa = (Resolve-Path -LiteralPath $p.A).Path; $fb = (Resolve-Path -LiteralPath $p.B).Path
    $notes = New-Object 'System.Collections.Generic.List[string]'
    $la = ""; $lb = ""; if ($Listings) { $la = [IO.Path]::ChangeExtension($fa, ".asm"); $lb = [IO.Path]::ChangeExtension($fb, ".asm") }
    $bad = @(Violations ([IO.File]::ReadAllBytes($fa)) ([IO.File]::ReadAllBytes($fb)) ([bool]$DebugOnly) $la $lb $notes)
    if ($bad.Count -ne 0 -and $bad[0] -match 'is not an x64 object$') { $notObject = $true }
    if ($bad.Count -ne 0) { $violations++; Write-Host "VIOLATION $($p.Name): $(Told $bad)" }
    elseif ($notes.Count -eq 0) { $same++ }
    else { $within++; if (-not $Quiet) { Write-Host "debug only $($p.Name): $($notes -join '; ')" } }
}
Write-Host ("cvsame: {0} pair(s): {1} the same{2}, {3} differ in debug information only, {4} violate invariant 1" -f $total, $same, $(if ($Listings) { " (listings too)" } else { "" }), $within, $violations)
if ($notObject) { exit 2 }
if ($violations -ne 0) { exit 1 }
exit 0
