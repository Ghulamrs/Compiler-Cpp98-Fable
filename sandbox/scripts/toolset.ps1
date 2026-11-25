# dot-sourced by the other scripts: locates MSVC and the Windows SDK, sets the environment, and defines
# $Sandbox, $Src, $Exe, $cl, $link; the linker setting $Linker with Link-Program, and where GRIDE is ($GrideRoot,
# $GrideLink, $GrideLib, Show-Gride)
$Sandbox = Split-Path $PSScriptRoot -Parent
$Src     = Split-Path $Sandbox -Parent
$Exe     = "$Sandbox\cppparse.exe"
$VsRoot  = "D:\Microsoft Visual Studio 2026"
$MsvcVer = (Get-ChildItem "$VsRoot\VC\Tools\MSVC" | Sort-Object Name | Select-Object -Last 1).Name
$Msvc    = "$VsRoot\VC\Tools\MSVC\$MsvcVer"
$SdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$SdkVer  = (Get-ChildItem "$SdkRoot\Include" | Sort-Object Name | Select-Object -Last 1).Name
$env:PATH    = "$Msvc\bin\Hostx64\x64;$env:PATH"
$env:INCLUDE = "$Msvc\include;$SdkRoot\Include\$SdkVer\ucrt;$SdkRoot\Include\$SdkVer\um;$SdkRoot\Include\$SdkVer\shared"
$env:LIB     = "$Msvc\lib\x64;$SdkRoot\Lib\$SdkVer\ucrt\x64;$SdkRoot\Lib\$SdkVer\um\x64"
# cppparse's source folders: the driver, shared support, the three stages of the compiler, the old pipeline and its reports
$SourceDirs = @("driver", "common", "frontend", "middle", "backend", "eh", "cv", "target", "analyzer", "analyzer\report")
$OwnSources = @(); foreach ($d in $SourceDirs) { $OwnSources += @(Get-ChildItem "$Src\cppparse\$d\*.cpp") }
# include options for compiling cppparse's own sources with cppparse: its folders, its library headers and the
# minimal Windows declarations of platform\ (only the self-build needs those)
$OwnIncludes = @(); foreach ($d in $SourceDirs) { $OwnIncludes += @("-I", "$Src\cppparse\$d") }
$OwnIncludes += @("-I", "$Src\include", "-I", "$Src\platform")
$cl   = "$Msvc\bin\Hostx64\x64\cl.exe"
$link = "$Msvc\bin\Hostx64\x64\link.exe"
New-Item -ItemType Directory -Force "$Sandbox\out\tmp" | Out-Null
$env:TMP  = "$Sandbox\out\tmp"
$env:TEMP = "$Sandbox\out\tmp"

