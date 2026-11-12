# Assemble a player release folder and zip.
#
# The release contains no game data: no ISO/RVZ, DOL, FST, textures, audio,
# saves or extracted trees. The script refuses to package if it finds any.

[CmdletBinding()]
param(
    [string]$Version = '0.1.0',
    [string]$Module = '',
    [string]$OutRoot = '',
    [switch]$SkipZip,
    [switch]$SkipManifestCheck
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build\moderngekko'
$OutRoot = if ($OutRoot) { $OutRoot } else { Join-Path $Root 'dist' }
$Module = if ($Module) { $Module } else { Join-Path $Build 'gGW2E78_recomp.dll' }

$Name = "DOR2StaticRecomp-v$Version-Windows-x64"
$Stage = Join-Path $OutRoot $Name

$required = @(
    'DOR2Recomp.exe',
    'DOR2RecompRunner.exe',
    'DOR2RecompRunnerCore.exe',
    'DOR2Prepare.exe'
)
foreach ($file in $required) {
    $path = Join-Path $Build $file
    if (-not (Test-Path $path)) { throw "Missing build artifact: $path" }
}
if (-not (Test-Path $Module)) {
    throw "Missing static module: $Module. Run scripts\recompile.ps1 first."
}

$sys = Join-Path $Build 'Sys'
if (-not (Test-Path $sys)) { throw "Missing runtime data folder: $sys" }
foreach ($dsp in @('GC\dsp_rom.bin', 'GC\dsp_coef.bin')) {
    if (-not (Test-Path (Join-Path $sys $dsp))) {
        throw "Release would fail DSP LLE: missing Sys\$dsp"
    }
}

# --- stage -------------------------------------------------------------------
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

foreach ($file in $required) {
    Copy-Item (Join-Path $Build $file) (Join-Path $Stage $file) -Force
}
Copy-Item $Module (Join-Path $Stage 'gGW2E78_recomp.dll') -Force
Copy-Item $sys (Join-Path $Stage 'Sys') -Recurse -Force

Copy-Item (Join-Path $Root 'LICENSE') (Join-Path $Stage 'LICENSE') -Force
Copy-Item (Join-Path $Root 'docs\release\README.txt') (Join-Path $Stage 'README.txt') -Force
Copy-Item (Join-Path $Root 'docs\release\PLAYERS.md') (Join-Path $Stage 'PLAYERS.md') -Force
Copy-Item (Join-Path $Root 'docs\release\recomplauncher.json') `
    (Join-Path $Stage 'recomplauncher.json') -Force
# Launcher artwork (SteamGridDB, user-approved; see docs/ASSETS.md).
foreach ($art in @('dor2_grid.png', 'dor2_logo.png')) {
    $src = Join-Path $Root "assets\sgdb\$art"
    if (Test-Path $src) { Copy-Item $src (Join-Path $Stage $art) -Force }
}

@'
@echo off
start "" "%~dp0DOR2Recomp.exe"
'@ | Set-Content -Path (Join-Path $Stage 'Play.cmd') -Encoding ASCII

# --- game-data guard ---------------------------------------------------------
$badExtensions = @('.iso', '.rvz', '.gcm', '.wbfs', '.gcz', '.dol', '.elf', '.rel',
                   '.gci', '.sav', '.raw', '.mcd', '.ps2', '.bnr')
$badNames = @('main.dol', 'boot.bin', 'bi2.bin', 'apploader.img', 'fst.bin',
              'opening.bnr', 'system.cfg')
$violations = @()

Get-ChildItem $Stage -Recurse -File | ForEach-Object {
    if ($badExtensions -contains $_.Extension.ToLowerInvariant()) {
        $violations += $_.FullName
    }
    if ($badNames -contains $_.Name.ToLowerInvariant()) {
        $violations += $_.FullName
    }
    if ($_.FullName -match '[\\/](Game|files|extracted)[\\/]') {
        $violations += $_.FullName
    }
}

if ($violations.Count -gt 0) {
    Write-Host '[package] REFUSING TO PACKAGE: game data found' -ForegroundColor Red
    $violations | ForEach-Object { Write-Host "  $_" }
    throw 'Game data is present in the staging folder.'
}

# --- manifest ----------------------------------------------------------------
if (-not $SkipManifestCheck) {
    $validator = Join-Path $Root 'tools\validate_manifest.py'
    $schema = Join-Path $Root 'docs\recomplauncher.schema.json'
    $manifest = Join-Path $Stage 'recomplauncher.json'
    python $validator $schema $manifest
    if ($LASTEXITCODE -ne 0) { throw 'recomplauncher.json failed schema validation' }
}

# --- sizes / hashes ----------------------------------------------------------
$allFiles = Get-ChildItem $Stage -Recurse -File
$total = ($allFiles | Measure-Object -Property Length -Sum).Sum
Write-Host ''
Write-Host ('[package] ' + $Stage) -ForegroundColor Green
Write-Host ('[package] {0} files, {1:N1} MB' -f $allFiles.Count, ($total / 1MB))
Write-Host ''
$allFiles | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($Stage.Length + 1)
    '{0,-46} {1,10:N0}' -f $relative, $_.Length
}

if (-not $SkipZip) {
    $zip = Join-Path $OutRoot "$Name.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $zip -CompressionLevel Optimal
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    $sums = Join-Path $OutRoot 'SHA256SUMS.txt'
    "$hash  $Name.zip" | Set-Content -Path $sums -Encoding ASCII
    Write-Host ''
    Write-Host ('[package] zip    : ' + $zip) -ForegroundColor Green
    Write-Host ('[package] sha256 : ' + $hash)
    Write-Host ('[package] sums   : ' + $sums)
}
