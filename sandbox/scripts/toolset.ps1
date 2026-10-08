# dot-sourced by the other scripts: locates MSVC and the Windows SDK, sets the environment, and defines
# $Sandbox, $Src, $Exe, $cl, $link
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
$cl   = "$Msvc\bin\Hostx64\x64\cl.exe"
$link = "$Msvc\bin\Hostx64\x64\link.exe"
New-Item -ItemType Directory -Force "$Sandbox\out\tmp" | Out-Null
$env:TMP  = "$Sandbox\out\tmp"
$env:TEMP = "$Sandbox\out\tmp"
