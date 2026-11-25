# failnew.ps1 - allocation failures injected into the self-built compiler (row M7, docs\design-cl-exceptions.md).
# The compiler's own sources and libcpp are compiled by sandbox\cppparse.exe at -O0 for each exception model (table and
# cl), and linked with examples\sample-ehx\failnew.cpp, whose operator new throws std::bad_alloc at the k-th
# allocation (CPPPARSE_FAILNEW). Each such compiler then compiles a sample with -j 1 for a sweep of k: the exception
# unwinds through whatever frames are active at that moment, up to the driver's handler. Every run must end either
# with "cppparse: out of memory" and exit code 4, or (k beyond the run's allocations) as the run without a failure
# ends; no run may end with any other exit code (an access violation, a fail-fast of the runtime).
# Not part of harness.ps1: about ten minutes. Exit code 1 when a run ends otherwise. -Linker ms|gride: the linker of
# the compilers (toolset.ps1, Link-Program).
param([string]$Compiler = "", [string[]]$Models = @("table", "cl"), [int]$Steps = 60, [string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Assert-Linker
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$failed = 0
$sample = "..\examples\sample-opt\opt.cpp"
foreach ($model in $Models) {
    $dir = "out\failnew\$model"
    New-Item -ItemType Directory -Force $dir | Out-Null
    Write-Host "=== allocation failures, --eh=$model ==="
    $objs = @()
    $units = @($OwnSources | ForEach-Object { $_.FullName }) + @("$Src\libcpp\iostream.cpp", "$Src\examples\sample-ehx\failnew.cpp")
    foreach ($u in $units) {
        $name = [IO.Path]::GetFileNameWithoutExtension($u)
        $msg = & $Exe --compile6 -O0 --obj --msvc-mangling "--eh=$model" -q @OwnIncludes -o $dir -n $name $u 2>&1 | Out-String
        if (-not (Test-Path "$dir\${name}6.obj")) { Write-Host "  FAIL compile $name"; Write-Host $msg; $failed++ }
        $objs += "$dir\${name}6.obj"
    }
    $null = Build-Runtime "$dir\cppparse_rt.obj"   # cl's, or cppparse's with CPPPARSE_RT=cppparse (toolset.ps1)
    $msg = Link-Program ($objs + @("$dir\cppparse_rt.obj")) "$dir\cppparse-failnew.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL link"; Write-Host $msg; $failed++; continue }
    function RunAt([long]$k) {
        $env:CPPPARSE_FAILNEW = "$k"
        $p = Start-Process -FilePath "$dir\cppparse-failnew.exe" -ArgumentList "--compile6 -O0 -q -j 1 -I ..\include -o $dir -n probe $sample" -NoNewWindow -PassThru -RedirectStandardOutput "$dir\run.out" -RedirectStandardError "$dir\run.err"
        $null = $p.Handle
        if (-not $p.WaitForExit(120000)) { $p.Kill(); return @(-1, "TIMEOUT") }
        $env:CPPPARSE_FAILNEW = $null
        return @($p.ExitCode, ((Get-Content "$dir\run.err" -Raw) + ""))
    }
    $clean = RunAt 0
    if ($clean[0] -ne 0) { Write-Host "  FAIL the run without a failure ends with exit code $($clean[0])"; $failed++; continue }
    # the number of allocations of the run: the smallest k at which nothing fails any more (by doubling, then bisection)
    [long]$hi = 1024
    while ((RunAt $hi)[0] -ne 0 -and $hi -lt 2000000000) { $hi *= 2 }
    [long]$lo = [long]($hi / 2)
    while ($hi - $lo -gt 1) { [long]$mid = [long](($lo + $hi) / 2); if ((RunAt $mid)[0] -eq 0) { $hi = $mid } else { $lo = $mid } }
    $total = $lo
    $bad = 0; $oom = 0
    for ($i = 1; $i -le $Steps; $i++) {
        [long]$k = [long]([Math]::Max(1, [Math]::Floor($total * $i / $Steps)))
        $r = RunAt $k
        if ($r[0] -eq 4 -and $r[1] -match "cppparse: out of memory") { $oom++ }
        else { $bad++; Write-Host ("  FAIL allocation {0} of {1}: exit code 0x{2:X8} {3}" -f $k, $total, $r[0], $r[1].Trim()) }
    }
    if ($bad -eq 0) { Write-Host "  ok   --eh=${model}: $oom runs, each with one of the run's $total allocations failing, all end with 'out of memory' and exit code 4" }
    else { $failed += $bad }
}
if ($failed) { Write-Host "failnew: $failed check(s) failed"; exit 1 }
Write-Host "failnew: all checks passed"
exit 0
