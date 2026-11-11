# Extract the retail disc and build the static game module.
#
# Usage:
#   scripts\recompile.ps1 -Iso "D:\dumps\WWE Day of Reckoning 2 (USA).iso"
#   scripts\recompile.ps1                      # reuse work\game
#
# Module builds happen under a short path (default C:\d2modules) because the
# generated chunk object paths exceed MSVC/CMake limits under long folders
# containing spaces.

[CmdletBinding()]
param(
    [string]$Iso = '',
    [string]$Game = '',
    [string]$ModuleRoot = '',
    [int]$Jobs = 8,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build\moderngekko'
$Game = if ($Game) { $Game } else { Join-Path $Root 'work\game' }
$ModuleRoot = if ($ModuleRoot) { $ModuleRoot } else { Join-Path $env:SystemDrive 'd2modules' }

$Prepare = Join-Path $Build 'DOR2Prepare.exe'
$Runner = Join-Path $Build 'DOR2RecompRunner.exe'

function Resolve-Tool([string]$name) {
    $candidates = @(
        (Join-Path $Build $name),
        (Join-Path $Build ('moderngekko-runtime\' + $name)),
        (Join-Path $Build ('moderngekko-runtime\vendor\dolphin\DolRecomp\' + $name))
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

$Port = Resolve-Tool 'moderngekko-port.exe'
$Prepare = Resolve-Tool 'DOR2Prepare.exe'
$Runner = Resolve-Tool 'DOR2RecompRunner.exe'

if (-not $Prepare -or -not $Port) {
    Write-Host '[recompile] runtime tools missing; running build.ps1' -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot 'build.ps1') -Jobs $Jobs
    $Port = Resolve-Tool 'moderngekko-port.exe'
    $Prepare = Resolve-Tool 'DOR2Prepare.exe'
    $Runner = Resolve-Tool 'DOR2RecompRunner.exe'
    if (-not $Prepare -or -not $Port) { throw 'moderngekko-port.exe / DOR2Prepare.exe not found after building.' }
}

# --- extract -----------------------------------------------------------------
if ($Iso) {
    if (-not (Test-Path $Iso)) { throw "ISO not found: $Iso" }
    Write-Host '[recompile] extracting the disc' -ForegroundColor Cyan
    $prepareArgs = @('--input', $Iso, '--output', $Game)
    if ($Force) { $prepareArgs += '--force' }
    & $Prepare @prepareArgs
    if ($LASTEXITCODE -ne 0) { throw "DOR2Prepare failed with exit code $LASTEXITCODE" }
}
elseif (-not (Test-Path (Join-Path $Game 'sys\main.dol'))) {
    throw "No extracted game at $Game. Pass -Iso '<disc.iso>' the first time."
}

# --- module ------------------------------------------------------------------
$ClangPath = (Get-Command clang -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ClangPath) {
    $fallback = 'C:\Program Files\LLVM\bin\clang.exe'
    if (Test-Path $fallback) { $ClangPath = $fallback }
    else { throw 'clang was not found on PATH or in C:\Program Files\LLVM\bin.' }
}
$env:Path = (Split-Path -Parent $ClangPath) + ';' + $env:Path

# The generated header defaults to 256/24. Ship the tested 4096/128 pair and
# the portable AVX2 baseline. CMake picks these up on the first configure of
# the module build.
$env:CFLAGS = '-march=x86-64-v3 -mtune=generic ' +
              '-DDOLRECOMP_C_LOOP_CYCLE_BUDGET=4096 ' +
              '-DDOLRECOMP_C_MAX_CALL_DEPTH=128'

New-Item -ItemType Directory -Force -Path $ModuleRoot | Out-Null

Write-Host '[recompile] generating and compiling the static module' -ForegroundColor Cyan
$portArgs = @('build', $Game, '--backend', 'c', '--toolchain', 'clang',
              '--opt-level', '3', '--output', $ModuleRoot)
if ($Force) {
    # A cache hit is cheap and correct; -Force only affects extraction today.
    Write-Host '[recompile] cache hits are reused by DOL + toolchain hash' -ForegroundColor DarkGray
}
& $Port @portArgs
if ($LASTEXITCODE -ne 0) { throw "moderngekko-port failed with exit code $LASTEXITCODE" }

$module = Get-ChildItem -Path (Join-Path $ModuleRoot 'GW2E78') -Recurse `
    -Filter 'gGW2E78_recomp.dll' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $module) { throw "No gGW2E78_recomp.dll was produced under $ModuleRoot" }

$dest = Join-Path $Build 'gGW2E78_recomp.dll'
Copy-Item $module.FullName $dest -Force

$manifest = Get-ChildItem -Path $module.Directory.FullName -Filter 'manifest.txt' |
    Select-Object -First 1
$metaDir = Join-Path $Root 'build\meta'
New-Item -ItemType Directory -Force -Path $metaDir | Out-Null
if ($manifest) { Copy-Item $manifest.FullName (Join-Path $metaDir 'module-manifest.txt') -Force }

$smc = Join-Path $module.Directory.FullName 'generated_smc.txt'
if (Test-Path $smc) { Copy-Item $smc (Join-Path $metaDir 'generated_smc.txt') -Force }

$hash = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
Write-Host ''
Write-Host '[recompile] module ready' -ForegroundColor Green
Write-Host "  source : $($module.FullName)"
Write-Host "  copy   : $dest"
Write-Host "  sha256 : $hash"
Write-Host ''
Write-Host 'Next: scripts\run.ps1 -Core static' -ForegroundColor Cyan
