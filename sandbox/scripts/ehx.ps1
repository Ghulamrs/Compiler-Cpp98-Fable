# ehx.ps1 - exceptions across compilers (examples\sample-ehx, row M7, docs\design-cl-exceptions.md). Called by eh.ps1;
# exit code 1 when anything fails. main.cpp is always cl's; each group's source is compiled as side A and as side B.
#
# 1. The crossing stop. cppparse's own models (--eh=table, --eh=sjlj) and cl's cannot unwind each other's frames. Where
#    the runtime sees an exception of one reach a frame of the other that has objects to destroy or handlers to try,
#    it stops with a message instead of passing the frame by (cppparse_rt.cpp crossingStop). cross.cpp, side A from cl
#    (with __CxxFrameHandler4 tables, cl's default, and with __CxxFrameHandler3's), side B from cppparse:
#      path 0 (all in A) prints what the all-cl build prints
#      --eh=table: paths 1 to 6 end with the message and a non-zero exit code
#      --eh=sjlj: paths 2, 4, 6 (cppparse throws into cl's frames) do; a setjmp frame has no unwind handler, so a cl
#      exception that passes one (paths 3, 5) cannot be seen, and path 1 is cl's own "no handler" stop
#
# 2. cl's own model, cppparse --eh=cl: its frames and throws are cl's to the runtime. The groups of the sample that
#    the model's stages cover so far ($Groups), each source compiled as both sides by cl /O2 /EHsc and by cppparse
#    --eh=cl at -O0..-O3. The reference has both sides from cl; the three other builds at each level (A from cppparse
#    and B from cl, the reverse, both from cppparse) must print what the reference prints.
#      throw    every kind of thrown type of each side's throwers against cl's handlers in main.cpp
#      unwind   main.cpp throws through each side's frames that own objects and catches: plain objects, temporaries
#               of ?: and &&, arrays partly built, a constructor that throws in a new-expression and after its
#               members, a throwing static initialiser, a cleanup with floating-point registers, a loop
#      catch    each side's handlers for the other side's throws (by value, reference, pointer, without a name, a
#               rethrow through the other side) and their shapes: an exception leaving a handler into the try around,
#               a try in a handler, two exceptions alive, a loop, return and goto out of a handler, a class result, a
#               function-try-block, a throw whose copy constructor throws
#      cross    the seven paths of section 1, which here must all run
#      state    std::uncaught_exception in a destructor of one side while the other's exception unwinds it; the
#               standard exception classes of each side's library caught by the other by reference; and, each a run
#               of its own, a terminate handler one side installs, reached when the other side's throw finds no handler
#    The same once more at -O2 with the program linked three other ways (/MD: the DLL runtime, __CxxFrameHandler3 from
#    VCRUNTIME140.dll; /DEBUG /INCREMENTAL: incremental-link thunks; /OPT:REF,ICF), and once with both sides using
#    cppparse's library (cl /X with cppparse's headers), where the standard exceptions are caught by value as well.
#    link refuses an --eh=cl object beside an --eh=table one (/FAILIFMISMATCH in each object's .drectve).
# -Linker ms|gride: the linker of every program (toolset.ps1, Link-Program). Under gride two cases stay on Microsoft's
# link, each printed "not under GRIDE's link: ..." (docs\design-selfhost.md 4.6): the /INCREMENTAL flavour, whose thunks
# GRIDE's link does not make, and the mixed-family link, which GRIDE's link does not refuse (request G5).
param([string]$Compiler = "", [string[]]$Groups = @("throw", "unwind", "catch", "cross", "state"), [string]$Linker = "")
. "$PSScriptRoot\toolset.ps1"
if ($Compiler -ne "") { $Exe = (Resolve-Path $Compiler).Path }
Assert-Linker
Set-Location $Sandbox
$ErrorActionPreference = "Continue"
$out = "out\ehx"
$src = "..\examples\sample-ehx"
New-Item -ItemType Directory -Force "$out\cl" | Out-Null
$failed = 0
function Report([string]$name, [bool]$ok, [string]$detail = "") {
    if ($ok) { Write-Host "  ok   $name" } else { Write-Host "  FAIL $name"; if ($detail) { Write-Host $detail }; $script:failed++ }
}
function Normalize([string]$t) { if ($null -eq $t) { return "" }; return ($t -replace "`r`n", "`n").TrimEnd() }
# runs a program; returns its exit code, its standard output and its standard error
function RunProgram([string]$prog, [string]$arguments) {
    $p = Start-Process -FilePath $prog -ArgumentList $arguments -NoNewWindow -PassThru -RedirectStandardOutput "$prog.out" -RedirectStandardError "$prog.err"
    $null = $p.Handle
    if (-not $p.WaitForExit(60000)) { $p.Kill(); return @(-1, "TIMEOUT", "") }
    return @($p.ExitCode, (Normalize (Get-Content "$prog.out" -Raw)), (Normalize (Get-Content "$prog.err" -Raw)))
}

