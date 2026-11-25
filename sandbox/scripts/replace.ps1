# replace.ps1 - cppparse's replaceable definitions (__declspec(replaceable), libcpp's eight allocation functions, row
# G17) under Microsoft's link and under GRIDE's link (row P1, docs\design-selfhost.md 4.4 and request G7). A
# replaceable function is a default: it stands unless another object of the program defines the name, and a library
# that also defines the name (the C runtime's operator new) is not asked.
#   1. examples\sample-replace: the default and its caller in one object and in two, in either order, with a library
#      and with an object that define the name again; what the program prints says whose definition was linked
#   2. libcpp's allocation functions: sample-replace\alloc.cpp and sample-lib's newhandler, replacenew, replacepart and
#      replacearray at -O0 and -O2. Each must print what cl's build with its own headers prints, and the map must give
#      none of the eight names to a member of a library
# Every case is linked by both linkers. Under Microsoft's link each must hold, or the script fails. Under GRIDE's link
# a case that does not hold is printed as "not under GRIDE's link: <case>" and counted (GRIDE's link searches the
# libraries for a weak external of the alias kind, which is how a replaceable name is written; request G7); with
# -Strict it fails the script, and "replace.ps1 -Strict passes" is that request's exit test.
# Where GRIDE is not found the script prints "skipped: GRIDE not found" and exits with 77, which is not a pass.
# -Linker ms|gride: the linker of cl's reference builds (toolset.ps1, Build-Program); the cases name both linkers.
param([switch]$Strict, [string]$Compiler = "", [string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Show-Gride
if ($GrideRoot -eq "") { Write-Host "skipped: GRIDE not found"; exit $GrideSkipCode }
$o = "out\replace"
New-Item -ItemType Directory -Force "$Sandbox\$o" | Out-Null
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$ms  = @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib")
$grt = @("$GrideLib\grtstart.lib", "$GrideLib\grtcxx.lib", "$GrideLib\grtc.lib", "$GrideLib\grtwin.lib", "$GrideLib\kernel32.lib")
$linkers = @(@{ Name = "Microsoft's link"; Short = "ms"; Path = $link; Options = @() }, @{ Name = "GRIDE's link"; Short = "gride"; Path = $GrideLink; Options = @("/timestamp:0") })
$failed = 0; $held = @{ ms = 0; gride = 0 }; $notGride = 0
# one case's verdict: counted for its linker; a case GRIDE's link does not hold is a difference, a failure with -Strict
function Verdict([hashtable]$L, [string]$case, [bool]$ok, [string]$got) {
    if ($ok) { $script:held[$L.Short]++; Write-Host "  ok   $($L.Name): $case" }
    elseif ($L.Short -eq "gride" -and -not $Strict) { $script:notGride++; Write-Host "not under GRIDE's link: $case ($got)" }
    else { $script:failed++; Write-Host "  FAIL $($L.Name): $case ($got)" }
}
function LinkRun([hashtable]$L, [string]$exe, [string[]]$inputs, [string[]]$options) {
    if (Test-Path $exe) { Remove-Item $exe }
    $o1 = $L.Options
    $msg = @(& $L.Path /nologo /SUBSYSTEM:CONSOLE @o1 @options "/OUT:$exe" @inputs 2>&1 | ForEach-Object { "$_" })
    if (-not (Test-Path $exe)) { return @{ Linked = $false; Text = ""; Code = -1; Message = (($msg | Select-Object -First 2) -join " | ") } }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $text = & ".\$exe" 2>&1 | Out-String
    $code = $LASTEXITCODE; $ErrorActionPreference = $eap
    return @{ Linked = $true; Text = $text; Code = $code; Message = "" }
}

Write-Host "=== replaceable definitions: the default, its caller, a second definition (examples\sample-replace) ==="
foreach ($n in @("both", "caller", "default")) {
    $msg = & $Exe --compile6 -O0 --obj --msvc-mangling -q -o $o -n $n "..\examples\sample-replace\$n.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$o\${n}6.obj")) { Write-Host "  FAIL --compile6 of sample-replace\$n.cpp"; Write-Host $msg; exit 1 }
}
& $cl /nologo /c /O1 /GS- /Zl "/Fo$o\other.obj" "..\examples\sample-replace\other.cpp" | Out-Null
& "$Msvc\bin\Hostx64\x64\lib.exe" /nologo "/OUT:$o\other.lib" "$o\other.obj" | Out-Null
if (-not (Test-Path "$o\other.lib")) { Write-Host "  FAIL no library of sample-replace\other.cpp"; exit 1 }
$both = "$o\both6.obj"; $caller = "$o\caller6.obj"; $default = "$o\default6.obj"; $otherObj = "$o\other.obj"; $otherLib = "$o\other.lib"
# the inputs in their order on the line, and whose definition must be linked
$cases = @(
    @{ Title = "one object, a library after it";                      In = @($both, $otherLib);             Want = "the default" },
    @{ Title = "one object, the second definition as an object";      In = @($both, $otherObj);             Want = "the other definition" },
    @{ Title = "caller, default, library";                            In = @($caller, $default, $otherLib); Want = "the default" },
    @{ Title = "default, caller, library";                            In = @($default, $caller, $otherLib); Want = "the default" },
    @{ Title = "library, caller, default";                            In = @($otherLib, $caller, $default); Want = "the default" },
    @{ Title = "caller, default, the second definition as an object"; In = @($caller, $default, $otherObj); Want = "the other definition" },
    @{ Title = "the second definition as an object, caller, default"; In = @($otherObj, $caller, $default); Want = "the other definition" }
)
foreach ($L in $linkers) {
    foreach ($c in $cases) {
        $r = LinkRun $L "$o\probe-$($L.Short).exe" ($c.In + $ms) @()
        $got = if ($r.Linked) { $r.Text.Trim() } else { "no exe: $($r.Message)" }
        Verdict $L "$($c.Title): $($c.Want)" ($r.Linked -and $r.Code -eq 0 -and $got -eq $c.Want) "it is $got"
    }
}
# GRIDE's runtime under the same objects (level 3 of row P1): its libraries define nothing named probe_which, the
# library on the line does
$L = $linkers[1]
foreach ($c in @($cases[0], $cases[2])) {
    $r = LinkRun $L "$o\probe-grt.exe" ($c.In + $grt) @("/NODEFAULTLIB")
    $got = if ($r.Linked) { $r.Text.Trim() } else { "no exe: $($r.Message)" }
    Verdict $L "on GRIDE's runtime, $($c.Title): $($c.Want)" ($r.Linked -and $r.Code -eq 0 -and $got -eq $c.Want) "it is $got"
}

