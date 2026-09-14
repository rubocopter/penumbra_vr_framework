[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 2147483647)]
    [int]$ProcessId,
    [switch]$CollisionComfort
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$logRoot = Join-Path $env:LOCALAPPDATA 'PenumbraVR\logs'
$logPath = Join-Path $logRoot "black-plague-probe-$ProcessId.log"
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Black Plague probe log not found for PID $ProcessId."
}

$lines = @(Get-Content -LiteralPath $logPath)
$frames = @($lines | Where-Object { $_ -like '* frame=*render_world_calls=*' })
$failures = @($frames | Where-Object {
    $_ -match 'stereo_failed=1' -or
    $_ -match 'stereo_error=.+VRCompositorError' -or
    $_ -match 'stereo_error=.+OpenVR compositor call failed'
})
$gameplay = @($frames | Where-Object {
    $_ -match 'render_world_calls=[1-9][0-9]*' -or
    $_ -match 'stereo_frames=[1-9][0-9]*'
})
$focused = @($frames | Where-Object { $_ -match 'controller_focus=1' })
$presentation = @($frames | Where-Object {
    $_ -match 'presentation_pose_acquisitions=[1-9][0-9]*' -or
    $_ -match 'presentation_pose_reuses=[1-9][0-9]*' -or
    $_ -match 'presentation_pose_stale_rejects=[1-9][0-9]*'
})
$visibilityFailures = @($frames | Where-Object {
    $_ -match 'hmd_visibility_failures=[1-9][0-9]*'
})

Write-Host "Probe presentation analysis: PID $ProcessId"
Write-Host "frames=$($frames.Count) gameplay_frames=$($gameplay.Count) controller_focus_frames=$($focused.Count) presentation_frames=$($presentation.Count) visibility_failure_frames=$($visibilityFailures.Count) stereo_failure_frames=$($failures.Count)"

foreach ($line in @($gameplay | Select-Object -First 2)) {
    Write-Host "FIRST_GAMEPLAY: $line"
}
foreach ($line in @($failures | Select-Object -First 3)) {
    Write-Host "FIRST_FAILURE: $line"
}
foreach ($line in @($presentation | Select-Object -First 3)) {
    Write-Host "FIRST_PRESENTATION: $line"
}
foreach ($line in @($visibilityFailures | Select-Object -First 3)) {
    Write-Host "FIRST_VISIBILITY_FAILURE: $line"
}
if ($frames.Count -ne 0) {
    Write-Host "LAST_FRAME: $($frames[-1])"
}

