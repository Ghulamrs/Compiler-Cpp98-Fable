# harness.ps1 - the release checks of cppparse, in order, each as its own PowerShell process; one line per check
# ("PASS" or "FAIL", the seconds it took, its log) and exit code 0 only when every check passed. The logs are in
# out\harness\<check>.txt; a failed check also prints the last lines of its log.
#   suite        build-cppparse.ps1: builds sandbox\cppparse.exe with cl /W4 /permissive- and runs all test groups
#   selfhost-O0  selfhost.ps1 -Lvl 0: the own sources compiled by cppparse at -O0, generation two identical
#   selfhost-O2  selfhost.ps1 -Lvl 2: the same at -O2 (leaves the -O2 compiler in out\selfhost and out\selfhost2,
#                which the following checks use)
#   suite-self   build-cppparse.ps1 -Exe out\selfhost\cppparse-self.exe: all test groups under the -O2 self-built compiler
#   mix          mix.ps1: cl's objects of the own sources swapped one at a time for cppparse's
#   maps         maps.ps1: the link maps of the reference and the self-built compiler
#   cprog        cprog.ps1: the C sample with cl and the three generations of cppparse
#   matrix       matrix.ps1: the matrix sample, strict C++98 under clang, against both oracles
#   strict98     strict98.ps1: the own sources under clang -std=c++98 -pedantic, no diagnostic but the documented ones
#   lib-O0/-O2   lib.ps1: the library sample with its native driver
#   life-O0/-O2  life.ps1: the lifetime sample with its native driver
#   eh           eh.ps1: exceptions and unwind tables at -O0..-O3
#   mapcompare   mapcompare.ps1: cppparse's object maps against link's
#   crc32        crc32.ps1: every tracked file against crc32.txt
# -Eh <model> runs every check with that exception model wherever a script names none (the compiler takes its default
# from the environment variable CPPPARSE_EH): harness.ps1 -Eh cl is the whole harness in cl's own model.
# -Quick runs the suite and selfhost-O0 only. When the suite or a selfhost check fails, the checks after it are
# reported as not run (they would test a stale compiler).
# Usage: powershell -ExecutionPolicy Bypass -File cpp\sandbox\scripts\harness.ps1 [-Quick]
param([switch]$Quick, [string]$Eh = "")
$ErrorActionPreference = "Stop"
$Sandbox = Split-Path $PSScriptRoot -Parent
$logDir = "$Sandbox\out\harness"
New-Item -ItemType Directory -Force $logDir | Out-Null

# name, script (relative to sandbox\), arguments, an extra test of the log (the scripts lib.ps1 and life.ps1 exit 0
# whatever their driver does, maps.ps1 too), and whether a failure stops the run
$checks = @(
    @{ Name = "suite";       Script = "build-cppparse.ps1";     Args = "";                                   Stop = $true; Want = "build and all tests succeeded" },
    @{ Name = "selfhost-O0"; Script = "scripts\selfhost.ps1";   Args = "-Lvl 0";                             Stop = $true; Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-O2"; Script = "scripts\selfhost.ps1";   Args = "-Lvl 2";                             Stop = $true; Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "suite-self";  Script = "build-cppparse.ps1";     Args = "-Exe `"$Sandbox\out\selfhost\cppparse-self.exe`""; Want = "build and all tests succeeded" },
    @{ Name = "mix";         Script = "scripts\mix.ps1";        Args = "";                                   Want = "mix: all builds linked and ran" },
    @{ Name = "maps";        Script = "scripts\maps.ps1";       Args = "";                                   Want = "cppparse symbols only in ours"; Refuse = "map files missing|LNK\d{4}" },
    @{ Name = "cprog";       Script = "scripts\cprog.ps1";      Args = "" },
    @{ Name = "matrix";      Script = "scripts\matrix.ps1";     Args = "";                                   Want = "matrix sample: all checks passed" },
    @{ Name = "strict98";    Script = "scripts\strict98.ps1";   Args = "";                                   Want = "strict98: passed" },
    @{ Name = "lib-O0";      Script = "scripts\lib.ps1";        Args = "-Lvl 0";                             Want = "(?m)^ALL OK\r?$"; Refuse = "(?m)driver exit code (?!0\r?$)" },
    @{ Name = "lib-O2";      Script = "scripts\lib.ps1";        Args = "-Lvl 2";                             Want = "(?m)^ALL OK\r?$"; Refuse = "(?m)driver exit code (?!0\r?$)" },
    @{ Name = "life-O0";     Script = "scripts\life.ps1";       Args = "-Lvl 0";                             Want = "(?m)^ALL OK\r?$"; Refuse = "(?m)driver exit code (?!0\r?$)" },
    @{ Name = "life-O2";     Script = "scripts\life.ps1";       Args = "-Lvl 2";                             Want = "(?m)^ALL OK\r?$"; Refuse = "(?m)driver exit code (?!0\r?$)" },
    @{ Name = "eh";          Script = "scripts\eh.ps1";         Args = "" },
    @{ Name = "mapcompare";  Script = "scripts\mapcompare.ps1"; Args = "" },
    @{ Name = "crc32";       Script = "scripts\crc32.ps1";      Args = "" }
)
if ($Quick) { $checks = @($checks | Where-Object { $_.Name -eq "suite" -or $_.Name -eq "selfhost-O0" }) }

