# Apply the DOR2 runtime delta to the pinned ModernGekko submodule.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$mg = Join-Path $Root 'lib\ModernGekko-Template\lib\ModernGekko'
$patch = Join-Path $Root 'patches\moderngekko-settings.patch'
if (-not (Test-Path (Join-Path $mg '.git'))) { throw "ModernGekko submodule not initialised: $mg" }
# Idempotent: skip if already applied.
git -C $mg apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -eq 0) { Write-Host '[apply-patches] already applied'; exit 0 }
git -C $mg apply --check $patch
if ($LASTEXITCODE -ne 0) { throw 'patch does not apply cleanly to the pinned submodule' }
git -C $mg apply $patch
Write-Host '[apply-patches] applied moderngekko-settings.patch'
