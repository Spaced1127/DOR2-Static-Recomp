# Memory-card lifecycle gate (raw slot A card).
#
# STATUS: AUTHORED BUT NEVER EXECUTED - launch pause in force (hub directive
# 2026-11-16: no GUI/test launches while the user is at the machine). Run this
# once the pause is lifted, then flip the status line above to "verifed <date>".
#
# What it proves, end to end, with no hardware and no menu driving:
#   1. fresh user dir boot creates GC/USA|EUR|JAP and GC/MemoryCardA.USA.raw
#      (16,777,216 bytes) - the raw slot A card, not the GCI folder;
#   2. the runtime log shows SlotA=1 / SlotB=255 and no "Card A" folder write;
#   3. the game's save lands in the card directory (game code + company + file);
#   4. a second boot reuses the same card byte-for-byte (save read, not recreated).
#
# Runs the real runner in a NORMAL VISIBLE window, ends it with the automation
# `command=stop` file (graceful exit flushes the card), and never force-kills.
#
# Usage: powershell -ExecutionPolicy Bypass -File scripts\test-memcard.ps1
[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$Game = '',
    [int]$BootSeconds = 60,
    [string]$ExpectGameCode = 'GW2E'
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$runner = Join-Path $Root 'build\moderngekko\DOR2RecompRunner.exe'
if (-not $Game) { $Game = Join-Path $Root 'work\game' }
if (-not (Test-Path $runner)) { throw "runner not built: $runner" }
if (-not (Test-Path (Join-Path $Game 'sys\main.dol'))) { throw "no extracted game at $Game" }

$user = Join-Path $Root 'work\memcard-gate'
$auto = Join-Path $Root 'work\memcard-gate-auto'
Remove-Item -Recurse -Force $user, $auto -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $user, (Join-Path $auto 'commands') | Out-Null

$card = Join-Path $user 'GC\MemoryCardA.USA.raw'
$failures = @()

function Invoke-Boot {
    param([string]$Tag)
    $log = Join-Path $user ("run-$Tag.log")
    # Visible window on purpose (see STATE.md notes).
    $p = Start-Process -FilePath $runner -PassThru `
        -ArgumentList @('--core', 'jit', '--game', "`"$Game`"", '--user-dir',
                        "`"$user`"", '--automation-dir', "`"$auto`"", '--mute') `
        -RedirectStandardError $log -RedirectStandardOutput (Join-Path $user "out-$Tag.log")
    Start-Sleep -Seconds $BootSeconds
    "command=stop" | Set-Content -Path (Join-Path $auto 'commands\stop.txt') -Encoding ASCII
    if (-not $p.WaitForExit(45000)) {
        # Only reached if the graceful path fails; report rather than hide it.
        throw "runner did not exit after the stop command ($Tag)"
    }
    return $log
}

function Get-CardEntries {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return @() }
    $bytes = [IO.File]::ReadAllBytes($Path)
    $entries = @()
    foreach ($base in 0x2000, 0x4000) {
        for ($i = 0; $i -lt 127; $i++) {
            $o = $base + $i * 64
            if ($o + 40 -gt $bytes.Length) { break }
            $code = [Text.Encoding]::ASCII.GetString($bytes, $o, 4)
            if ($code -match '^[\x20-\x7e]{4}$' -and $code -ne '    ') {
                $company = [Text.Encoding]::ASCII.GetString($bytes, $o + 4, 2)
                $name = ([Text.Encoding]::ASCII.GetString($bytes, $o + 8, 32)) -replace "[\x00\xff]+$", ''
                $entries += [pscustomobject]@{ Block = $base; Code = $code; Company = $company; File = $name }
            }
        }
    }
    return $entries
}

Write-Host "[memcard-gate] boot 1 (fresh user dir)"
$log1 = Invoke-Boot -Tag 'boot1'

foreach ($region in 'USA', 'EUR', 'JAP') {
    if (-not (Test-Path (Join-Path $user "GC\$region"))) { $failures += "GC\$region missing" }
}
if (-not (Test-Path $card)) {
    $failures += "card missing: $card"
} else {
    $len = (Get-Item $card).Length
    if ($len -ne 16777216) { $failures += "card size $len != 16777216" }
}
$logText = Get-Content $log1 -Raw
if ($logText -notmatch 'effective slot A=1 slot B=255') { $failures += 'log: no "effective slot A=1 slot B=255"' }
if (Test-Path (Join-Path $user 'GC\USA\Card A')) { $failures += 'GCI folder device used (GC\USA\Card A exists)' }

$sha1 = if (Test-Path $card) { (Get-FileHash $card).Hash } else { '' }
$entries1 = Get-CardEntries -Path $card
$saveEntry = $entries1 | Where-Object { $_.Code -eq $ExpectGameCode } | Select-Object -First 1
if (-not $saveEntry) { $failures += "no card directory entry for game code $ExpectGameCode" }
Write-Host ("[memcard-gate] card {0} bytes sha {1}" -f (Get-Item $card -ErrorAction SilentlyContinue).Length, $sha1)
foreach ($e in $entries1) { Write-Host ("[memcard-gate]   entry block 0x{0:x} {1}/{2} {3}" -f $e.Block, $e.Code, $e.Company, $e.File) }

Write-Host "[memcard-gate] boot 2 (same card, must be reused)"
$null = Invoke-Boot -Tag 'boot2'
$sha2 = if (Test-Path $card) { (Get-FileHash $card).Hash } else { '' }
if ($sha1 -ne $sha2) { $failures += "card rewritten on the second boot ($sha1 -> $sha2)" }
if (Test-Path (Join-Path $user 'GC\USA\Card A')) { $failures += 'GCI folder device used on boot 2' }

Write-Host ""
if ($failures.Count -eq 0) {
    Write-Host "[memcard-gate] PASS: raw slot A card created, game save present, reused unchanged" -ForegroundColor Green
    Write-Host "[memcard-gate] card sha256 $sha1"
    exit 0
}
Write-Host "[memcard-gate] FAIL" -ForegroundColor Red
$failures | ForEach-Object { Write-Host "  - $_" }
exit 1