# Apply the DOR2 runtime delta to the pinned ModernGekko submodule, and the SDL
# layout-class delta to the vendored Dolphin tree.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$mg = Join-Path $Root 'lib\ModernGekko-Template\lib\ModernGekko'
$dolphin = Join-Path $mg 'vendor\dolphin'
$patch = Join-Path $Root 'patches\moderngekko-settings.patch'
$dolphin_patch = Join-Path $Root 'patches\dolphin-controller-layout.patch'
if (-not (Test-Path (Join-Path $mg '.git'))) { throw "ModernGekko submodule not initialised: $mg" }
if (-not (Test-Path (Join-Path $dolphin '.git'))) { throw "Dolphin submodule not initialised: $dolphin" }

function Apply-Patch {
    param([string]$Repo, [string]$Patch, [string]$Label)
    # Idempotent: skip if already applied.
    git -C $Repo apply --reverse --check $Patch 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host "[apply-patches] $Label already applied"; return }
    git -C $Repo apply --check $Patch
    if ($LASTEXITCODE -ne 0) { throw "$Label does not apply cleanly to its pinned submodule" }
    git -C $Repo apply $Patch
    Write-Host "[apply-patches] applied $Label"
}

Apply-Patch -Repo $mg -Patch $patch -Label 'moderngekko-settings.patch'
Apply-Patch -Repo $dolphin -Patch $dolphin_patch -Label 'dolphin-controller-layout.patch'