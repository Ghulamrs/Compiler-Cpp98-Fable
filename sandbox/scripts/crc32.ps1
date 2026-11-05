# crc32.ps1 - the CRC32 of every file of the repository, against the manifest crc32.txt at the repository root.
#   crc32.ps1            verifies: every file must have the manifest's CRC32 and size, every manifest entry must exist;
#                        prints the differences and exits 1 on any
#   crc32.ps1 -Write     computes the manifest anew (a release step, after the last edit before the commit)
# The files are those git tracks (ls-files), the manifest itself apart; an untracked file that is not ignored is named
# as a note, so that a new file gets added before -Write. Outside a git checkout (the release archive, cppparse-6.0.zip
# from `git archive`) every file under the root counts. The CRC32 is the IEEE one (polynomial EDB88320, as zip and
# PNG use). A text file (no NUL byte in its first 8000 bytes, git's own test) is hashed with every CR LF taken as LF and
# its size counted that way, so that the manifest holds whichever line endings a checkout has; a binary file is hashed
# as it is. Format: one line per file, "<crc32 hex> <size> <path>", sorted by path, the paths with forward slashes.
param([switch] $Write)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$manifest = Join-Path $root "crc32.txt"
Add-Type -TypeDefinition @"
public static class Crc32Ieee {
    static readonly uint[] table = Build();
    static uint[] Build() {
        var t = new uint[256];
        for (uint n = 0; n < 256; ++n) { uint c = n; for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[n] = c; }
        return t;
    }
    public static bool IsText(byte[] data) {
        int n = data.Length < 8000 ? data.Length : 8000;
        for (int i = 0; i < n; ++i) if (data[i] == 0) return false;
        return true;
    }
    // the CRC and the length, a CR before an LF left out when text is true
    public static uint[] Of(byte[] data, bool text) {
        uint c = 0xFFFFFFFFu; uint len = 0;
        for (int i = 0; i < data.Length; ++i) {
            if (text && data[i] == 13 && i + 1 < data.Length && data[i + 1] == 10) continue;
            c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8); ++len;
        }
        return new uint[] { c ^ 0xFFFFFFFFu, len };
    }
}
"@
Push-Location $root
try {
    $inGit = $false
    $files = @()
    if (Test-Path -LiteralPath (Join-Path $root ".git")) {
        $files = @(& git ls-files 2>$null | Where-Object { $_ -ne "crc32.txt" } | Sort-Object -Unique)
        $inGit = $LASTEXITCODE -eq 0 -and $files.Count -gt 0
    }
    if ($inGit) {
        $others = @(& git ls-files --others --exclude-standard | Where-Object { $_ -ne "crc32.txt" })
        if ($others.Count -gt 0) { Write-Host "crc32: untracked, so not listed: $($others -join ', ')" }
    } else {
        $files = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/') } | Where-Object { $_ -ne "crc32.txt" -and $_ -notmatch '^\.git/' } | Sort-Object -Unique)
        if ($Write) { Write-Host "crc32: not a git checkout: the manifest would list every file under $root"; exit 1 }
    }
    if ($files.Count -eq 0) { Write-Host "crc32: no files found under $root"; exit 1 }
    $lines = @()
    $current = @{}
    foreach ($p in $files) {
        $full = Join-Path $root $p
        if (-not (Test-Path -LiteralPath $full)) { $current[$p] = "missing"; continue }
        $bytes = [IO.File]::ReadAllBytes($full)
        $r = [Crc32Ieee]::Of($bytes, [Crc32Ieee]::IsText($bytes))
        $entry = ("{0:X8} {1}" -f $r[0], $r[1])
        $current[$p] = $entry
        $lines += "$entry $p"
    }
    if ($Write) {
        [IO.File]::WriteAllText($manifest, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::ASCII)
        Write-Host "crc32: wrote $manifest with $($lines.Count) files"
        exit 0
    }
    if (-not (Test-Path -LiteralPath $manifest)) { Write-Host "crc32: no manifest $manifest (run with -Write)"; exit 1 }
    $listed = @{}
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if ($line -notmatch '^([0-9A-F]{8}) (\d+) (.+)$') { continue }
        $listed[$Matches[3]] = "$($Matches[1]) $($Matches[2])"
    }
    $problems = @()
    foreach ($p in $files) {
        if (-not $listed.ContainsKey($p)) { $problems += "not in the manifest: $p"; continue }
        if ($current[$p] -eq "missing") { $problems += "missing from the checkout: $p"; continue }
        if ($listed[$p] -ne $current[$p]) { $problems += "differs: $p (manifest $($listed[$p]), checkout $($current[$p]))" }
    }
    foreach ($p in $listed.Keys) { if ($files -notcontains $p) { $problems += "in the manifest, not tracked: $p" } }
    if ($problems.Count -eq 0) { Write-Host "crc32: all $($files.Count) tracked files match the manifest"; exit 0 }
    foreach ($m in $problems) { Write-Host "  $m" }
    Write-Host "crc32: $($problems.Count) difference(s)"
    exit 1
} finally { Pop-Location }
