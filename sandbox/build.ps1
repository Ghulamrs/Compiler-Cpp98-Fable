# build.ps1 - compile and run the Vector/Matrix tests with MSVC (Visual Studio 2026)
#
# Usage (from any directory):
#   powershell -ExecutionPolicy Bypass -File "cpp\sandbox\build.ps1"
#
# cmd.exe is disabled by policy on this machine, so vcvars64.bat and
# Enter-VsDevShell cannot be used. This script sets INCLUDE / LIB / PATH
# for cl.exe by hand instead.
#
# MSVC has no /std:c++98 switch; its oldest mode is C++14 (the default).
# The sources are strict C++98, a subset of that, so they compile unchanged.
# /permissive- enforces standard conformance, /W4 enables most warnings.

$ErrorActionPreference = "Stop"

$Sandbox = $PSScriptRoot
$Src     = Split-Path $Sandbox -Parent

# --- toolset locations (newest MSVC toolset and Windows SDK found) ---------
$VsRoot  = "D:\Microsoft Visual Studio 2026"
$MsvcVer = (Get-ChildItem "$VsRoot\VC\Tools\MSVC" | Sort-Object Name | Select-Object -Last 1).Name
$Msvc    = "$VsRoot\VC\Tools\MSVC\$MsvcVer"
$SdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$SdkVer  = (Get-ChildItem "$SdkRoot\Include" | Sort-Object Name | Select-Object -Last 1).Name

$env:PATH    = "$Msvc\bin\Hostx64\x64;$env:PATH"
$env:INCLUDE = "$Msvc\include;$SdkRoot\Include\$SdkVer\ucrt;$SdkRoot\Include\$SdkVer\um;$SdkRoot\Include\$SdkVer\shared"
$env:LIB     = "$Msvc\lib\x64;$SdkRoot\Lib\$SdkVer\ucrt\x64;$SdkRoot\Lib\$SdkVer\um\x64"

$cl = "$Msvc\bin\Hostx64\x64\cl.exe"

Write-Host "=== Toolset ==="
Write-Host "MSVC $MsvcVer, Windows SDK $SdkVer"
Write-Host ""

# The project path contains spaces, which Windows PowerShell 5.1 mangles when
# passing /I and /Fe arguments to a native exe. Work inside the sandbox
# directory with relative paths instead.
Push-Location $Sandbox
try {
    function Build-Test([string]$name) {
        Write-Host "=== Building $name ==="
        & $cl /nologo /EHsc /W4 /permissive- /I.. /Fo.\ "/Fe.\$name.exe" "..\$name.cpp"
        if ($LASTEXITCODE -ne 0) { throw "$name failed to compile (exit code $LASTEXITCODE)" }
        Write-Host ""
    }

    function Run-Test([string]$name) {
        Write-Host "=== Running $name ==="
        & ".\$name.exe"
        if ($LASTEXITCODE -ne 0) { throw "$name FAILED (exit code $LASTEXITCODE)" }
        Write-Host ""
    }

    Build-Test "VectorTest"
    Build-Test "MatrixTest"
    Run-Test   "VectorTest"
    Run-Test   "MatrixTest"

    Write-Host "=== All builds and tests succeeded ==="
}
finally {
    Pop-Location
}
