# eh.ps1 - unwind tables and exceptions of cppparse-compiled code (examples\sample-eh, driver and ml64 entry in
# examples\sample-eh-driver).
#   reference: probe.cpp and probe-mix.cpp compiled by cl /O2 /EHa (C++ exceptions of MSVC)
#   then for each exception model (sjlj: setjmp frames, table: --eh=table, and both mixed: probe.cpp with one model,
#   probe-mix.cpp with the other) and each of -O0..-O3, the two probe files compiled by cppparse --compile6 --obj
# Each program must print ALL OK, and its output must equal the reference:
#   unwind walk: from inside nested frames, RtlVirtualUnwind must find every frame and restore the callee-saved
#   registers (rbx rbp rsi rdi r12-r15, xmm6-xmm15) that an ml64 function set before calling in
#   exceptions: the results of the eh* functions and the number of destructors run
# Exit code 1 when anything fails.
param([string]$Compiler = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$ml64 = "$Msvc\bin\Hostx64\x64\ml64.exe"
$out = "out\eh"
$src = "..\examples\sample-eh"
$drv = "..\examples\sample-eh-driver"
New-Item -ItemType Directory -Force $out | Out-Null
$failed = 0
function Report([string]$name, [bool]$ok, [string]$detail = "") {
    if ($ok) { Write-Host "  ok   $name" } else { Write-Host "  FAIL $name"; if ($detail) { Write-Host $detail }; $script:failed++ }
}
function Normalize([string]$t) { return ($t -replace "`r`n", "`n").TrimEnd() }

Write-Host "=== exceptions and unwind tables (sample-eh) ==="
$msg = & $cl /nologo /c /EHsc /W4 /WX "/Fo$out\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" 2>&1 | Out-String
Report "runtime compiled (/W4 /WX)" ($LASTEXITCODE -eq 0) $msg
$msg = & $ml64 /nologo /c "/Fo$out\entry.obj" "$drv\entry.asm" 2>&1 | Out-String
Report "entry.asm assembled by ml64" ($LASTEXITCODE -eq 0) $msg

New-Item -ItemType Directory -Force "$out\cl" | Out-Null
# /EHa: cl 19.52 (preview) with /EHsc or /EHs at /O1 and above crashes on a throw out of a recursion whose frames
# have destructors (deep() below; a 6-line program reproduces it, clang -O2 and cl /EHa run it correctly)
# /Zc:implicitNoexcept-: C++98's destructors, which may throw (ehDtorThrow, ehArrays)
$msg = & $cl /nologo /EHa /Zc:implicitNoexcept- /W4 /O2 "/Fo$out\cl\" "/Fe$out\cl\eh.exe" "$drv\driver-eh.cpp" "$src\probe.cpp" "$src\probe-mix.cpp" "$out\entry.obj" "$out\cppparse_rt.obj" 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { Report "cl reference build" $false $msg; exit 1 }
$reference = Normalize (& "$out\cl\eh.exe" 2>&1 | Out-String)
Report "cl reference build runs, ALL OK" ($LASTEXITCODE -eq 0 -and $reference -match 'ALL OK') $reference

# model of probe.cpp, model of probe-mix.cpp
$models = [ordered]@{ "sjlj" = @("--eh=sjlj", "--eh=sjlj"); "table" = @("--eh=table", "--eh=table"); "table+sjlj" = @("--eh=table", "--eh=sjlj"); "sjlj+table" = @("--eh=sjlj", "--eh=table") }
foreach ($m in $models.Keys) {
    foreach ($lvl in 0, 1, 2, 3) {
        $dir = "$out\$($m -replace '\+', '-')-O$lvl"
        New-Item -ItemType Directory -Force $dir | Out-Null
        $ok = $true
        foreach ($k in 0, 1) {
            $name = @("probe", "probe-mix")[$k]
            $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling $models[$m][$k] -q -I ..\include -I .. -o $dir -n $name "$src\$name.cpp" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$dir\${name}6.obj")) { Report "$m -O$lvl compile $name" $false $msg; $ok = $false }
        }
        if (-not $ok) { continue }
        $msg = & $cl /nologo /EHsc /W4 /WX "/Fo$dir\" "/Fe$dir\eh.exe" "$drv\driver-eh.cpp" "$dir\probe6.obj" "$dir\probe-mix6.obj" "$out\entry.obj" "$out\cppparse_rt.obj" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Report "$m -O$lvl link" $false $msg; continue }
        $p = Start-Process -FilePath "$dir\eh.exe" -NoNewWindow -PassThru -RedirectStandardOutput "$dir\eh.out" -RedirectStandardError "$dir\eh.err"
        $null = $p.Handle
        if (-not $p.WaitForExit(60000)) { $p.Kill(); Report "$m -O$lvl" $false "TIMEOUT"; continue }
        $run = Normalize ((Get-Content "$dir\eh.out" -Raw) + (Get-Content "$dir\eh.err" -Raw))
        $same = $run -eq $reference
        $detail = ""
        if (-not $same) {
            $a = $run -split "`n"; $b = $reference -split "`n"
            for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($a[$i] -ne $b[$i]) { $detail = "       line $($i + 1): got '$($a[$i])', cl '$($b[$i])'"; break } }
        }
        Report "$m -O${lvl}: output equals cl's (exit $($p.ExitCode))" ($p.ExitCode -eq 0 -and $same) $detail
    }
}

# whole programs that must call terminate; the standard output and the exit code (abort) must equal cl's:
#   terminate.cpp: an exception leaving a destructor run by the stack unwinding
#   terminate-catch.cpp: a copy constructor that throws while it initializes a by-value catch parameter
#   terminate-handler.cpp: the destructor of a by-value parameter that throws while an exception leaves its handler
foreach ($tn in "terminate", "terminate-catch", "terminate-handler") {
    $tdir = "$out\$tn"
    New-Item -ItemType Directory -Force "$tdir\cl" | Out-Null
    $msg = & $cl /nologo /EHsc /Zc:implicitNoexcept- /O2 "/Fo$tdir\cl\" "/Fe$tdir\cl\$tn.exe" "$src\$tn.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "${tn}: cl reference build" $false $msg; continue }
    $tref = Normalize (& "$tdir\cl\$tn.exe" 2>$null | Out-String)
    $tcode = $LASTEXITCODE
    foreach ($m in "--eh=table", "--eh=sjlj") {
        foreach ($lvl in 0, 2) {
            $d = "$tdir\$($m.Substring(5))-O$lvl"
            New-Item -ItemType Directory -Force $d | Out-Null
            $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling $m -q -I ..\include -I .. -o $d -n $tn "$src\$tn.cpp" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Report "$tn $m -O$lvl compile" $false $msg; continue }
            $msg = & $link /nologo "/OUT:$d\$tn.exe" "$d\${tn}6.obj" "$out\cppparse_rt.obj" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Report "$tn $m -O$lvl link" $false $msg; continue }
            $run = Normalize (& "$d\$tn.exe" 2>$null | Out-String)
            Report "$tn $m -O${lvl}: output and exit code equal cl's ($LASTEXITCODE)" ($run -eq $tref -and $LASTEXITCODE -eq $tcode -and $tcode -ne 0) "       got '$run' exit $LASTEXITCODE, cl '$tref' exit $tcode"
        }
    }
}
if ($failed) { Write-Host "eh: $failed check(s) failed"; exit 1 }
Write-Host "eh: all checks passed"
exit 0