if ($CollisionComfort) {
    function Get-Timestamp {
        param([string]$Line)
        if ($Line.Length -lt 23) { return $null }
        $timestamp = [DateTime]::MinValue
        if (-not [DateTime]::TryParseExact(
                $Line.Substring(0, 23),
                'yyyy-MM-dd HH:mm:ss.fff',
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::None,
                [ref]$timestamp)) {
            return $null
        }
        return $timestamp
    }

    function Get-VectorField {
        param([string]$Line, [string]$Field)
        $match = [regex]::Match(
            $Line,
            [regex]::Escape($Field) + '=\[([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\]')
        if (-not $match.Success) { return $null }
        return @(
            [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture),
            [double]::Parse($match.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture),
            [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture))
    }

    function Get-HorizontalMagnitude {
        param($Vector)
        return [Math]::Sqrt(($Vector[0] * $Vector[0]) + ($Vector[2] * $Vector[2]))
    }

    $bodySamples = @()
    foreach ($line in $lines | Where-Object {
        $_ -like '*body_collision *' -and
        $_ -match 'physical_consumed=1 physical_injected=1'
    }) {
        $requested = Get-VectorField $line 'physical_requested'
        $accepted = Get-VectorField $line 'physical_accepted'
        if ($null -eq $requested -or $null -eq $accepted) { continue }
        $requestedLength = Get-HorizontalMagnitude $requested
        if ($requestedLength -le 0.002) { continue }
        $acceptedAlong = (($accepted[0] * $requested[0]) +
            ($accepted[2] * $requested[2])) / $requestedLength
        $acceptedAlong = [Math]::Max(0.0, [Math]::Min($requestedLength, $acceptedAlong))
        $rejected = $requestedLength - $acceptedAlong
        $tolerance = [Math]::Max(0.00010, $requestedLength * 0.15)
        $outcome = if ($rejected -le $tolerance) {
            'free'
        } elseif ($acceptedAlong -le $tolerance) {
            'blocked'
        } else {
            'partial'
        }
        $bodySamples += [pscustomobject]@{
            Time = Get-Timestamp $line
            Outcome = $outcome
            Requested = $requestedLength
            AcceptedAlong = $acceptedAlong
            Rejected = $rejected
        }
    }

    $roomScaleFrames = @($frames | Where-Object {
        $_ -match 'room_scale_enabled=1 room_scale_sample_valid=1 positional_translation_applied=1'
    })
    $renderSamples = @()
    foreach ($line in $roomScaleFrames) {
        $prediction = Get-VectorField $line 'room_scale_render_prediction_m'
        $anchor = Get-VectorField $line 'room_scale_head_anchor_m'
        if ($null -eq $prediction -or $null -eq $anchor) { continue }
        [uint64]$generation = 0
        if ($line -match 'room_scale_generation=(\d+)') {
            $generation = [uint64]$Matches[1]
        }
        $renderSamples += [pscustomobject]@{
            Time = Get-Timestamp $line
            Generation = $generation
            Prediction = $prediction
            PredictionLength = Get-HorizontalMagnitude $prediction
            Anchor = $anchor
            BaseX = $anchor[0] - $prediction[0]
            BaseZ = $anchor[2] - $prediction[2]
        }
    }

    $transitions = @()
    for ($index = 1; $index -lt $renderSamples.Count; $index++) {
        $previous = $renderSamples[$index - 1]
        $current = $renderSamples[$index]
        if ($previous.Generation -eq 0 -or $previous.Generation -ne $current.Generation) { continue }
        $anchorDelta = [Math]::Sqrt(
            (($current.Anchor[0] - $previous.Anchor[0]) * ($current.Anchor[0] - $previous.Anchor[0])) +
            (($current.Anchor[2] - $previous.Anchor[2]) * ($current.Anchor[2] - $previous.Anchor[2])))
        $baseDelta = [Math]::Sqrt(
            (($current.BaseX - $previous.BaseX) * ($current.BaseX - $previous.BaseX)) +
            (($current.BaseZ - $previous.BaseZ) * ($current.BaseZ - $previous.BaseZ)))
        $transitions += [pscustomobject]@{
            Time = $current.Time
            AnchorDelta = $anchorDelta
            BaseDelta = $baseDelta
            PredictionBefore = $previous.PredictionLength
            PredictionAfter = $current.PredictionLength
        }
    }

    $outcomes = @($bodySamples | Group-Object Outcome | Sort-Object Name |
        ForEach-Object { "$($_.Name):$($_.Count)" }) -join ','
    $predictionResets = @($transitions | Where-Object {
        $_.BaseDelta -gt 0.0015 -and
        $_.PredictionBefore -gt 0.0015 -and
        $_.PredictionAfter -lt ($_.PredictionBefore * 0.5)
    })
    $maxAnchorDelta = if ($transitions.Count -eq 0) { 0.0 } else {
        ($transitions | Measure-Object AnchorDelta -Maximum).Maximum
    }
    $maxBaseDelta = if ($transitions.Count -eq 0) { 0.0 } else {
        ($transitions | Measure-Object BaseDelta -Maximum).Maximum
    }
    Write-Host "COLLISION_COMFORT body_samples=$($bodySamples.Count) outcomes=$outcomes room_scale_frames=$($roomScaleFrames.Count) transitions=$($transitions.Count) prediction_reset_candidates=$($predictionResets.Count) max_anchor_delta_m=$([Math]::Round($maxAnchorDelta, 5)) max_base_delta_m=$([Math]::Round($maxBaseDelta, 5))"
    foreach ($sample in @($predictionResets | Sort-Object AnchorDelta -Descending | Select-Object -First 10)) {
        Write-Host "COLLISION_RESET anchor_delta_m=$([Math]::Round($sample.AnchorDelta, 5)) base_delta_m=$([Math]::Round($sample.BaseDelta, 5)) prediction_before_m=$([Math]::Round($sample.PredictionBefore, 5)) prediction_after_m=$([Math]::Round($sample.PredictionAfter, 5))"
    }

    foreach ($outcome in @('blocked', 'partial')) {
        $collisionSamples = @($bodySamples | Where-Object Outcome -eq $outcome)
        if ($collisionSamples.Count -eq 0 -or $transitions.Count -eq 0) { continue }
        $nearReset = 0
        $nearLargeReset = 0
        foreach ($collision in $collisionSamples) {
            if ($null -eq $collision.Time) { continue }
            $nearest = $transitions |
                Sort-Object { [Math]::Abs(($_.Time - $collision.Time).TotalMilliseconds) } |
                Select-Object -First 1
            if ($null -eq $nearest) { continue }
            $distanceMs = [Math]::Abs(($nearest.Time - $collision.Time).TotalMilliseconds)
            if ($distanceMs -le 20 -and
                $nearest.PredictionBefore -gt 0.0015 -and
                $nearest.PredictionAfter -lt ($nearest.PredictionBefore * 0.5)) {
                $nearReset++
                if ($nearest.AnchorDelta -gt 0.003) { $nearLargeReset++ }
            }
        }
        Write-Host "COLLISION_CORRELATION outcome=$outcome samples=$($collisionSamples.Count) prediction_resets_within_20ms=$nearReset anchor_resets_over_3mm_within_20ms=$nearLargeReset"
    }
}