Write-Host "=== exceptions across compilers (sample-ehx) ==="
$msg = Build-Runtime "$out\cppparse_rt.obj" @("/EHsc", "/W4", "/WX")   # cl's, or cppparse's with CPPPARSE_RT=cppparse (toolset.ps1)
Report "runtime compiled (/W4 /WX)" ($LASTEXITCODE -eq 0) $msg
$msg = & $cl /nologo /c /EHsc /O2 /W3 "/Fo$out\main.obj" "$src\main.cpp" 2>&1 | Out-String
Report "main.cpp compiled by cl" ($LASTEXITCODE -eq 0) $msg
# every group's source as both sides from cl /O2 /EHsc: the reference of section 2, and what a build links for the
# groups it does not test (main.cpp names every group's functions)
$AllGroups = @("cross", "throw", "unwind", "catch", "state")
foreach ($g in $AllGroups) {
    foreach ($side in "A", "B") {
        $msg = & $cl /nologo /c /EHsc /O2 /W3 "/DSIDE=$side" "/Fo$out\cl\$g-$side.obj" "$src\$g.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Report "cl $g.cpp side $side" $false $msg; exit 1 }
    }
}
function ClObjects([string[]]$except) {
    $objs = @()
    foreach ($g in $AllGroups) { if ($except -notcontains $g) { $objs += "$out\cl\$g-A.obj"; $objs += "$out\cl\$g-B.obj" } }
    return $objs
}

