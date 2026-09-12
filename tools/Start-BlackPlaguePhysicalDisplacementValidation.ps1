[CmdletBinding()]
param(
    [string]$GamePath = (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Penumbra Black Plague\redist\penumbra.exe'),
    [string]$ImagePath = '',
    [switch]$EnableRoomScale
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

    # Rework 23c890f deliberately reacts to any non-zero HMD tracking delta.
    # Real headsets therefore keep producing tiny injected X/Z requests even
    # when the player is intentionally stationary. Use the shared 2 mm
    # reconciliation significance boundary to distinguish tracking jitter from
    # meaningful physical motion; do not require a mathematically zero request.
    $meaningfulPhysicalMotionMeters = 0.002
    $stationary = $false
    $free = $false
    $blocked = $false
    $slide = $false
    [UInt64]$stationarySamples = 0
    [UInt64]$freeSamples = 0
    [UInt64]$blockedSamples = 0
    [UInt64]$slideSamples = 0

    # Body telemetry is emitted from the already-owned native D6E00 tick.
    # Stationary means no meaningful horizontal native/physical displacement;
    # sub-2 mm injected tracking jitter is expected on real HMDs. Physical cases
    # compare the injected X/Z request with displacement accepted from the
    # pre-injection position, so native stick locomotion that occurred earlier
    # in the tick is excluded.
    foreach ($line in $Lines | Where-Object { $_ -like '*body_collision *' }) {
        $nativeRequested = Get-VectorField -Line $line -Field 'requested_delta'
        $nativeAccepted = Get-VectorField -Line $line -Field 'accepted_delta'
        $physicalRequested = Get-VectorField -Line $line -Field 'physical_requested'
        $physicalAccepted = Get-VectorField -Line $line -Field 'physical_accepted'
        $physicalInactive = $line -match 'physical_consumed=0 physical_injected=0'
        $physicalJitterOnly = $line -match 'physical_consumed=1 physical_injected=1' -and
            $null -ne $physicalRequested -and $null -ne $physicalAccepted -and
            (Get-HorizontalMagnitude -Vector $physicalRequested) -le $meaningfulPhysicalMotionMeters -and
            (Get-HorizontalMagnitude -Vector $physicalAccepted) -le $meaningfulPhysicalMotionMeters
        if ($null -ne $nativeRequested -and $null -ne $nativeAccepted -and
            ($physicalInactive -or $physicalJitterOnly) -and
            (Get-HorizontalMagnitude -Vector $nativeRequested) -le $meaningfulPhysicalMotionMeters -and
            (Get-HorizontalMagnitude -Vector $nativeAccepted) -le $meaningfulPhysicalMotionMeters) {
            $stationary = $true
            $stationarySamples++
        }

        if ($line -notmatch 'physical_consumed=1 physical_injected=1') {
            continue
        }

        $requested = $physicalRequested
        $accepted = $physicalAccepted
        if ($null -eq $requested -or $null -eq $accepted) {
            continue
        }

        $requestedMagnitude = Get-HorizontalMagnitude -Vector $requested
        if ($requestedMagnitude -lt $meaningfulPhysicalMotionMeters) {
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
$roomScaleRequest = $null
if ($EnableRoomScale) {
    & $launcher '--set-vr-mirror' 'on'
    if ($LASTEXITCODE -ne 0) {
        throw "Could not enable the persisted monitor mirror for room-scale validation."
    }
    $roomScaleRequest = New-Object System.Threading.Mutex -ArgumentList $false,
        'Local\PenumbraVR.BlackPlague.RoomScaleValidation'
}
$gameProcess = $null
$exitCode = 1
try {
    Write-Host 'Exact-build verifier passed. Starting Black Plague with physical displacement validation requested.'
    Write-Host 'This mode injects only bounded X/Z validation requests through the existing native body tick.'
    if ($EnableRoomScale) {
        Write-Host 'Active room-scale X/Z translation is enabled through the reconciled body path; Y and jump remain native.'
        Write-Host 'The saved monitor mirror is on so headset behavior can be observed on the PC.'
    } else {
        Write-Host 'Positional HMD translation remains disabled.'
    }
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
                Select-String -Pattern 'Black Plague body adapter installed=.*physical_displacement_validation enabled=[01] source=(disabled|environment|mutex).*room_scale_validation enabled=[01] source=(disabled|environment|mutex).*positional_translation_enabled=[01]' |
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
    $expectedActivation = if ($EnableRoomScale) {
        'Black Plague body adapter installed=1 .*body_reconciliation_shadow enabled=1 source=physical_validation .*physical_displacement_validation enabled=1 source=mutex .*room_scale_validation enabled=1 source=mutex positional_translation_enabled=1'
    } else {
        'Black Plague body adapter installed=1 .*body_reconciliation_shadow enabled=1 source=physical_validation .*physical_displacement_validation enabled=1 source=mutex .*room_scale_validation enabled=0 source=disabled positional_translation_enabled=0'
    }
    if ($activationLine -notmatch $expectedActivation) {
        throw "Black Plague PID $($gameProcess.Id) did not activate the requested validation mode. Probe telemetry: $activationLine"
    }

    Write-Host "Black Plague detected (PID $($gameProcess.Id)); physical displacement validation is active via mutex."
    Write-Host "Probe log: $probeLog"
    Write-Host 'Validate stationary/free/block/slide while keeping this window open. Hold each case for several seconds so periodic body telemetry captures it.'
    if ($EnableRoomScale) {
        Write-Host 'Also test stick locomotion, recenter once, crouch and stand once, then repeat free/block/slide after the native shape swap.'
        Write-Host 'Observe that the desktop mirror shows gameplay and that head motion, hands and world remain coherent.'
    }
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
    $roomScaleApplied = $false
    $nonZeroCameraOffset = $false
    $mirrorEnabled = $false
    $standingBodyObserved = $false
    $crouchedBodyObserved = $false
    $standingBodyRestored = $false
    [int]$standingBodyRestoredLineIndex = -1
    $roomScaleRecoveredAfterCrouch = $false
    for ($lineIndex = 0; $lineIndex -lt $freshLines.Count; $lineIndex++) {
        $line = $freshLines[$lineIndex]
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
        if ($line -like '*render_world_calls=*') {
            if ($line -match 'room_scale_enabled=1 room_scale_sample_valid=1 positional_translation_applied=1') {
                $roomScaleApplied = $true
                if ($standingBodyRestored) {
                    $roomScaleRecoveredAfterCrouch = $true
                }
            }
            if (Test-NonZeroHorizontalVector -Line $line -Field 'room_scale_camera_offset_m') {
                $nonZeroCameraOffset = $true
            }
            if ($line -match 'monitor_mirror=1') {
                $mirrorEnabled = $true
            }
        }
        if ($EnableRoomScale -and $line -like '*body_collision *') {
            $characterSize = Get-VectorField -Line $line -Field 'character_size'
            if ($null -ne $characterSize) {
                if ([Math]::Abs($characterSize.Y - 1.65) -le 0.05) {
                    if ($crouchedBodyObserved) {
                        $standingBodyRestored = $true
                        if ($standingBodyRestoredLineIndex -lt 0) {
                            $standingBodyRestoredLineIndex = $lineIndex
                        }
                    } else {
                        $standingBodyObserved = $true
                    }
                } elseif ($standingBodyObserved -and
                    [Math]::Abs($characterSize.Y - 0.95) -le 0.05) {
                    $crouchedBodyObserved = $true
                }
            }
        }
    }

    $postCrouchScenarioEvidence = $null
    if ($EnableRoomScale -and $standingBodyRestoredLineIndex -ge 0 -and
        $standingBodyRestoredLineIndex -lt $freshLines.Count) {
        $postCrouchLines = @($freshLines[
            $standingBodyRestoredLineIndex..($freshLines.Count - 1)])
        $postCrouchScenarioEvidence = Get-PhysicalScenarioEvidence `
            -Lines $postCrouchLines
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
    if ($EnableRoomScale -and -not $roomScaleApplied) {
        $missingEvidence += 'fresh reconciled room-scale sample applied to the rendered camera'
    }
    if ($EnableRoomScale -and -not $nonZeroCameraOffset) {
        $missingEvidence += 'non-zero horizontal room-scale camera offset'
    }
    if ($EnableRoomScale -and -not $mirrorEnabled) {
        $missingEvidence += 'monitor mirror enabled in a gameplay render frame'
    }
    if ($EnableRoomScale -and
        (-not $standingBodyObserved -or -not $crouchedBodyObserved -or
         -not $standingBodyRestored)) {
        $missingEvidence += 'native standing -> crouched -> standing shape sequence (1.65 m -> 0.95 m -> 1.65 m)'
    }
    if ($EnableRoomScale -and -not $roomScaleRecoveredAfterCrouch) {
        $missingEvidence += 'fresh room-scale camera application after returning to the standing shape'
    }
    if ($EnableRoomScale -and
        ($null -eq $postCrouchScenarioEvidence -or
         -not $postCrouchScenarioEvidence.Free -or
         -not $postCrouchScenarioEvidence.Blocked -or
         -not $postCrouchScenarioEvidence.Slide)) {
        $missingEvidence += 'free, blocked and slide/partial physical outcomes after returning to the standing shape'
    }

    if ($missingEvidence.Count -ne 0) {
        throw "Physical displacement live validation is incomplete for PID $($gameProcess.Id). Missing fresh evidence: $($missingEvidence -join '; '). Probe log: '$probeLog'."
    }

    Write-Host "Physical displacement evidence passed: queued=$queuedPlans consumed=$consumedRequests injected=$injectedRequests matched=$matchedObservations."
    Write-Host "Scenario evidence: stationary=$($scenarioEvidence.StationarySamples) free=$($scenarioEvidence.FreeSamples) blocked=$($scenarioEvidence.BlockedSamples) slide_or_partial=$($scenarioEvidence.SlideSamples)."
    if ($EnableRoomScale) {
        Write-Host "Room-scale evidence passed: camera_applied=$roomScaleApplied non_zero_offset=$nonZeroCameraOffset mirror=$mirrorEnabled crouch_shape_sequence=$standingBodyObserved/$crouchedBodyObserved/$standingBodyRestored recovered_after_crouch=$roomScaleRecoveredAfterCrouch."
        Write-Host "Post-crouch outcomes: free=$($postCrouchScenarioEvidence.FreeSamples) blocked=$($postCrouchScenarioEvidence.BlockedSamples) slide_or_partial=$($postCrouchScenarioEvidence.SlideSamples)."
    }
    $exitCode = 0
}
finally {
    if ($null -ne $roomScaleRequest) {
        $roomScaleRequest.Dispose()
    }
    $request.Dispose()
}

exit $exitCode