if ($Eh -ne "") { $env:CPPPARSE_EH = $Eh; Write-Host "=== exception model: $Eh (CPPPARSE_EH) ===" }
$started = Get-Date
$failed = @(); $notRun = @(); $stopped = $false
Write-Host ("=== harness: {0} check(s){1}, logs in {2} ===" -f $checks.Count, $(if ($Quick) { " (-Quick)" } else { "" }), $logDir)
foreach ($c in $checks) {
    if ($stopped) { $notRun += $c.Name; Write-Host ("NOT RUN {0}" -f $c.Name); continue }
    $log = "$logDir\$($c.Name).txt"; $err = "$logDir\$($c.Name).err.txt"
    $argLine = "-NoProfile -ExecutionPolicy Bypass -File `"$Sandbox\$($c.Script)`" $($c.Args)"
    $t = Get-Date
    # the check's own process only: Start-Process -Wait waits for every process it started as well, and cl leaves
    # vctip.exe (MSVC's telemetry) behind, which can outlive the check by many minutes. The handle is taken at once,
    # so that the exit code is kept after the process ends
    $p = Start-Process -FilePath "powershell.exe" -ArgumentList $argLine -WorkingDirectory $Sandbox -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError $err
    $null = $p.Handle
    $p.WaitForExit()
    $secs = ((Get-Date) - $t).TotalSeconds
    $text = (Get-Content $log -Raw -ErrorAction SilentlyContinue) + (Get-Content $err -Raw -ErrorAction SilentlyContinue)
    if ($null -eq $text) { $text = "" }
    $why = @()
    if ($p.ExitCode -ne 0) { $why += "exit $($p.ExitCode)" }
    if ($c.Want -and $text -notmatch $c.Want) { $why += "no '$($c.Want)' in the log" }
    if ($c.Refuse -and $text -match $c.Refuse) { $why += "the log has '$($Matches[0])'" }
    if ($why.Count -eq 0) { Write-Host ("PASS    {0,-12} {1,6:N0} s" -f $c.Name, $secs) }
    else {
        Write-Host ("FAIL    {0,-12} {1,6:N0} s  ({2}; log {3})" -f $c.Name, $secs, ($why -join ", "), $log)
        ($text -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -Last 12) | ForEach-Object { Write-Host "        | $_" }
        $failed += $c.Name
        if ($c.Stop) { $stopped = $true }
    }
}
Write-Host ("=== harness: {0} passed, {1} failed{2}, {3:N0} min ===" -f ($checks.Count - $failed.Count - $notRun.Count), $failed.Count, $(if ($notRun.Count) { ", $($notRun.Count) not run" } else { "" }), ((Get-Date) - $started).TotalMinutes)
if ($failed.Count -ne 0 -or $notRun.Count -ne 0) { Write-Host "harness FAILED: $($failed -join ', ')"; exit 1 }
Write-Host "harness: all checks passed"
exit 0