Write-Host "=== libcpp's allocation functions: the program's where it defines them, libcpp's otherwise, never a library's ==="
$allocation = @('??2@YAPEAX_K@Z', '??2@YAPEAX_KAEBUnothrow_t@std@@@Z', '??3@YAXPEAX@Z', '??3@YAXPEAXAEBUnothrow_t@std@@@Z', '??_U@YAPEAX_K@Z', '??_U@YAPEAX_KAEBUnothrow_t@std@@@Z', '??_V@YAXPEAX@Z', '??_V@YAXPEAXAEBUnothrow_t@std@@@Z')
# the allocation functions a map gives to a member of a library ("libcmt:new_scalar.obj"), as "name from member"
function FromLibraries([string]$map) {
    $found = @()
    foreach ($l in (Get-Content $map -ErrorAction SilentlyContinue)) {
        if ($l -match '^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+[0-9a-f]{16}\s+(?:f\s+)?(?:i\s+)?(\S+)\s*$' -and $allocation -contains $Matches[1] -and $Matches[2].Contains(":")) { $found += "$($Matches[1]) from $($Matches[2])" }
    }
    return $found
}
& $cl /nologo /c /EHsc /W3 "/Fo$o\cppparse_rt.obj" "..\runtime\cppparse_rt.cpp" | Out-Null
foreach ($lvl in @(0, 2)) {
    $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -o $o -n "iostreamL$lvl" "..\libcpp\iostream.cpp" 2>&1 | Out-String
    if (-not (Test-Path "$o\iostreamL${lvl}6.obj")) { Write-Host "  FAIL --compile6 of libcpp\iostream.cpp at -O$lvl"; Write-Host $msg; exit 1 }
}
foreach ($prog in @("alloc", "newhandler", "replacenew", "replacepart", "replacearray")) {
    $source = if ($prog -eq "alloc") { "..\examples\sample-replace\alloc.cpp" } else { "..\examples\sample-lib\$prog.cpp" }
    $clOut = Build-Program @("/EHsc", "/W3", "/Fo$o\${prog}_cl.obj", $source) @("$o\${prog}_cl.obj") "$o\${prog}_cl.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL cl could not compile $source"; Write-Host $clOut; $failed++; continue }
    $want = & ".\$o\${prog}_cl.exe" 2>&1 | Out-String
    foreach ($lvl in @(0, 2)) {
        $nm = "$prog$lvl"
        if (Test-Path "$o\${nm}6.obj") { Remove-Item "$o\${nm}6.obj" }
        $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling -q -I ..\include -o $o -n $nm $source 2>&1 | Out-String
        if (-not (Test-Path "$o\${nm}6.obj")) { Write-Host "  FAIL --compile6 of $source at -O$lvl"; Write-Host $msg; $failed++; continue }
        foreach ($L in $linkers) {
            $map = "$o\$nm-$($L.Short).map"
            if (Test-Path $map) { Remove-Item $map }
            $r = LinkRun $L "$o\$nm-$($L.Short).exe" (@("$o\${nm}6.obj", "$o\iostreamL${lvl}6.obj", "$o\cppparse_rt.obj") + $ms) @("/MAP:$map")
            if (-not $r.Linked) { Verdict $L "$prog.cpp (-O$lvl) links" $false "no exe: $($r.Message)"; continue }
            Verdict $L "$prog.cpp (-O$lvl) prints what cl's build prints" ($r.Code -eq 0 -and $r.Text -eq $want) "exit code $($r.Code), $(@($r.Text -split "`n" | Where-Object { $_.Trim() }).Count) lines"
            $lib = @(FromLibraries $map)
            Verdict $L "$prog.cpp (-O$lvl): no allocation function from a library, by the map" ((Test-Path $map) -and $lib.Count -eq 0) $(if (Test-Path $map) { $lib -join ", " } else { "no map" })
        }
    }
}

Write-Host "=== summary ==="
Write-Host "Microsoft's link: $($held.ms) case(s) hold; GRIDE's link: $($held.gride) hold, $notGride do not"
if ($notGride -ne 0) { Write-Host "$notGride case(s) not under GRIDE's link (listed above; request G7 of docs\design-selfhost.md)" }
if ($failed -ne 0) { Write-Host "replace: $failed check(s) failed"; exit 1 }
Write-Host "replace: all checks passed$(if ($notGride -ne 0) { " under Microsoft's link" })"
exit 0
