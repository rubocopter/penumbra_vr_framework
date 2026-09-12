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

function Get-FreshProbeLines {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [DateTime]$StartedAt
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return @()
    }

    $minimumTimestamp = $StartedAt.AddSeconds(-1)
    return @(Get-Content -LiteralPath $Path | Where-Object {
        if ($_.Length -lt 23) {
            return $false
        }
        $timestamp = [DateTime]::MinValue
        if (-not [DateTime]::TryParseExact(
                $_.Substring(0, 23),
                'yyyy-MM-dd HH:mm:ss.fff',
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::None,
                [ref]$timestamp)) {
            return $false
        }
        return $timestamp -ge $minimumTimestamp
    })
}

function Test-NonZeroHorizontalVector {
    param(
        [Parameter(Mandatory = $true)] [string]$Line,
        [Parameter(Mandatory = $true)] [string]$Field
    )

    $pattern = [regex]::Escape($Field) + '=\[([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\]'
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        return $false
    }

    $x = [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $z = [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
    return ([Math]::Sqrt(($x * $x) + ($z * $z))) -gt 0.00001
}

function Get-VectorField {
    param(
        [Parameter(Mandatory = $true)] [string]$Line,
        [Parameter(Mandatory = $true)] [string]$Field
    )

    $pattern = [regex]::Escape($Field) + '=\[([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\]'
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        return $null
    }

    return [pscustomobject]@{
        X = [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
        Y = [double]::Parse($match.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
        Z = [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
    }
}

function Get-HorizontalMagnitude {
    param(
        [Parameter(Mandatory = $true)] $Vector
    )

    return [Math]::Sqrt(($Vector.X * $Vector.X) + ($Vector.Z * $Vector.Z))
}

function Get-PhysicalScenarioEvidence {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Lines
    )

    $stationary = $false
    $free = $false
    $blocked = $false
    $slide = $false
    [UInt64]$stationarySamples = 0
    [UInt64]$freeSamples = 0
    [UInt64]$blockedSamples = 0
    [UInt64]$slideSamples = 0

    # Body telemetry is emitted from the already-owned native D6E00 tick. A
    # stationary sample requires no native horizontal request/acceptance and no
    # physical injection. Physical cases compare the injected X/Z request with
    # displacement accepted from the pre-injection position, so native stick
    # locomotion that occurred earlier in the tick is excluded.
    foreach ($line in $Lines | Where-Object { $_ -like '*body_collision *' }) {
        $nativeRequested = Get-VectorField -Line $line -Field 'requested_delta'
        $nativeAccepted = Get-VectorField -Line $line -Field 'accepted_delta'
        if ($null -ne $nativeRequested -and $null -ne $nativeAccepted -and
            $line -match 'physical_consumed=0 physical_injected=0' -and
            (Get-HorizontalMagnitude -Vector $nativeRequested) -le 0.00025 -and
            (Get-HorizontalMagnitude -Vector $nativeAccepted) -le 0.00025) {
            $stationary = $true
            $stationarySamples++
        }

        if ($line -notmatch 'physical_consumed=1 physical_injected=1') {
            continue
        }

        $requested = Get-VectorField -Line $line -Field 'physical_requested'
        $accepted = Get-VectorField -Line $line -Field 'physical_accepted'
        if ($null -eq $requested -or $null -eq $accepted) {
            continue
        }

        $requestedMagnitude = Get-HorizontalMagnitude -Vector $requested
        if ($requestedMagnitude -lt 0.00025) {
            continue
        }

        $acceptedMagnitude = Get-HorizontalMagnitude -Vector $accepted
        $residualX = $requested.X - $accepted.X
        $residualZ = $requested.Z - $accepted.Z
        $rejectedMagnitude = [Math]::Sqrt(($residualX * $residualX) + ($residualZ * $residualZ))
        $tolerance = [Math]::Max(0.00010, $requestedMagnitude * 0.15)

        if ($rejectedMagnitude -le $tolerance) {
            $free = $true
            $freeSamples++
        }
        elseif ($acceptedMagnitude -le $tolerance) {
            $blocked = $true
            $blockedSamples++
        }
        else {
            # Any meaningful accepted component plus meaningful rejection is
            # useful evidence for slide/partial acceptance. Direction-specific
            # collision classification stays native and is not inferred here.
            $slide = $true
            $slideSamples++
        }
    }

    return [pscustomobject]@{
        Stationary = $stationary
        Free = $free
        Blocked = $blocked
        Slide = $slide
        StationarySamples = $stationarySamples
        FreeSamples = $freeSamples
        BlockedSamples = $blockedSamples
        SlideSamples = $slideSamples
    }
}

function Get-MaxCounter {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Lines,
        [Parameter(Mandatory = $true)] [string]$Prefix,
        [Parameter(Mandatory = $true)] [string]$Counter
    )

    [UInt64]$maximum = 0
    $pattern = '\b' + [regex]::Escape($Counter) + '=(\d+)'
    foreach ($line in $Lines | Where-Object { $_ -like "*$Prefix*" }) {
        $match = [regex]::Match($line, $pattern)
        if ($match.Success) {
            $value = [UInt64]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
            if ($value -gt $maximum) {
                $maximum = $value
            }
        }
    }
    return $maximum
}

if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Release launcher not found at '$launcher'. Build the framework first."
}
if (-not (Test-Path -LiteralPath $GamePath -PathType Leaf)) {
    throw "Black Plague executable not found at '$GamePath'."
}

