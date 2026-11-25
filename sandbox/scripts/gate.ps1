# gate.ps1 - the release gate: the worktree must be clean (git status --porcelain prints nothing: no change, no
# untracked file that is not ignored) before and after harness.ps1, and every check of harness.ps1 must pass.
# Exit code 0 only then. -Quick is passed on to harness.ps1 (the suite and selfhost -Lvl 0), -RequireGride as well
# (a check skipped because GRIDE is not found then fails the gate; the gate of a machine that has GRIDE).
# Usage: powershell -ExecutionPolicy Bypass -File cpp\sandbox\scripts\gate.ps1 [-Quick] [-RequireGride]
param([switch]$Quick, [switch]$RequireGride)
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function DirtyFiles {
    $lines = @(& git -C $Root status --porcelain 2>&1)
    if ($LASTEXITCODE -ne 0) { return @("git status failed: $($lines -join ' ')") }
    return @($lines | Where-Object { "$_".Trim() })
}

$commit = (& git -C $Root rev-parse --short HEAD 2>&1 | Out-String).Trim()
Write-Host "=== gate: $Root at $commit ==="
$dirty = DirtyFiles
if ($dirty.Count -ne 0) {
    Write-Host "FAIL    clean worktree: $($dirty.Count) file(s) changed or untracked"
    $dirty | Select-Object -First 20 | ForEach-Object { Write-Host "        | $_" }
    Write-Host "gate FAILED: commit or remove the changes first (the gate checks a commit)"
    exit 1
}
Write-Host "PASS    clean worktree"

$harnessArgs = @(); if ($Quick) { $harnessArgs += "-Quick" }; if ($RequireGride) { $harnessArgs += "-RequireGride" }
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\harness.ps1" @harnessArgs
$harnessCode = $LASTEXITCODE

$dirty = DirtyFiles
$cleanAfter = ($dirty.Count -eq 0)
if ($cleanAfter) { Write-Host "PASS    worktree still clean after the harness" }
else {
    Write-Host "FAIL    the harness changed the worktree: $($dirty.Count) file(s)"
    $dirty | Select-Object -First 20 | ForEach-Object { Write-Host "        | $_" }
}
if ($harnessCode -ne 0 -or -not $cleanAfter) { Write-Host "gate FAILED at $commit"; exit 1 }
Write-Host "gate passed at $commit"
exit 0
