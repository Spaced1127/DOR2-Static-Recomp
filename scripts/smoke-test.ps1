# Deterministic smoke test: boot the port, sample status, screenshot, save a
# state, stop, and keep the evidence.
#
# Usage:
#   scripts\smoke-test.ps1 -Seconds 35 -Tag boot
#   scripts\smoke-test.ps1 -Core jit -Tag jit-reference

[CmdletBinding()]
param(
    [int]$Seconds = 35,
    [ValidateSet('static', 'jit')]
    [string]$Core = 'static',
    [string]$Tag = 'boot',
    [string]$Game = '',
    [string]$Module = '',
    [string]$UserDir = '',
    [string]$Graphics = 'Vulkan',
    [string]$Audio = 'Null',
    [string]$LoadState = '',
    [int]$ShutdownTimeout = 60
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build\moderngekko'
$Runner = Join-Path $Build 'DOR2RecompRunner.exe'
$Game = if ($Game) { $Game } else { Join-Path $Root 'work\game' }
$Module = if ($Module) { $Module } else { Join-Path $Build 'gGW2E78_recomp.dll' }
$UserDir = if ($UserDir) { $UserDir } else { Join-Path $Root "work\user-$Tag" }
$Auto = Join-Path $Root "work\auto-$Tag"
$Evidence = Join-Path $Root "docs\evidence\$Tag"

if (-not (Test-Path $Runner)) { throw "Runner not built: $Runner" }
if ($Core -eq 'static' -and -not (Test-Path $Module)) { throw "Module missing: $Module" }
if (-not (Test-Path (Join-Path $Game 'sys\main.dol'))) { throw "Game not extracted: $Game" }

if (Test-Path $Auto) { Remove-Item -Recurse -Force $Auto }
New-Item -ItemType Directory -Force -Path (Join-Path $Auto 'commands') | Out-Null
New-Item -ItemType Directory -Force -Path $Evidence | Out-Null
New-Item -ItemType Directory -Force -Path $UserDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $UserDir 'Cache\Shaders') | Out-Null

$shot = Join-Path $Evidence 'frame.png'
# Savestates are game data (AGENTS.md): never write them under docs/. They go
# to work\states and can be replayed with -LoadState.
$StateDir = Join-Path $Root 'work\states'
New-Item -ItemType Directory -Force -Path $StateDir | Out-Null
$state = Join-Path $StateDir "$Tag.sav"
$console = Join-Path $Evidence 'console.txt'
$consoleErr = Join-Path $Evidence 'console.err.txt'

$runnerArgs = @(
    '--core', $Core,
    '--game', $Game,
    '--user-dir', $UserDir,
    '--graphics', $Graphics,
    '--audio', $Audio,
    '--automation-dir', $Auto
)
if ($Core -eq 'static') { $runnerArgs += @('--module', $Module) }
if ($LoadState) { $runnerArgs += @('--load-state', $LoadState) }

$quoted = $runnerArgs | ForEach-Object {
    if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
}
$argLine = $quoted -join ' '

Write-Host "[test] starting $Core core for $Seconds s" -ForegroundColor Cyan
$process = Start-Process -FilePath $Runner -ArgumentList $argLine -PassThru `
    -RedirectStandardOutput $console -RedirectStandardError $consoleErr

$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
    Start-Sleep -Seconds 1
}

$status = Join-Path $Auto 'status.txt'
if (Test-Path $status) {
    Copy-Item $status (Join-Path $Evidence 'status.txt') -Force
    Write-Host '--- status.txt ---'
    Get-Content $status
}

if (-not $process.HasExited) {
    "command=screenshot`npath=$($shot -replace '\\','/')" |
        Set-Content -Path (Join-Path $Auto 'commands\shot.txt') -Encoding ASCII
    Start-Sleep -Seconds 3

    "command=save_state`npath=$($state -replace '\\','/')" |
        Set-Content -Path (Join-Path $Auto 'commands\save.txt') -Encoding ASCII
    Start-Sleep -Seconds 3

    "command=stop" |
        Set-Content -Path (Join-Path $Auto 'commands\stop.txt') -Encoding ASCII
}

if (-not $process.WaitForExit($ShutdownTimeout * 1000)) {
    Write-Warning 'Runner did not exit; killing the process tree.'
    Stop-Process -Id $process.Id -Force
}

if (Test-Path $status) {
    Copy-Item $status (Join-Path $Evidence 'status-final.txt') -Force
}

$log = Join-Path $UserDir 'Logs\ModernGekko.log'
if (Test-Path $log) {
    Copy-Item $log (Join-Path $Evidence 'ModernGekko.log') -Force
    Write-Host ''
    Write-Host '--- log tail ---'
    Get-Content $log -Tail 40
}

Write-Host ''
Write-Host "[test] evidence in $Evidence" -ForegroundColor Green
if (-not (Test-Path $shot)) { Write-Warning 'No screenshot was produced.' }
exit $process.ExitCode
