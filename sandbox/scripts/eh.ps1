# eh.ps1 - unwind tables and exceptions of cppparse-compiled code (examples\sample-eh, driver and ml64 entry in
# examples\sample-eh-driver). For each of -O0..-O3: probe.cpp is compiled by cppparse --compile6 --obj, entry.asm
# by ml64, the driver and the runtime by cl; the program must print ALL OK.
#   unwind walk: from inside nested cppparse frames, RtlVirtualUnwind must find every frame and restore the
#   callee-saved registers (rbx rbp rsi rdi r12-r15, xmm6-xmm15) that an ml64 function set before calling in
# Exit code 1 when anything fails.
param([string]$Compiler = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$ml64 = "$Msvc\bin\Hostx64\x64\ml64.exe"
$out = "out\eh"
New-Item -ItemType Directory -Force $out | Out-Null
$failed = 0
function Report([string]$name, [bool]$ok, [string]$detail = "") {
    if ($ok) { Write-Host "  ok   $name" } else { Write-Host "  FAIL $name"; if ($detail) { Write-Host $detail }; $script:failed++ }
}

Write-Host "=== exceptions and unwind tables (sample-eh) ==="
$msg = & $cl /nologo /c /EHsc /W3 "/Fo$out\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" 2>&1 | Out-String
Report "runtime compiled" ($LASTEXITCODE -eq 0) $msg
$msg = & $ml64 /nologo /c "/Fo$out\entry.obj" "..\examples\sample-eh-driver\entry.asm" 2>&1 | Out-String
Report "entry.asm assembled by ml64" ($LASTEXITCODE -eq 0) $msg
foreach ($lvl in 0, 1, 2, 3) {
    $dir = "$out\O$lvl"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -I .. -o $dir -n probe "..\examples\sample-eh\probe.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$dir\probe6.obj")) { Report "-O$lvl compile" $false $msg; continue }
    $msg = & $cl /nologo /EHsc /W4 /WX "/Fo$dir\" "/Fe$dir\eh.exe" "..\examples\sample-eh-driver\driver-eh.cpp" "$dir\probe6.obj" "$out\entry.obj" "$out\cppparse_rt.obj" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "-O$lvl link" $false $msg; continue }
    $run = & "$dir\eh.exe" 2>&1 | Out-String
    Report "-O${lvl}: $((($run -split "`n") | Where-Object { $_ -match 'unwind walk' }) -join '' )".Trim() ($LASTEXITCODE -eq 0 -and $run -match 'ALL OK') $run
}
if ($failed) { Write-Host "eh: $failed check(s) failed"; exit 1 }
Write-Host "eh: all checks passed"
exit 0
