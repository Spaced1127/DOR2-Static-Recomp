# Verify the v0.1.6 controller-profile behaviour without hardware.
#
#   A) existing pad profile + no pad connected  -> profile must be PRESERVED
#      (v0.1.5 replaced it with the keyboard profile, which left Xbox players
#      with no input at all)
#   B) no profile + no pad                      -> keyboard fallback generated
#   C) explicit --controller-layout keyboard    -> keyboard profile written
#
# Runs the real runner on a hidden desktop; nothing appears on the user desktop.
[CmdletBinding()]
param(
    [string]$Root = '',
    [int]$WaitSeconds = 40
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$runner = Join-Path $Root 'build\moderngekko\DOR2RecompRunner.exe'
$game = Join-Path $Root 'work\game'
$hidden = 'H:\Orchestration\tools\run-hidden.ps1'
if (-not (Test-Path $runner)) { throw "runner not built: $runner" }

$standardProfile = @'
[GCPad1]
Device = SDL/0/Xbox Wireless Controller
Buttons/A = `Button A`
Buttons/B = `Button B`
Buttons/X = `Button X`
Buttons/Y = `Button Y`
Buttons/Z = `Shoulder R`
Buttons/Start = Start
Main Stick/Up = `Left Y+`
Main Stick/Down = `Left Y-`
Main Stick/Left = `Left X-`
Main Stick/Right = `Left X+`
Main Stick/Calibration = 100.00
C-Stick/Up = `Right Y+`
C-Stick/Down = `Right Y-`
C-Stick/Left = `Right X-`
C-Stick/Right = `Right X+`
C-Stick/Calibration = 100.00
Triggers/L = `Trigger L`
Triggers/R = `Trigger R`
Triggers/L-Analog = `Trigger L`
Triggers/R-Analog = `Trigger R`
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Rumble/Motor = `Motor L` | `Motor R`
[GCPad2]
[GCPad3]
[GCPad4]
'@

function Invoke-Scenario {
    param([string]$Name, [bool]$PlantProfile, [string[]]$ExtraArgs = @())
    $user = Join-Path $Root "work\ctrl-check-$Name"
    Remove-Item -Recurse -Force $user -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $user 'Config') | Out-Null
    $profile = Join-Path $user 'Config\GCPadNew.ini'
    if ($PlantProfile) { Set-Content -Path $profile -Value $standardProfile -NoNewline }
    $before = if (Test-Path $profile) { 'present' } else { 'absent' }
    $log = Join-Path $user 'run.log'
    $args = @('--core', 'jit', '--game', "`"$game`"", '--user-dir', "`"$user`"",
              '--mute') + $ExtraArgs
    $argLine = ($args -join ' ') + " > `"$log`" 2>&1"
    # run-hidden.ps1 passes ExeArgs straight to CreateProcess, so keep the
    # command line in a space-free .cmd file (the project path has spaces).
    $cmdPath = Join-Path $env:TEMP "dor2-ctrl-$Name.cmd"
    Set-Content -Path $cmdPath -Encoding ASCII -Value @(
        '@echo off',
        "`"$runner`" $argLine"
    )
    & powershell -ExecutionPolicy Bypass -File $hidden -Exe $env:ComSpec `
        -ExeArgs "/c `"$cmdPath`"" -WaitSeconds $WaitSeconds | Out-Null
    # run-hidden only reaps cmd.exe; stop the runner it started.
    Get-CimInstance Win32_Process -Filter "Name='DOR2RecompRunner.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.Contains($user) } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
    $profileText = if (Test-Path $profile) { Get-Content $profile -Raw } else { '' }
    # Dolphin normalizes the file itself; judge it by content, not by hash.
    $state = if ($profileText -match 'DInput/0/Keyboard Mouse') { 'keyboard' }
             elseif ($profileText -match 'SDL/0/Xbox Wireless Controller') { 'xbox-profile' }
             elseif ($profileText) { 'other' }
             else { 'absent' }
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    $controllerLines = @($text -split "`r?`n" | Where-Object { $_ -match '^controller:' })
    [pscustomobject]@{
        Scenario = $Name
        Profile  = $state
        Keyboard = if ($text -match 'DInput/0/Keyboard Mouse') { 'yes' } else { 'no' }
        Booted   = if ($text -match '\[video\]') { 'yes' } else { 'no' }
        Lines    = $controllerLines
    }
}

$results = @()
$results += Invoke-Scenario -Name 'a-keep' -PlantProfile $true
$results += Invoke-Scenario -Name 'b-fresh' -PlantProfile $false
$results += Invoke-Scenario -Name 'c-explicit' -PlantProfile $true -ExtraArgs @('--controller-layout', 'keyboard')

$fail = 0
foreach ($r in $results) {
    Write-Host ""
    Write-Host ("[{0}] profile={1} keyboard-profile={2} booted={3}" -f `
        $r.Scenario, $r.Profile, $r.Keyboard, $r.Booted)
    $r.Lines | Select-Object -First 3 | ForEach-Object { Write-Host "    $_" }
}
if ($results[0].Profile -ne 'xbox-profile' -or $results[0].Keyboard -ne 'no') { Write-Host "FAIL: A (existing pad profile was not preserved)" -ForegroundColor Red; $fail++ }
if ($results[1].Profile -ne 'keyboard' -or $results[1].Keyboard -ne 'yes') { Write-Host "FAIL: B (fresh install should fall back to the keyboard)" -ForegroundColor Red; $fail++ }
if ($results[2].Profile -ne 'keyboard' -or $results[2].Keyboard -ne 'yes') { Write-Host "FAIL: C (explicit keyboard layout)" -ForegroundColor Red; $fail++ }
foreach ($r in $results) { if ($r.Booted -ne 'yes') { Write-Host "FAIL: $($r.Scenario) did not boot" -ForegroundColor Red; $fail++ } }
Write-Host ""
if ($fail -eq 0) { Write-Host "controller profile regression: PASS" -ForegroundColor Green } else { Write-Host "controller profile regression: FAIL ($fail)" -ForegroundColor Red }
exit $fail