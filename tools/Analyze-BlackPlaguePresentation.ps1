[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 2147483647)]
    [int]$ProcessId
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
