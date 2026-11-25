# strict98.ps1 - cppparse's own sources and its runtime against strict C++98: clang's C++98 front end with -pedantic over the units,
# against cppparse's own library headers (include\) and Windows declarations (platform\), as the review of 2026-11-05
# ran it (docs/next-targets.md, rows P4-P9):
#   clang++ --target=x86_64-pc-windows-msvc -std=c++98 -pedantic -fsyntax-only -fno-ms-compatibility
#           -fno-delayed-template-parsing -nostdinc -nostdinc++ -I<each source folder> -Iinclude -Iplatform <unit>
# (with -fno-caret-diagnostics, so that each diagnostic is one line)
# Every diagnostic fails the check except the documented ones:
#   - 'long long' (-Wc++11-long-long) at its two typedef pairs, the one extension of C++98 the sources and headers
#     use (row P4): include\__config (__cppparse_int64, for the headers) and cppparse\common\Int64.hpp (int64, for
#     the sources, which cl compiles against its own headers): 146 of them, two lines for each unit and header
#     that reaches a pair;
#   - the driver's note that -nostdinc++ is unused for this target.
# The units run in parallel (one clang per unit, as many at a time as there are processors); exit code 1 on any
# other diagnostic, with the first ones listed. -Wnontrivial-memcall is one of those since row P9: <vector> moves
# only the arithmetic types and the pointers with memcpy and memmove.
. "$PSScriptRoot\toolset.ps1"
Set-Location $Src
$ErrorActionPreference = "Continue"
$clang = "$VsRoot\VC\Tools\Llvm\x64\bin\clang++.exe"
$out = "$Sandbox\out\strict98"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item
$inc = @(); foreach ($sd in $SourceDirs) { $inc += "-I`"cppparse\$sd`"" }
$common = "--target=x86_64-pc-windows-msvc -std=c++98 -pedantic -fsyntax-only -fno-ms-compatibility -fno-delayed-template-parsing -nostdinc -nostdinc++ $($inc -join ' ') -Iinclude -Iplatform -fno-caret-diagnostics"
$allowed = @(
    @{ Flag = "-Wc++11-long-long";   File = '(^|[\\/])(include[\\/]__config|cppparse[\\/]common[\\/]Int64\.hpp)$' }
)
$started = Get-Date
$slots = [Math]::Max(1, [Environment]::ProcessorCount)
$running = New-Object System.Collections.ArrayList
$next = 0
# the runtime too (row P1 stage 2, docs\design-selfhost.md 4.3): C++98 but for its documented extensions, which it spells
# through macros clang takes (CPPPARSE_THREAD, __cppparse_align) or through the 64-bit typedefs of include\__config
$units = @($OwnSources) + @(Get-Item "$Src\runtime\cppparse_rt.cpp")
while ($next -lt $units.Count -or $running.Count -gt 0) {
    while ($next -lt $units.Count -and $running.Count -lt $slots) {
        $u = $units[$next]; $next++
        $p = Start-Process -FilePath $clang -ArgumentList "$common `"$($u.FullName)`"" -WorkingDirectory $Src -NoNewWindow -PassThru -RedirectStandardOutput "$out\$($u.BaseName).out" -RedirectStandardError "$out\$($u.BaseName).txt"
        $null = $p.Handle
        $null = $running.Add($p)
    }
    $null = $running[0].WaitForExit(250)
    foreach ($p in @($running)) { if ($p.HasExited) { $running.Remove($p) } }
}
$bad = @(); $known = @{}; $clean = 0
foreach ($u in $units) {
    $unitBad = 0
    foreach ($line in @(Get-Content "$out\$($u.BaseName).txt" -ErrorAction SilentlyContinue)) {
        if ($line -match "^clang\S*: warning: argument unused during compilation: '-nostdinc\+\+'") { continue }
        if ($line -notmatch '^(.+?):\d+:\d+: (warning|error|fatal error): .*?(\[(-W[^\],]+)[^\]]*\])?$') {
            if ($line -match '^\S.*\b(warning|error):') { $bad += "$($u.Name): $line"; $unitBad++ }
            continue
        }
        $file = $Matches[1]; $flag = $Matches[4]; $ok = $false
        foreach ($a in $allowed) { if ($flag -eq $a.Flag -and $file -match $a.File) { $ok = $true } }
        if ($ok) { $known[$flag] = 1 + $known[$flag] } else { $bad += "$($u.Name): $line"; $unitBad++ }
    }
    if ($unitBad -eq 0) { $clean++ }
}
$secs = ((Get-Date) - $started).TotalSeconds
$knownText = ($known.Keys | Sort-Object | ForEach-Object { "$($known[$_]) $_" }) -join ", "
Write-Host ("strict98: {0} of {1} units have no diagnostic but the documented ones ({2}; {3:N0} s)" -f $clean, $units.Count, $(if ($knownText) { $knownText } else { "none" }), $secs)
if ($bad.Count -ne 0) {
    Write-Host "strict98: $($bad.Count) other diagnostic(s), the first 20:"
    $bad | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }
    Write-Host "strict98 FAILED"
    exit 1
}
Write-Host "strict98: passed"
exit 0
