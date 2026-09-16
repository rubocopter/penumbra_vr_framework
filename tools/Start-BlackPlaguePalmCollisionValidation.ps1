[CmdletBinding()]
param(
    [string]$GamePath = (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Penumbra Black Plague\redist\penumbra.exe'),
    [string]$ImagePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $repoRoot 'build\bin\Release\PenumbraVR.ProbeLauncher.exe'
$verifier = Join-Path $PSScriptRoot 'Test-BlackPlagueInputMap.ps1'
$logRoot = Join-Path $env:LOCALAPPDATA 'PenumbraVR\logs'

if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Release probe launcher not found at '$launcher'. Build the Release configuration first."
}
if (-not (Test-Path -LiteralPath $GamePath -PathType Leaf)) {
    throw "Black Plague executable not found at '$GamePath'."
}
if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $ImagePath = Join-Path $repoRoot 'artifacts\black-plague-22000-live.bin'
}
if (-not (Test-Path -LiteralPath $ImagePath -PathType Leaf)) {
    throw "Initialized exact-build image not found at '$ImagePath'."
}

Write-Host 'Validating the exact Black Plague image and palm filter contract...'
& $verifier -ImagePath $ImagePath
if ($LASTEXITCODE -ne 0) {
    throw "Exact-build palm verifier failed with exit code $LASTEXITCODE."
}

$existingGame = @(Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue)
if ($existingGame.Count -ne 0) {
    throw 'A penumbra.exe process is already running. Close it first; this validation request is sampled when the probe initializes.'
}

$request = New-Object System.Threading.Mutex -ArgumentList $false,
    'Local\PenumbraVR.BlackPlague.PalmCollisionValidation'
$gameProcess = $null
try {
    Write-Host 'Starting Black Plague with tracked palm collision validation requested.'
    & $launcher '--launch-vr' $GamePath
    if ($LASTEXITCODE -ne 0) {
        throw "Probe launcher failed with exit code $LASTEXITCODE."
    }

    $deadline = (Get-Date).AddSeconds(60)
    do {
        Start-Sleep -Milliseconds 250
        $gameProcess = Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue |
            Select-Object -First 1
    } while ($null -eq $gameProcess -and (Get-Date) -lt $deadline)
    if ($null -eq $gameProcess) {
        throw 'Black Plague did not create penumbra.exe within 60 seconds.'
    }

    $probeLog = Join-Path $logRoot "black-plague-probe-$($gameProcess.Id).log"
    $activationDeadline = (Get-Date).AddSeconds(30)
    $activation = $null
    do {
        Start-Sleep -Milliseconds 200
        if (Test-Path -LiteralPath $probeLog -PathType Leaf) {
            $activation = Get-Content -LiteralPath $probeLog |
                Select-String -Pattern 'palm_collision enabled=1 source=mutex ' |
                Select-Object -Last 1
        }
    } while ($null -eq $activation -and -not $gameProcess.HasExited -and
             (Get-Date) -lt $activationDeadline)
    if ($null -eq $activation) {
        throw "PID $($gameProcess.Id) did not report palm-collision activation. Check '$probeLog'."
    }

    Write-Host "Palm collision is active in PID $($gameProcess.Id)."
    Write-Host "Probe log: $probeLog"
    Write-Host 'Run this focused headset gate, then close the game normally:'
    Write-Host '1. In open space, move both tracked hands around your torso and head. They must follow normally and must not stop on the player character body.'
    Write-Host '2. Press each palm slowly into a wall or table, then sweep sideways. The visible palm should stop at the surface and slide along it instead of crossing or snapping through.'
    Write-Host '3. Pull each hand back out of contact and repeat at another angle. Recovery must be immediate, without a hand remaining stuck or jumping to a body-side anchor unnecessarily.'
    Write-Host '4. Grab several small free props that previously appeared far from the hand. Move them in clear space and confirm the selected point stays aligned with the owning palm instead of retaining native distance manipulation.'
    Write-Host '5. Repeat with one long wooden board/bar that previously behaved better. Then try one door, lever or other clearly jointed mechanism and confirm it keeps its native constrained motion instead of becoming a rigid free-body grab.'
    Write-Host '6. While holding a small eligible free prop, keep the hand clear for several seconds. The held prop itself must not push its owning palm backward or make the hand freeze; then release it and repeat wall/table contact.'
    Write-Host '7. Confirm stick locomotion, head translation and the other hand still behave normally. Close Black Plague when finished.'

    Wait-Process -Id $gameProcess.Id

    if (-not (Test-Path -LiteralPath $probeLog -PathType Leaf)) {
        throw "Probe log disappeared: '$probeLog'."
    }
    $pattern = 'palm_collision enabled=(?<enabled>[01]) source=(?<source>\w+) samples=(?<samples>\d+) published=(?<published>\d+) queries=(?<queries>\d+) contacts=(?<contacts>\d+) constrained=(?<constrained>\d+) held_body_skips=(?<held>\d+) stale_tracking=(?<stale>\d+) failures=(?<failures>\d+) creates=(?<creates>\d+) destroys=(?<destroys>\d+) world_replacements=(?<worlds>\d+)'
    [uint64]$samples = 0
    [uint64]$published = 0
    [uint64]$queries = 0
    [uint64]$contacts = 0
    [uint64]$constrained = 0
    [uint64]$held = 0
    [uint64]$failures = 0
    [uint64]$creates = 0
    $enabledSeen = $false
    foreach ($line in Get-Content -LiteralPath $probeLog) {
        if ($line -notmatch $pattern) { continue }
        if ($Matches.enabled -eq '1' -and $Matches.source -eq 'mutex') {
            $enabledSeen = $true
        }
        $samples += [uint64]$Matches.samples
        $published += [uint64]$Matches.published
        $queries += [uint64]$Matches.queries
        $contacts += [uint64]$Matches.contacts
        $constrained += [uint64]$Matches.constrained
        $held += [uint64]$Matches.held
        $failures += [uint64]$Matches.failures
        $creates += [uint64]$Matches.creates
    }

    Write-Host "Palm totals: samples=$samples published=$published queries=$queries contacts=$contacts constrained=$constrained held_body_skips=$held failures=$failures creates=$creates"
    if (-not $enabledSeen -or $samples -eq 0 -or $published -eq 0 -or
        $queries -eq 0 -or $creates -eq 0) {
        throw 'The tracked palm resolver did not produce the minimum live telemetry required by this gate.'
    }
    if ($failures -ne 0) {
        throw "The tracked palm resolver reported $failures query/resolver failures."
    }
    if ($contacts -eq 0 -or $constrained -eq 0) {
        throw 'No blocking palm contact was captured. Repeat the gate and hold wall/table contact for several seconds.'
    }
    if ($held -eq 0) {
        throw 'No per-hand held-body skip was captured. Repeat the gate while holding a small eligible free prop for several seconds.'
    }
    Write-Host 'Tracked palm live gate telemetry passed. Preserve the user comfort/contact observations with this log before promoting headset validation.'
}
finally {
    if ($null -ne $request) {
        $request.Dispose()
    }
}
