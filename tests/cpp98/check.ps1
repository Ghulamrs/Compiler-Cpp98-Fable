# check.ps1 - compiles every source named in files.txt as C++98, syntax only, and fails if any is rejected.
# MSVC has no C++98 mode, so this needs g++ (MinGW on Windows) or clang++ (macOS) on PATH. clang++ that
# targets MSVC cannot be used: Microsoft's library headers refuse anything older than C++14.
# -pedantic-errors is left off on purpose: it would also reject long long, a separate question.
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$cxx = $null
foreach ($c in 'g++', 'clang++') { if (Get-Command $c -ErrorAction SilentlyContinue) { $cxx = $c; break } }
if (-not $cxx) { Write-Host 'g++ or clang++ is needed on PATH'; exit 2 }
$inc = @('driver', 'common', 'frontend', 'middle', 'backend', 'analyzer', 'analyzer/report') |
    ForEach-Object { '-I' + (Join-Path $root "cppparse/$_") }
$failed = 0
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'files.txt')) {
    $f = $line.Trim()
    if ($f -eq '' -or $f.StartsWith('#')) { continue }
    & $cxx -std=c++98 -fsyntax-only @inc (Join-Path $root $f)
    if ($LASTEXITCODE -ne 0) { Write-Host "FAIL $f"; $failed++ } else { Write-Host "ok   $f" }
}
exit $failed
