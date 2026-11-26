# harness.ps1 - the release checks of cppparse, in order, each as its own PowerShell process; one line per check
# ("PASS", "FAIL" or "SKIPPED", the seconds it took, its log) and exit code 0 only when every check that ran passed.
# The logs are in out\harness\<check>.txt; a failed check also prints the last lines of its log.
#   suite        build-cppparse.ps1: builds sandbox\cppparse.exe with cl /W4 /permissive- and runs all test groups
#   selfhost-O0  selfhost.ps1 -Lvl 0: the own sources compiled by cppparse at -O0, generation two identical
#   selfhost-gride-O0  selfhost.ps1 -Lvl 0 -Linker gride -Relink: the objects selfhost-O0 left, both generations
#                linked by GRIDE's link with /timestamp:0; the two exes byte-identical (needs GRIDE)
#   selfhost-g-O0  selfhost.ps1 -Lvl 0 -G: both generations compile with -g (CodeView types and variables, row M10),
#                generation two identical; in folders of their own
#   selfhost-g2-O0  selfhost.ps1 -Lvl 0 -G2: the same with -g2 (the member functions in the classes too), generation two
#                identical
#   selfhost-O2  selfhost.ps1 -Lvl 2: the same at -O2 (leaves the -O2 compiler in out\selfhost and out\selfhost2,
#                which the following checks use)
#   selfhost-g-O2  selfhost.ps1 -Lvl 2 -G: both generations compile with -g at -O2, where the variables' places are
#                register and frame ranges that the object writer checks against the code (CvCheck); generation two
#                identical
#   selfhost-gride-O2  the same relink at -O2 (leaves the GRIDE-linked -O2 compiler in out\selfhost-gride; needs GRIDE)
#   suite-self-gride   build-cppparse.ps1 -Exe out\selfhost-gride\cppparse-self.exe: all test groups under the
#                self-built compiler as GRIDE's link made it (needs GRIDE). It runs before suite-self, so that the
#                checks after the two find what suite-self leaves in out\
#   suite-self   build-cppparse.ps1 -Exe out\selfhost\cppparse-self.exe: all test groups under the -O2 self-built compiler
#   mix          mix.ps1: cl's objects of the own sources swapped one at a time for cppparse's
#   maps         maps.ps1: the link maps of the reference and the self-built compiler
#   cprog        cprog.ps1: the C sample with cl and the three generations of cppparse
#   matrix       matrix.ps1: the matrix sample, strict C++98 under clang, against both oracles
#   strict98     strict98.ps1: the own sources under clang -std=c++98 -pedantic, no diagnostic but the documented ones
#   lib-O0/-O2   lib.ps1: the library sample with its native driver
#   life-O0/-O2  life.ps1: the lifetime sample with its native driver
#   eh           eh.ps1: exceptions and unwind tables at -O0..-O3
#   eh-rtself    eh.ps1 again with cppparse's object of the runtime in place of cl's (CPPPARSE_RT=cppparse, row P1)
#   target       target.ps1: what the Windows x64 target adds (row P1): thread-locals against ml64 and cl, in threads under
#                both linkers and on GRIDE's runtime, shared with cl's objects, the refusals (needs GRIDE)
#   replace      replace.ps1: replaceable definitions and libcpp's allocation functions under both linkers (needs GRIDE)
#   mapcompare   mapcompare.ps1: cppparse's object maps against link's
#   crc32        crc32.ps1: every tracked file against crc32.txt
# -Eh <model> runs every check with that exception model wherever a script names none (the compiler takes its default
# from the environment variable CPPPARSE_EH): harness.ps1 -Eh cl is the whole harness in cl's own model.
# -Quick runs the suite and selfhost-O0 only. When the suite or a selfhost check fails, the checks after it are
# reported as not run (they would test a stale compiler).
# GRIDE (row P1, docs\design-selfhost.md 4.6): a check that needs GRIDE's tools logs which it used (the folder, the
# SHA-256 of link.exe and of the runtime's libraries, GRIDE3's commit). Where GRIDE is not found (toolset.ps1 says
# where it is looked for) such a check is "SKIPPED": it is named in the summary, it is not a pass, and the harness
# then says "every check that ran passed" and not "all checks passed". -RequireGride makes a skip a failure: that
# is the gate on a machine that has GRIDE. What a check finds different under GRIDE's link from Microsoft's, and is
# known (the script prints "not under GRIDE's link: ..."), is counted on its line and in the summary.
# Usage: powershell -ExecutionPolicy Bypass -File cpp\sandbox\scripts\harness.ps1 [-Quick] [-Eh cl] [-RequireGride]
param([switch]$Quick, [string]$Eh = "", [switch]$RequireGride)
$ErrorActionPreference = "Stop"
$Sandbox = Split-Path $PSScriptRoot -Parent
$logDir = "$Sandbox\out\harness"
New-Item -ItemType Directory -Force $logDir | Out-Null

