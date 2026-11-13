# RAM differential: dump guest RAM at frame checkpoints on one core.
# Usage: ramdiff.ps1 -Core static -Tag rd-static [-Steps 5] [-StepFrames 60]
param(
    [string]$Core = 'static',
    [string]$Tag = 'rd',
    [int]$Steps = 5,
    [int]$StepFrames = 60
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$state = Join-Path $Root 'work\states\diag-long.sav'
$ud = Join-Path $Root "work\user-$Tag"
if (Test-Path $ud) { Remove-Item -Recurse -Force $ud }
New-Item -ItemType Directory -Force -Path $ud | Out-Null
$auto = Join-Path $Root "work\auto-$Tag"
if (Test-Path $auto) { Remove-Item -Recurse -Force $auto }
$cmd = Join-Path $auto 'commands'
New-Item -ItemType Directory -Force -Path $cmd | Out-Null
$dumps = Join-Path $Root "work\ramdiff\$Tag"
if (Test-Path $dumps) { Remove-Item -Recurse -Force $dumps }
New-Item -ItemType Directory -Force -Path $dumps | Out-Null

# Build command sequence: dump0, pad, dump1, pad, ... dumpN
$n = 0
for ($i = 0; $i -le $Steps; $i++) {
    $p = (Join-Path $dumps "ram_$i.bin") -replace '\\','/'
    "{0:D4}_read.txt" -f $n | ForEach-Object {
        "command=read_memory`naddress=0x80000000`nsize=0x1000000`npath=$p" |
            Set-Content -Path (Join-Path $cmd $_) -Encoding ASCII }
    $n++
    if ($i -lt $Steps) {
        "{0:D4}_pad.txt" -f $n | ForEach-Object {
            "command=pad_frames`nport=0`nframes=$StepFrames" |
                Set-Content -Path (Join-Path $cmd $_) -Encoding ASCII }
        $n++
    }
}
$total = $n

$arglist = @('--core', $Core, '--game', (Join-Path $Root 'work\game'),
             '--user-dir', $ud, '--graphics', 'Vulkan', '--audio', 'Null',
             '--automation-dir', $auto, '--load-state', $state) |
    ForEach-Object { '"' + $_ + '"' }
$p = Start-Process -FilePath (Join-Path $Root 'build\moderngekko\DOR2RecompRunner.exe') `
    -ArgumentList ($arglist -join ' ') -PassThru `
    -RedirectStandardOutput (Join-Path $dumps 'console.txt') `
    -RedirectStandardError (Join-Path $dumps 'console.err.txt')

# Wait until all commands processed (or timeout).
$deadline = (Get-Date).AddSeconds(240)
while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
    $st = Join-Path $auto 'status.txt'
    if (Test-Path $st) {
        $pc = (Select-String -Path $st -Pattern '^processed_commands=(\d+)').Matches[0].Groups[1].Value
        if ([int]$pc -ge $total) { break }
    }
    Start-Sleep -Milliseconds 500
}
'command=stop' | Set-Content (Join-Path $cmd 'zzzz_stop.txt') -Encoding ASCII
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; Get-CimInstance Win32_Process -Filter "Name='DOR2RecompRunner.exe'" | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
"[$Tag] core=$Core dumps=$((Get-ChildItem $dumps -Filter 'ram_*.bin').Count) expected=$($Steps+1)"