# ---- 1. the crossing stop ----
$stopText = "cppparse runtime: an exception thrown by"
foreach ($tables in "FH4", "FH3") {
    $clOpt = @("/EHsc"); if ($tables -eq "FH3") { $clOpt += "/d2FH4-" }
    $dir = "$out\stop-$tables"
    New-Item -ItemType Directory -Force $dir | Out-Null
    foreach ($side in "A", "B") {
        $msg = & $cl /nologo /c /O2 /W3 @clOpt "/DSIDE=$side" "/Fo$dir\cross-$side.obj" "$src\cross.cpp" 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { Report "cl cross.cpp side $side ($tables)" $false $msg; exit 1 }
    }
    $rest = @(ClObjects @("cross"))
    $msg = Link-Program (@("$out\main.obj", "$dir\cross-A.obj", "$dir\cross-B.obj") + $rest + @("$out\cppparse_rt.obj")) "$dir\ref.exe" | Out-String
    if ($LASTEXITCODE -ne 0) { Report "reference link ($tables)" $false $msg; exit 1 }
    $ref0 = RunProgram "$dir\ref.exe" "cross 0"
    Report "cl's tables ${tables}: the all-cl build runs path 0" ($ref0[0] -eq 0 -and $ref0[1] -match "(?m)^done$") $ref0[1]
    foreach ($model in "table", "sjlj") {
        foreach ($lvl in 0, 2) {
            $name = "cross-B-$model-O$lvl"
            $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling "--eh=$model" -q -D "SIDE=B" -I ..\include -I .. -o $dir -n $name "$src\cross.cpp" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$dir\${name}6.obj")) { Report "$model -O$lvl compile cross.cpp" $false $msg; continue }
            $prog = "$dir\$model-O$lvl.exe"
            $msg = Link-Program (@("$out\main.obj", "$dir\cross-A.obj", "$dir\${name}6.obj") + $rest + @("$out\cppparse_rt.obj")) $prog | Out-String
            if ($LASTEXITCODE -ne 0) { Report "$model -O$lvl link" $false $msg; continue }
            $r = RunProgram $prog "cross 0"
            Report "cl ($tables) + cppparse --eh=$model -O${lvl}: path 0 as the all-cl build" ($r[0] -eq 0 -and $r[1] -eq $ref0[1]) "       got '$($r[1])' exit $($r[0])"
            $paths = if ($model -eq "table") { 1..6 } else { 2, 4, 6 }
            $bad = @()
            foreach ($path in $paths) {
                $r = RunProgram $prog "cross $path"
                if ($r[0] -eq 0 -or $r[2] -notmatch [regex]::Escape($stopText)) { $bad += "path ${path}: exit $($r[0]), output '$($r[1])', message '$($r[2])'" }
            }
            Report "cl ($tables) + cppparse --eh=$model -O${lvl}: paths $($paths -join ', ') stop with the runtime's message" ($bad.Count -eq 0) ("       " + ($bad -join "`n       "))
        }
    }
}

