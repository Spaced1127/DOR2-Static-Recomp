# Run the port.
#
# Build first (scripts\build.ps1) and recompile the module
# (scripts\recompile.ps1 -Iso ...).

[CmdletBinding()]
param(
    [string]$Game = '',
    [string]$Module = '',
    [string]$UserDir = '',
    [ValidateSet('static', 'jit')]
    [string]$Core = 'static',
    [string]$Graphics = 'Vulkan',
    [string]$Audio = 'Cubeb',
    [string]$LoadState = '',
    [string]$AutomationDir = '',
    [switch]$Fullscreen,
    [switch]$Windowed,
    [switch]$AllowInterpreter,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build\moderngekko'
$Runner = Join-Path $Build 'DOR2RecompRunner.exe'

$Game = if ($Game) { $Game } else { Join-Path $Root 'work\game' }
$UserDir = if ($UserDir) { $UserDir } else { Join-Path $Root 'work\user' }
if (-not $Module) {
    $Module = Join-Path $Build 'gGW2E78_recomp.dll'
}

if (-not (Test-Path $Runner)) { throw "Runner not built: $Runner. Run scripts\build.ps1." }
if (-not (Test-Path (Join-Path $Game 'sys\main.dol'))) {
    throw "No extracted game at $Game. Run scripts\recompile.ps1 -Iso '<disc.iso>'."
}

foreach ($required in @('sys\GC\dsp_rom.bin', 'sys\GC\dsp_coef.bin')) {
    if (-not (Test-Path (Join-Path (Split-Path -Parent $Runner) $required))) {
        Write-Warning "Missing $required next to the runner. DSP LLE audio will fail to start."
    }
}

if ($Core -eq 'static' -and -not (Test-Path $Module)) {
    throw "Static module not found: $Module. Run scripts\recompile.ps1."
}

New-Item -ItemType Directory -Force -Path $UserDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $UserDir 'Cache\Shaders') | Out-Null

$runnerArgs = @(
    '--core', $Core,
    '--game', $Game,
    '--user-dir', $UserDir,
    '--graphics', $Graphics,
    '--audio', $Audio
)
if ($Core -eq 'static') { $runnerArgs += @('--module', $Module) }
if ($LoadState) { $runnerArgs += @('--load-state', $LoadState) }
if ($AutomationDir) { $runnerArgs += @('--automation-dir', $AutomationDir) }
if ($Fullscreen -and -not $Windowed) {
    # The runtime applies fullscreen after the video backend is ready.
    $runnerArgs += @('--fullscreen')
}
if ($Windowed) { $runnerArgs += @('--windowed') }
if ($AllowInterpreter) { $runnerArgs += @('--allow-interpreter') }

if ($ExtraArgs.Count -gt 0) { $runnerArgs += $ExtraArgs }

Write-Host ('[run] ' + $Runner) -ForegroundColor Cyan
Write-Host ('[run] core=' + $Core + ' graphics=' + $Graphics + ' audio=' + $Audio)
Write-Host ('[run] game=' + $Game)
Write-Host ('[run] user=' + $UserDir)
if ($Core -eq 'static') { Write-Host ('[run] module=' + $Module) }
Write-Host ''

# The single runner writes diagnostics to stderr. Capture them into the user
# log (the launcher's Play path does the same); there is no shim tee anymore.
$RunLog = Join-Path $UserDir 'Logs\ModernGekko.log'
New-Item -ItemType Directory -Force -Path (Split-Path $RunLog) | Out-Null
& $Runner @runnerArgs 2>&1 | Tee-Object -FilePath $RunLog
exit $LASTEXITCODE