# GRIDE's tools (docs\design-selfhost.md, 4.6): the folder that holds link.exe and lib\x86_64-windows, GRIDE's runtime.
# It is looked for in this order, and the first place that names one counts:
#   1. the environment variable GRIDE_ROOT: that folder itself (an installed GRIDE, or GRIDEd\x64\Release of a
#      checkout), or the checkout of GRIDE3 above it. When the variable is set nothing else is looked at, so a
#      wrong value means "not found" and never another GRIDE than the one named
#   2. the development tree, My Documents\GRIDE3
#   3. the location GRIDE's Setup wrote: InstallDir under Software\G. R. Akhtar\GRIDE <version>\<outlook> in the
#      registry, the user's before the machine's, the highest version first
# $GrideRoot is "" where there is none; a script that needs GRIDE then prints "skipped: GRIDE not found" and exits
# with $GrideSkipCode, which harness.ps1 reports as a skip and not as a pass.
$GrideSkipCode = 77
function Find-Gride {
    function Holds([string]$dir) { return ($dir -ne "") -and (Test-Path "$dir\link.exe") -and (Test-Path "$dir\lib\x86_64-windows\kernel32.lib") }
    function InTree([string]$dir) {
        if (Holds $dir) { return $dir }
        if (Holds "$dir\GRIDEd\x64\Release") { return "$dir\GRIDEd\x64\Release" }
        return ""
    }
    if ($env:GRIDE_ROOT) { return @((InTree $env:GRIDE_ROOT.TrimEnd('\')), "GRIDE_ROOT") }
    $dev = InTree (Join-Path ([Environment]::GetFolderPath("MyDocuments")) "GRIDE3")
    if ($dev -ne "") { return @($dev, "the development tree") }
    foreach ($hive in @("HKCU:", "HKLM:")) {
        $products = @(Get-ChildItem "$hive\Software\G. R. Akhtar" -ErrorAction SilentlyContinue | Where-Object { $_.PSChildName -like "GRIDE *" } | Sort-Object { try { [version]($_.PSChildName.Substring(6)) } catch { [version]"0.0" } } -Descending)
        foreach ($p in $products) {
            foreach ($outlook in @(Get-ChildItem $p.PSPath -ErrorAction SilentlyContinue)) {
                $dir = "$((Get-ItemProperty $outlook.PSPath -ErrorAction SilentlyContinue).InstallDir)".TrimEnd('\')
                if (Holds $dir) { return @($dir, "installed, $($p.PSChildName)") }
            }
        }
    }
    return @("", "")
}
$GrideRoot, $GrideFrom = Find-Gride
$GrideLink = if ($GrideRoot -ne "") { "$GrideRoot\link.exe" } else { "" }
$GrideLib  = if ($GrideRoot -ne "") { "$GrideRoot\lib\x86_64-windows" } else { "" }
# what a check that uses GRIDE logs: where it is, the SHA-256 of link.exe and of each library of its runtime, and
# GRIDE3's commit where the folder lies in a checkout (the binaries there are built from the tree, so the commit says
# which sources were at hand, the hashes say which binaries ran)
function Show-Gride {
    if ($GrideRoot -eq "") { Write-Host "GRIDE: not found (GRIDE_ROOT, My Documents\GRIDE3, the registry's InstallDir)"; return }
    Write-Host "GRIDE: $GrideRoot ($GrideFrom)"
    $files = @(Get-Item "$GrideRoot\link.exe") + @(Get-ChildItem "$GrideLib\grt*.lib", "$GrideLib\kernel32.lib" | Sort-Object Name)
    foreach ($f in $files) { Write-Host ("  {0,-15} {1}  {2,9:N0} bytes  {3:yyyy-MM-dd HH:mm}" -f $f.Name, (Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLower(), $f.Length, $f.LastWriteTime) }
    $eap = $ErrorActionPreference; $ErrorActionPreference = "SilentlyContinue"
    $commit = ""; try { $commit = "$(& git -C $GrideRoot rev-parse --short HEAD 2>$null)".Trim() } catch { $commit = "" }
    $ErrorActionPreference = $eap
    if ($commit -ne "") { Write-Host "  a checkout, at commit $commit" } else { Write-Host "  not a checkout" }
}

# The linker of Link-Program: "ms", Microsoft's link (the default), or "gride", GRIDE's link. It comes from the
# including script's $Linker (its -Linker parameter) and, where that is empty, from the environment variable
# CPPPARSE_LINKER. Since stage 0b of row P1 every program the scripts link goes through Link-Program (or Build-Program,
# which compiles with cl /c first); what is known to differ under GRIDE's link is printed by Write-NotUnderGride.
if ("$Linker" -eq "") { $Linker = if ($env:CPPPARSE_LINKER) { $env:CPPPARSE_LINKER } else { "ms" } }
if ($Linker -ne "ms" -and $Linker -ne "gride") { throw "unknown linker '$Linker': ms (Microsoft's link) or gride (GRIDE's link)" }
# a script run with -Linker gride says which GRIDE it uses; where there is none it prints "skipped: GRIDE not found" and
# exits with $GrideSkipCode, which harness.ps1 reports as a skip
function Assert-Linker {
    if ($Linker -ne "gride") { return }
    Show-Gride
    if ($GrideRoot -eq "") { Write-Host "skipped: GRIDE not found"; exit $GrideSkipCode }
}
# a case of docs\design-selfhost.md 4.6's list, what is known to differ under GRIDE's link: one line "not under GRIDE's
# link: <where, what is done instead, why>", which harness.ps1 counts, and the script's own count, $KnownDifferences
$KnownDifferences = 0
function Write-NotUnderGride([string]$Reason) { $script:KnownDifferences++; Write-Host "not under GRIDE's link: $Reason" }
# the end of a script's last line: "" or "; <n> known difference(s) under GRIDE's link"
function Get-KnownDifferences { if ($KnownDifferences -eq 0) { return "" }; return "; $KnownDifferences known difference(s) under GRIDE's link" }
# links a console program from objects with the linker of $Linker, on Microsoft's libraries. GRIDE's link gets
# /timestamp:0 (its own option): the image is then a function of its input, where it would carry the time of the
# link. -Libraries: the libraries in place of Microsoft's static C runtime (an empty list: only those the objects name,
# as with cl /MD and /MTd); -With ms or gride: that linker, whatever $Linker says (a case of 4.6's list). Returns the
# linker's messages; $LASTEXITCODE is 0 when the output was written and the linker said so.
function Link-Program([string[]]$Objects, [string]$Output, [string[]]$Options = @(), [string[]]$Libraries = $null, [string]$With = "") {
    $ErrorActionPreference = "Continue"
    $libs = if ($null -eq $Libraries) { @("libucrt.lib", "libvcruntime.lib", "libcmt.lib", "kernel32.lib", "legacy_stdio_definitions.lib") } else { $Libraries }
    $which = if ($With -ne "") { $With } else { $Linker }
    if (Test-Path $Output) { Remove-Item $Output }
    if ($which -eq "gride") { $msg = @(& $GrideLink /nologo /SUBSYSTEM:CONSOLE /timestamp:0 @Options "/OUT:$Output" @Objects @libs 2>&1 | ForEach-Object { "$_" }) }
    else { $msg = @(& $link /nologo /SUBSYSTEM:CONSOLE @Options "/OUT:$Output" @Objects @libs 2>&1 | ForEach-Object { "$_" }) }
    if ($LASTEXITCODE -eq 0 -and -not (Test-Path $Output)) { $global:LASTEXITCODE = 1 }
    return $msg
}
# what "cl ... /Fe<program>" did in one call: cl /c with the given arguments (options, /Fo, sources), then Link-Program
# of the objects (which the caller names, as /Fo puts them). Returns cl's and the linker's messages; $LASTEXITCODE is
# 0 when both succeeded.
function Build-Program([string[]]$Compile, [string[]]$Objects, [string]$Output, [string[]]$Options = @(), [string[]]$Libraries = $null) {
    $ErrorActionPreference = "Continue"
    if (Test-Path $Output) { Remove-Item $Output }
    $msg = @(& $cl /nologo /c @Compile 2>&1 | ForEach-Object { "$_" })
    if ($LASTEXITCODE -ne 0) { return $msg }
    return @($msg + (Link-Program $Objects $Output $Options $Libraries))
}

# The runtime's object (runtime\cppparse_rt.cpp; row P1 stage 2, docs\design-selfhost.md 4.5): compiled by cl, the
# default, or by the compiler under test ($Exe) when the environment variable CPPPARSE_RT is "cppparse". One object
# per compiler: cppparse's has no exception model's mark, so it serves the table, sjlj and cl models alike. Returns
# the compiler's messages; $LASTEXITCODE is 0 when the object was written. ($RepositoryRoot, not $Src: the scripts
# name variables of their own $src, and PowerShell's names ignore case.)
$RepositoryRoot = $Src
function Build-Runtime([string]$Object, [string[]]$ClOptions = @("/EHsc", "/W3")) {
    if (Test-Path $Object) { Remove-Item $Object }
    if ($env:CPPPARSE_RT -eq "cppparse") {
        $dir = Split-Path $Object -Parent; if ($dir -eq "") { $dir = "." }
        $stem = [IO.Path]::GetFileNameWithoutExtension($Object) + "-rt"
        $msg = @(& $Exe --compile6 -O2 --obj --msvc-mangling --eh=table -q -I "$RepositoryRoot\include" -I "$RepositoryRoot\platform" -o $dir -n $stem "$RepositoryRoot\runtime\cppparse_rt.cpp" 2>&1 | ForEach-Object { "$_" })
        if (Test-Path "$dir\${stem}6.obj") { Move-Item -Force "$dir\${stem}6.obj" $Object }
    }
    elseif ($env:CPPPARSE_RT -and $env:CPPPARSE_RT -ne "cl") { throw "unknown CPPPARSE_RT '$env:CPPPARSE_RT': cl or cppparse" }
    else { $msg = @(& $cl /nologo /c @ClOptions "/Fo$Object" "$RepositoryRoot\runtime\cppparse_rt.cpp" 2>&1 | ForEach-Object { "$_" }) }
    $global:LASTEXITCODE = if (Test-Path $Object) { 0 } else { 1 }
    return ($msg -join "`n")
}