# ---- 2. cl's own model ----
# cl's side twice: /EHsc, what cl-compiled code usually is, and /EHa (with main.cpp compiled so too). cl 19.52 /EHsc
# miscompiles two exceptions alive in one frame at /O1 and up (catch.cpp catchTwoLive: a 10-line program of it reads
# through a null pointer; /EHa and /Od run it), so under /EHsc cl's copy of that one function is a stand-in and only
# the /EHa pass has cl run it; cppparse's copy is the real code in both
# flavour: how cl's side and main.cpp are compiled and the program is linked
#   EHsc, EHa        cl /EHsc or /EHa
#   MD               cl /EHsc /MD, the runtime too: the DLL C runtime
#   DEBUG, OPT       cl /EHsc, linked /DEBUG /INCREMENTAL or /OPT:REF,ICF
#   ONELIB           cl /EHsc, state.cpp of both sides against cppparse's library (cl /X, cppparse's headers)
function ClModel([string]$flavour, [int[]]$levels) {
    $dir = "$out\cl-$flavour"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $clOpt = @("/EHsc"); $linkOpt = @(); $cpOpt = @(); $rt = "$out\cppparse_rt.obj"
    # (the libraries of the link: Microsoft's static C runtime, or with /MD none but those the objects name)
    $linkLibs = $null; $linkWith = ""
    if ($flavour -eq "EHa") { $clOpt = @("/EHa", "/DEHX_EHA") }
    if ($flavour -eq "MD") {
        $clOpt = @("/EHsc", "/MD"); $rt = "$dir\cppparse_rt.obj"; $linkLibs = @()
        $linkOpt = @("legacy_stdio_definitions.lib")   # cppparse's object of the runtime calls fprintf, which the DLL runtime has inline only
        $msg = Build-Runtime $rt @("/EHsc", "/MD", "/W4", "/WX")
        if ($LASTEXITCODE -ne 0) { Report "cl /MD runtime" $false $msg; return }
    }
    if ($flavour -eq "DEBUG") {
        $linkOpt = @("/DEBUG", "/INCREMENTAL")
        if ($Linker -eq "gride") { $linkWith = "ms"; Write-NotUnderGride "ehx.ps1, the /DEBUG /INCREMENTAL flavour is linked by Microsoft's link: GRIDE's link accepts /INCREMENTAL and makes no thunks, so the case would test nothing" }
    }
    if ($flavour -eq "OPT") { $linkOpt = @("/OPT:REF,ICF", "/INCREMENTAL:NO") }
    if ($flavour -eq "ONELIB") { $clOpt = @("/EHsc", "/DEHX_ONE_LIBRARY"); $cpOpt = @("-D", "EHX_ONE_LIBRARY") }
    $msg = & $cl /nologo /c /O2 /W3 @clOpt "/Fo$dir\main.obj" "$src\main.cpp" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Report "cl /$flavour main.cpp" $false $msg; return }
    $clObjs = @{ "A" = @(); "B" = @() }
    foreach ($g in $AllGroups) {
        foreach ($side in "A", "B") {
            # (one library: cl sees cppparse's headers alone for state.cpp, as mix.ps1 compiles the compiler's own sources)
            $lib = @(); if ($flavour -eq "ONELIB" -and $g -eq "state") { $lib = @("/X", "/I..\include") }
            $msg = & $cl /nologo /c /O2 /W3 @clOpt @lib "/DSIDE=$side" "/Fo$dir\$g-$side.obj" "$src\$g.cpp" 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) { Report "cl /$flavour $g.cpp side $side" $false $msg; return }
            if ($Groups -contains $g) { $clObjs[$side] += "$dir\$g-$side.obj" }
        }
    }
    $rest = @(); foreach ($g in $AllGroups) { if ($Groups -notcontains $g) { $rest += "$dir\$g-A.obj"; $rest += "$dir\$g-B.obj" } }
    $msg = Link-Program (@("$dir\main.obj") + @($clObjs["A"]) + @($clObjs["B"]) + $rest + @($rt)) "$dir\ehx.exe" $linkOpt $linkLibs $linkWith | Out-String
    if ($LASTEXITCODE -ne 0) { Report "cl /$flavour reference link" $false $msg; return }
    $reference = RunProgram "$dir\ehx.exe" ($Groups -join " ")
    Report "--eh=cl against cl ${flavour}: the reference (both sides cl; $($Groups -join ', ')) runs to its end" ($reference[0] -eq 0 -and $reference[1] -match "(?m)^done$") ("       exit $($reference[0]), last line '" + ($reference[1] -split "`n")[-1] + "'")
    foreach ($lvl in $levels) {
        $cdir = "$out\O$lvl$(if ($flavour -eq 'ONELIB') { '-onelib' })"
        New-Item -ItemType Directory -Force $cdir | Out-Null
        $cpObjs = @{ "A" = @(); "B" = @() }
        $ok = $true
        foreach ($g in $Groups) {
            foreach ($side in "A", "B") {
                $msg = & $Exe --compile6 "-O$lvl" --obj --msvc-mangling --eh=cl -q -D "SIDE=$side" @cpOpt -I ..\include -I .. -o $cdir -n "$g-$side" "$src\$g.cpp" 2>&1 | Out-String
                if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$cdir\$g-${side}6.obj")) { Report "--eh=cl -O$lvl compile $g.cpp side $side" $false $msg; $ok = $false }
                $cpObjs[$side] += "$cdir\$g-${side}6.obj"
            }
        }
        if (-not $ok) { continue }
        foreach ($combo in "cppparse+cl", "cl+cppparse", "cppparse+cppparse") {
            $a = if ($combo.StartsWith("cppparse")) { $cpObjs["A"] } else { $clObjs["A"] }
            $b = if ($combo.EndsWith("cppparse")) { $cpObjs["B"] } else { $clObjs["B"] }
            $prog = "$cdir\ehx-$flavour-$($combo -replace '\+', '-').exe"
            $msg = Link-Program (@("$dir\main.obj") + @($a) + @($b) + $rest + @($rt)) $prog $linkOpt $linkLibs $linkWith | Out-String
            if ($LASTEXITCODE -ne 0) { Report "--eh=cl A+B = $combo -O$lvl link (cl $flavour)" $false $msg; continue }
            $run = RunProgram $prog ($Groups -join " ")
            $detail = ""
            if ($run[0] -ne 0 -or $run[1] -ne $reference[1]) {
                $x = $run[1] -split "`n"; $y = $reference[1] -split "`n"
                $detail = "       exit $($run[0]) $($run[2])"
                for ($i = 0; $i -lt [Math]::Max($x.Count, $y.Count); $i++) { if ($x[$i] -ne $y[$i]) { $detail += "`n       line $($i + 1): got '$($x[$i])'`n               cl  '$($y[$i])'"; break } }
            }
            Report "--eh=cl A+B = $combo -O$lvl against cl ${flavour}: output equals the all-cl build's" ($run[0] -eq 0 -and $run[1] -eq $reference[1]) $detail
            if ($Groups -contains "state") {
                # a terminate handler of one side, reached from the other's throw that nothing catches
                $bad = @()
                foreach ($who in "A", "B") {
                    $t = RunProgram $prog "terminate $who"
                    if ($t[0] -ne 7 -or $t[1] -notmatch "(?m)^terminate handler$") { $bad += "handler of ${who}: exit $($t[0]), output '$($t[1])' $($t[2])" }
                }
                Report "--eh=cl A+B = $combo -O$lvl against cl ${flavour}: a side's terminate handler runs for the other side's unhandled throw" ($bad.Count -eq 0) ("       " + ($bad -join "`n       "))
            }
        }
    }
}
ClModel "EHsc" @(0, 1, 2, 3)
ClModel "EHa" @(2)
ClModel "MD" @(2)
ClModel "DEBUG" @(2)
ClModel "OPT" @(2)
ClModel "ONELIB" @(2)
# the two families of models do not link together
$msg = & $Exe --compile6 -O0 --obj --msvc-mangling --eh=table -q -D "SIDE=B" -I ..\include -I .. -o "$out\O0" -n "cross-B-table" "$src\cross.cpp" 2>&1 | Out-String
$mismatchWith = ""
if ($Linker -eq "gride") { $mismatchWith = "ms"; Write-NotUnderGride "ehx.ps1, the mixed-family link that must fail with LNK2038 is linked by Microsoft's link: GRIDE's link does not enforce /FAILIFMISMATCH (request G5)" }
$msg = Link-Program @("$out\cl-EHsc\main.obj", "$out\O0\cross-A6.obj", "$out\O0\cross-B-table6.obj", "$out\cl-EHsc\throw-A.obj", "$out\cl-EHsc\throw-B.obj", "$out\cl-EHsc\unwind-A.obj", "$out\cl-EHsc\unwind-B.obj", "$out\cl-EHsc\catch-A.obj", "$out\cl-EHsc\catch-B.obj", "$out\cl-EHsc\state-A.obj", "$out\cl-EHsc\state-B.obj", "$out\cppparse_rt.obj") "$out\O0\mismatch.exe" @() $null $mismatchWith | Out-String
Report "link refuses an --eh=cl object beside an --eh=table one (LNK2038: cppparse_eh)" ($LASTEXITCODE -ne 0 -and $msg -match "LNK2038" -and $msg -match "cppparse_eh") $msg
# the ml64 path cannot carry the model's tables: the driver refuses it, and the listing stops ml64
$msg = & $Exe --compile6 --assemble --eh=cl -q -I ..\include -o $out -n refused "$src\throw.cpp" 2>&1 | Out-String
Report "--eh=cl with --assemble is refused" ($LASTEXITCODE -ne 0 -and $msg -match "--eh=cl needs cppparse's own object writer") $msg
Report "an --eh=cl listing stops ml64 (.ERR)" ([bool](Select-String -Path "$out\O0\throw-A6.asm" -Pattern "^\.ERR <--eh=cl" -Quiet)) ""

if ($failed) { Write-Host "ehx: $failed check(s) failed"; exit 1 }
Write-Host "ehx: all checks passed$(Get-KnownDifferences)"
exit 0
