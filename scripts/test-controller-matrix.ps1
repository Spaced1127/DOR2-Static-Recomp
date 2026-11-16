<#
.SYNOPSIS
  Compile and run the offline controller-layout matrix.

.DESCRIPTION
  Builds tools\controller_matrix.cpp against the REAL frontend_config.cpp from
  the pinned ModernGekko submodule, then asserts every controller family's
  GameCube layout against the REAL SDL gamepad DB rows shipped in the build.

  No controller hardware is required and no window is created. This is the
  gate that catches a layout rule misfiring on a family we do not own.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\test-controller-matrix.ps1
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "work\controller-matrix",
    [switch]$KeepBinaries
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $root 'lib\ModernGekko-Template\lib\ModernGekko\tools'
$db = Join-Path $root 'lib\ModernGekko-Template\lib\ModernGekko\vendor\dolphin\Externals\SDL\SDL\src\joystick\SDL_gamepad_db.h'
$out = Join-Path $root $BuildDir

if (-not (Test-Path $db)) { throw "SDL gamepad DB not found: $db" }

# MSVC environment (the submodule is built with VS Build Tools; keep the ABI and
# the standard library identical to the shipped runner).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install VS Build Tools" }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "no VS installation with the C++ toolset found" }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsPath" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'controller_matrix.exe'
$src = Join-Path $root 'tools\controller_matrix.cpp'
$impl = Join-Path $tools 'frontend_config.cpp'

$cmd = @"
call "$vcvars" >nul 2>&1
cd /d "$out"
cl /nologo /std:c++latest /W4 /EHsc /O2 /D_HAS_EXCEPTIONS=1 ^
   /DMODERNGEKKO_GAMECUBE_CONTROLLERS=1 ^
   /I"$tools" ^
   "$src" "$impl" /Fe:"$exe"
"@
$cmdFile = Join-Path $out 'build.cmd'
Set-Content -Path $cmdFile -Value $cmd -Encoding ASCII
$build = & cmd /c "`"$cmdFile`" 2>&1"
if (-not (Test-Path $exe)) {
    $build | ForEach-Object { Write-Host $_ }
    throw "controller matrix build failed"
}
$build | Where-Object { $_ -match 'warning|error' } | ForEach-Object { Write-Host $_ }

Push-Location $root
try {
    & $exe $db
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
if (-not $KeepBinaries) {
    Remove-Item -Force (Join-Path $out '*.obj') -ErrorAction SilentlyContinue
}
Write-Host ""
if ($code -eq 0) { Write-Host "controller matrix: PASS" -ForegroundColor Green }
else { Write-Host "controller matrix: FAIL ($code)" -ForegroundColor Red }
exit $code