if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $defaultImage = Join-Path $repoRoot 'artifacts\black-plague-22000-live.bin'
    if (-not (Test-Path -LiteralPath $defaultImage -PathType Leaf)) {
        throw "Initialized exact-build image not found at '$defaultImage'. Pass -ImagePath <capture> explicitly; physical displacement validation must not skip the binary-evidence gate."
    }
    $ImagePath = $defaultImage
}

Write-Host 'Validating exact-build initialized image before physical displacement launch...'
& $verifier -ImagePath $ImagePath

$existingGame = @(Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue)
if ($existingGame.Count -ne 0) {
    throw 'A penumbra.exe process is already running. Close it before starting physical displacement validation so activation can be proven from a fresh game process.'
}

$requestName = 'Local\PenumbraVR.BlackPlague.PhysicalDisplacementValidation'
$request = New-Object System.Threading.Mutex -ArgumentList $false, $requestName
$gameProcess = $null
$exitCode = 1
try {
    Write-Host 'Exact-build verifier passed. Starting Black Plague with physical displacement validation requested.'
    Write-Host 'This mode injects only bounded X/Z validation requests through the existing native body tick.'
    Write-Host 'Positional HMD translation remains disabled.'
    $launchStartedAt = Get-Date
    & $launcher '--launch-vr' $GamePath
    $launcherExitCode = $LASTEXITCODE
    if ($launcherExitCode -ne 0) {
        throw "Probe launcher failed with exit code $launcherExitCode."
    }

    $deadline = (Get-Date).AddSeconds(60)
    do {
        Start-Sleep -Milliseconds 250
        $gameProcess = Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue |
            Select-Object -First 1
    } while ($null -eq $gameProcess -and (Get-Date) -lt $deadline)

    if ($null -eq $gameProcess) {
        throw 'Black Plague did not create penumbra.exe within 60 seconds. The physical displacement request was not validated.'
    }

    $probeLog = Join-Path $logRoot "black-plague-probe-$($gameProcess.Id).log"
    $activationDeadline = (Get-Date).AddSeconds(30)
    $activationLine = $null
    do {
        Start-Sleep -Milliseconds 100
        if (Test-Path -LiteralPath $probeLog -PathType Leaf) {
            $candidate = Get-Content -LiteralPath $probeLog |
                Select-String -Pattern 'Black Plague body adapter installed=.*physical_displacement_validation enabled=[01] source=(disabled|environment|mutex).*positional_translation_enabled=[01]' |
                Select-Object -Last 1
            if ($null -ne $candidate -and $candidate.Line.Length -ge 23) {
                $candidateTime = [DateTime]::ParseExact(
                    $candidate.Line.Substring(0, 23),
                    'yyyy-MM-dd HH:mm:ss.fff',
                    [Globalization.CultureInfo]::InvariantCulture)
                if ($candidateTime -ge $launchStartedAt.AddSeconds(-1)) {
                    $activationLine = $candidate.Line
                }
            }
        }
    } while ($null -eq $activationLine -and -not $gameProcess.HasExited -and (Get-Date) -lt $activationDeadline)

    if ($null -eq $activationLine) {
        throw "Black Plague PID $($gameProcess.Id) did not report physical-displacement activation within 30 seconds. Check '$probeLog'."
    }
    if ($activationLine -notmatch 'Black Plague body adapter installed=1 .*body_reconciliation_shadow enabled=1 source=physical_validation .*physical_displacement_validation enabled=1 source=mutex .*positional_translation_enabled=0') {
        throw "Black Plague PID $($gameProcess.Id) did not activate the requested physical displacement validation with positional translation disabled. Probe telemetry: $activationLine"
    }

    Write-Host "Black Plague detected (PID $($gameProcess.Id)); physical displacement validation is active via mutex."
    Write-Host "Probe log: $probeLog"
    Write-Host 'Validate stationary/free/block/slide while keeping this window open. Hold each case for several seconds so periodic body telemetry captures it.'
    Write-Host 'This run will only pass after the fresh log proves all four cases plus queue -> injection -> native collision consumption -> matched reconciliation.'
    $reportedScenarios = @{
        Stationary = $false
        Free = $false
        Blocked = $false
        Slide = $false
    }
    $allScenariosReported = $false
    while (-not $gameProcess.HasExited) {
        Start-Sleep -Milliseconds 500
        $progressLines = Get-FreshProbeLines -Path $probeLog -StartedAt $launchStartedAt
        $progress = Get-PhysicalScenarioEvidence -Lines $progressLines
        foreach ($scenario in @('Stationary', 'Free', 'Blocked', 'Slide')) {
            if ($progress.$scenario -and -not $reportedScenarios[$scenario]) {
                $reportedScenarios[$scenario] = $true
                $label = if ($scenario -eq 'Slide') { 'slide/partial' } else { $scenario.ToLowerInvariant() }
                Write-Host "Observed physical validation case: $label."
            }
        }
        if (-not $allScenariosReported -and
            $reportedScenarios.Stationary -and $reportedScenarios.Free -and
            $reportedScenarios.Blocked -and $reportedScenarios.Slide) {
            $allScenariosReported = $true
            Write-Host 'All four physical outcome classes are captured. Close the game when the session is complete; final queue/injection/reconciliation evidence will then be checked.'
        }
    }

    $freshLines = Get-FreshProbeLines -Path $probeLog -StartedAt $launchStartedAt
    $queuedPlans = Get-MaxCounter -Lines $freshLines -Prefix 'physical_displacement_validation ' -Counter 'queued'
    $matchedObservations = Get-MaxCounter -Lines $freshLines -Prefix 'physical_displacement_validation ' -Counter 'matched'
    $consumedRequests = Get-MaxCounter -Lines $freshLines -Prefix 'physical_displacement_boundary ' -Counter 'consumed'
    $injectedRequests = Get-MaxCounter -Lines $freshLines -Prefix 'physical_displacement_boundary ' -Counter 'injected'
    $scenarioEvidence = Get-PhysicalScenarioEvidence -Lines $freshLines

    $queuedVectorObserved = $false
    $boundaryVectorObserved = $false
    $bodyInjectionObserved = $false
    $nativeTickObserved = $false
    foreach ($line in $freshLines) {
        if ($line -like '*physical_displacement_validation *' -and
            (Test-NonZeroHorizontalVector -Line $line -Field 'requested')) {
            $queuedVectorObserved = $true
        }
        if ($line -like '*physical_displacement_boundary *' -and
            (Test-NonZeroHorizontalVector -Line $line -Field 'bounded')) {
            $boundaryVectorObserved = $true
        }
        if ($line -like '*body_collision *' -and
            $line -match 'physical_consumed=1 physical_injected=1' -and
            (Test-NonZeroHorizontalVector -Line $line -Field 'physical_requested') -and
            (Test-NonZeroHorizontalVector -Line $line -Field 'physical_injected_delta')) {
            $bodyInjectionObserved = $true
        }
        if ($line -like '*body_collision *' -and
            $line -match 'dt=0\.01666[0-9]') {
            $nativeTickObserved = $true
        }
    }

    $missingEvidence = @()
    if ($queuedPlans -eq 0 -or -not $queuedVectorObserved) {
        $missingEvidence += 'non-zero physical plan queued'
    }
    if ($consumedRequests -eq 0 -or $injectedRequests -eq 0 -or -not $boundaryVectorObserved) {
        $missingEvidence += 'physical request consumed and injected at the pre-collision boundary'
    }
    if ($matchedObservations -eq 0) {
        $missingEvidence += 'matched physical observation/reconciliation'
    }
    if (-not $bodyInjectionObserved) {
        $missingEvidence += 'body telemetry with physical_consumed=1, physical_injected=1 and non-zero request/injection'
    }
    if (-not $nativeTickObserved) {
        $missingEvidence += 'existing native body tick at dt~=1/60'
    }
    if (-not $scenarioEvidence.Stationary) {
        $missingEvidence += 'stationary body baseline with no native or physical X/Z displacement'
    }
    if (-not $scenarioEvidence.Free) {
        $missingEvidence += 'free physical displacement with the injected X/Z request substantially accepted'
    }
    if (-not $scenarioEvidence.Blocked) {
        $missingEvidence += 'blocked physical displacement with the injected X/Z request substantially rejected'
    }
    if (-not $scenarioEvidence.Slide) {
        $missingEvidence += 'slide/partial physical displacement with both accepted and rejected X/Z components'
    }

    if ($missingEvidence.Count -ne 0) {
        throw "Physical displacement live validation is incomplete for PID $($gameProcess.Id). Missing fresh evidence: $($missingEvidence -join '; '). Probe log: '$probeLog'."
    }

    Write-Host "Physical displacement evidence passed: queued=$queuedPlans consumed=$consumedRequests injected=$injectedRequests matched=$matchedObservations."
    Write-Host "Scenario evidence: stationary=$($scenarioEvidence.StationarySamples) free=$($scenarioEvidence.FreeSamples) blocked=$($scenarioEvidence.BlockedSamples) slide_or_partial=$($scenarioEvidence.SlideSamples)."
    $exitCode = 0
}
finally {
    $request.Dispose()
}

exit $exitCode
