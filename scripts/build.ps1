# Build DOR2 Static Recomp.
#
# Configures (only when needed) and builds the ModernGekko runtime, the
# launcher, the runner and DOR2Prepare. Never cleans an existing build tree.

[CmdletBinding()]
param(
    [int]$Jobs = 8,
    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build\moderngekko'

function Find-VcVars {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe not found. Install Visual Studio Build Tools with the C++ x64 workload.'
    }
    $install = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $install) {
        throw 'No Visual Studio installation with the C++ x64 tools was found.'
    }
    $vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found under $install"
    }
    return $vcvars
}

function Invoke-VsCommand([string]$VcVars, [string]$Command) {
    $inner = 'call "' + $VcVars + '" >nul && ' + $Command
    cmd /c $inner
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command"
    }
}

function Find-Clang {
    $onPath = Get-Command clang -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $fallback = 'C:\Program Files\LLVM\bin\clang.exe'
    if (Test-Path $fallback) { return $fallback }
    throw 'clang was not found on PATH or in C:\Program Files\LLVM\bin.'
}

$VcVars = Find-VcVars
$Clang = Find-Clang
$env:Path = (Split-Path -Parent $Clang) + ';' + $env:Path

New-Item -ItemType Directory -Force -Path $Build | Out-Null

$cache = Join-Path $Build 'CMakeCache.txt'
if ($Reconfigure -or -not (Test-Path $cache)) {
    Write-Host '[build] configuring' -ForegroundColor Cyan
    Invoke-VsCommand $VcVars ('cmake -S "' + $Root + '" -B "' + $Build + '" -G Ninja ' +
        '-DCMAKE_BUILD_TYPE=Release')
}
else {
    Write-Host '[build] reusing existing CMake cache' -ForegroundColor Cyan
}

Write-Host '[build] compiling' -ForegroundColor Cyan
Invoke-VsCommand $VcVars ('cmake --build "' + $Build + '" --target ' +
    'moderngekko-run moderngekko-launcher moderngekko-port DOR2Prepare DOR2RecompRunner -j ' + $Jobs)

$artifacts = @(
    'DOR2Recomp.exe',
    'DOR2RecompRunner.exe',
    'DOR2RecompRunnerCore.exe',
    'DOR2Prepare.exe',
    'moderngekko-runtime\moderngekko-port.exe',
    'moderngekko-runtime\dolrecomp.exe'
)

Write-Host ''
Write-Host '[build] artifacts' -ForegroundColor Green
foreach ($name in $artifacts) {
    $path = Join-Path $Build $name
    if (Test-Path $path) {
        $item = Get-Item $path
        '{0,-26} {1,10:N0} bytes' -f $item.Name, $item.Length
    }
}

if (-not (Test-Path (Join-Path $Build 'Sys'))) {
    Write-Warning 'Sys/ was not copied next to the runner. Check the runtime post-build step.'
}

Write-Host ''
Write-Host 'Next: scripts\recompile.ps1 -Iso "<your USA disc.iso>"' -ForegroundColor Cyan