# name, script (relative to sandbox\), arguments, an extra test of the log (the scripts lib.ps1 and life.ps1 exit 0
# whatever their driver does, maps.ps1 too), whether a failure stops the run, whether the check needs GRIDE (its
# script then exits with 77 and "skipped: GRIDE not found" where there is none), and the check whose result it
# builds on (After: skipped when that one was skipped, not run when that one failed)
$checks = @(
    @{ Name = "suite";       Script = "build-cppparse.ps1";     Args = "";                                   Stop = $true; Want = "build and all tests succeeded" },
    @{ Name = "selfhost-O0"; Script = "scripts\selfhost.ps1";   Args = "-Lvl 0 -Linker ms";                  Stop = $true; Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-gride-O0"; Script = "scripts\selfhost.ps1"; Args = "-Lvl 0 -Linker gride -Relink";   Gride = $true; Want = "GRIDE's link: cppparse-self.exe and cppparse-self2.exe are byte-identical" },
    @{ Name = "selfhost-g-O0"; Script = "scripts\selfhost.ps1"; Args = "-Lvl 0 -Linker ms -G";            Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-g2-O0"; Script = "scripts\selfhost.ps1"; Args = "-Lvl 0 -Linker ms -G2";          Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-O2"; Script = "scripts\selfhost.ps1";   Args = "-Lvl 2 -Linker ms";                  Stop = $true; Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-g-O2"; Script = "scripts\selfhost.ps1"; Args = "-Lvl 2 -Linker ms -G";            Want = "bootstrap: generation two is identical to generation one" },
    @{ Name = "selfhost-gride-O2"; Script = "scripts\selfhost.ps1"; Args = "-Lvl 2 -Linker gride -Relink";   Gride = $true; Want = "GRIDE's link: cppparse-self.exe and cppparse-self2.exe are byte-identical" },
    @{ Name = "suite-self-gride"; Script = "build-cppparse.ps1"; Args = "-Exe `"$Sandbox\out\selfhost-gride\cppparse-self.exe`""; After = "selfhost-gride-O2"; Want = "build and all tests succeeded" },
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
    @{ Name = "eh-rtself";   Script = "scripts\eh.ps1";         Args = "";                                   Env = @{ CPPPARSE_RT = "cppparse" }; Want = "eh: all checks passed" },
    @{ Name = "target";      Script = "scripts\target.ps1";     Args = "";                                   Gride = $true; Want = "target: all checks passed" },
    @{ Name = "replace";     Script = "scripts\replace.ps1";    Args = "";                                   Gride = $true; Want = "replace: all checks passed" },
    @{ Name = "mapcompare";  Script = "scripts\mapcompare.ps1"; Args = "" },
    @{ Name = "crc32";       Script = "scripts\crc32.ps1";      Args = "" }
)
if ($Quick) { $checks = @($checks | Where-Object { $_.Name -eq "suite" -or $_.Name -eq "selfhost-O0" }) }

if ($Eh -ne "") { $env:CPPPARSE_EH = $Eh; Write-Host "=== exception model: $Eh (CPPPARSE_EH) ===" }
$started = Get-Date
$failed = @(); $notRun = @(); $skipped = @(); $differences = @(); $stopped = $false
Write-Host ("=== harness: {0} check(s){1}, logs in {2} ===" -f $checks.Count, $(if ($Quick) { " (-Quick)" } else { "" }), $logDir)
foreach ($c in $checks) {
    if ($stopped -or ($c.After -and ($failed -contains $c.After -or $notRun -contains $c.After))) { $notRun += $c.Name; Write-Host ("NOT RUN {0}" -f $c.Name); continue }
    if ($c.After -and $skipped -contains $c.After) { $skipped += $c.Name; Write-Host ("SKIPPED {0,-17}           (GRIDE not found: {1} was skipped)" -f $c.Name, $c.After); continue }
    $log = "$logDir\$($c.Name).txt"; $err = "$logDir\$($c.Name).err.txt"
    $argLine = "-NoProfile -ExecutionPolicy Bypass -File `"$Sandbox\$($c.Script)`" $($c.Args)"
    $t = Get-Date
    # the check's own process only: Start-Process -Wait waits for every process it started as well, and cl leaves
    # vctip.exe (MSVC's telemetry) behind, which can outlive the check by many minutes. The handle is taken at once,
    # so that the exit code is kept after the process ends
    # a check's own environment variables (eh-rtself: CPPPARSE_RT), set for its process and removed after it
    if ($c.Env) { foreach ($k in $c.Env.Keys) { Set-Item "env:$k" $c.Env[$k] } }
    $p = Start-Process -FilePath "powershell.exe" -ArgumentList $argLine -WorkingDirectory $Sandbox -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError $err
    $null = $p.Handle
    $p.WaitForExit()
    if ($c.Env) { foreach ($k in $c.Env.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue } }
    $secs = ((Get-Date) - $t).TotalSeconds
    $text = (Get-Content $log -Raw -ErrorAction SilentlyContinue) + (Get-Content $err -Raw -ErrorAction SilentlyContinue)
    if ($null -eq $text) { $text = "" }
    if ($c.Gride -and $p.ExitCode -eq 77 -and $text -match "(?m)^skipped: GRIDE not found") {
        $skipped += $c.Name; Write-Host ("SKIPPED {0,-17} {1,6:N0} s  (GRIDE not found; log {2})" -f $c.Name, $secs, $log); continue
    }
    $why = @()
    if ($p.ExitCode -ne 0) { $why += "exit $($p.ExitCode)" }
    if ($c.Want -and $text -notmatch $c.Want) { $why += "no '$($c.Want)' in the log" }
    if ($c.Refuse -and $text -match $c.Refuse) { $why += "the log has '$($Matches[0])'" }
    $known = [regex]::Matches($text, "(?m)^not under GRIDE's link:").Count
    if ($known -ne 0) { $differences += "$($c.Name) $known" }
    if ($why.Count -eq 0) { Write-Host ("PASS    {0,-17} {1,6:N0} s{2}" -f $c.Name, $secs, $(if ($known -ne 0) { "  ($known not under GRIDE's link, in the log)" } else { "" })) }
    else {
        Write-Host ("FAIL    {0,-17} {1,6:N0} s  ({2}; log {3})" -f $c.Name, $secs, ($why -join ", "), $log)
        ($text -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -Last 12) | ForEach-Object { Write-Host "        | $_" }
        $failed += $c.Name
        if ($c.Stop) { $stopped = $true }
    }
}
Write-Host ("=== harness: {0} passed, {1} failed{2}{3}, {4:N0} min ===" -f ($checks.Count - $failed.Count - $notRun.Count - $skipped.Count), $failed.Count, $(if ($notRun.Count) { ", $($notRun.Count) not run" } else { "" }), $(if ($skipped.Count) { ", $($skipped.Count) skipped" } else { "" }), ((Get-Date) - $started).TotalMinutes)
if ($differences.Count -ne 0) { Write-Host "known differences under GRIDE's link (docs\design-selfhost.md 4.6), by check: $($differences -join ', ')" }
if ($skipped.Count -ne 0) { Write-Host "skipped, which is not passed (GRIDE not found): $($skipped -join ', ')" }
if ($failed.Count -ne 0 -or $notRun.Count -ne 0) { Write-Host "harness FAILED: $($failed -join ', ')"; exit 1 }
if ($skipped.Count -ne 0) {
    if ($RequireGride) { Write-Host "harness FAILED: -RequireGride, and $($skipped.Count) check(s) skipped"; exit 1 }
    Write-Host "harness: every check that ran passed; $($skipped.Count) skipped"
    exit 0
}
Write-Host "harness: all checks passed"
exit 0
