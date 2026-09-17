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
try {
    & $verifier -ImagePath $ImagePath
}
catch {
    throw "Exact-build palm verifier failed: $($_.Exception.Message)"
}

$existingGame = @(Get-Process -Name 'penumbra' -ErrorAction SilentlyContinue)
if ($existingGame.Count -ne 0) {
    throw 'A penumbra.exe process is already running. Close it first; this validation request is sampled when the probe initializes.'
}

$mirrorExitCode = 0
& $launcher '--set-vr-mirror' 'on'
$mirrorExitCode = $LASTEXITCODE
if ($mirrorExitCode -ne 0) {
    throw 'Could not enable the persisted monitor mirror for palm/room-scale validation.'
}

$physicalRequest = New-Object System.Threading.Mutex -ArgumentList $false,
    'Local\PenumbraVR.BlackPlague.PhysicalDisplacementValidation'
$roomScaleRequest = New-Object System.Threading.Mutex -ArgumentList $false,
    'Local\PenumbraVR.BlackPlague.RoomScaleValidation'
$palmRequest = New-Object System.Threading.Mutex -ArgumentList $false,
    'Local\PenumbraVR.BlackPlague.PalmCollisionValidation'
$gameProcess = $null
try {
    Write-Host 'Starting Black Plague with tracked palms on the validated physical-displacement/room-scale stack.'
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
    $palmActivation = $null
    $roomScaleActivation = $null
    Write-Host 'Load/continue into gameplay. The palm resolver is sampled from the first live game-thread body service, so this validation request will remain alive until gameplay activates it or the game exits.'
    do {
        Start-Sleep -Milliseconds 200
        if (Test-Path -LiteralPath $probeLog -PathType Leaf) {
            $palmActivation = Get-Content -LiteralPath $probeLog |
                Select-String -Pattern 'palm_collision enabled=1 source=mutex ' |
                Select-Object -Last 1
            $roomScaleActivation = Get-Content -LiteralPath $probeLog |
                Select-String -Pattern 'Black Plague body adapter installed=.*physical_displacement_validation enabled=1 source=mutex.*room_scale_validation enabled=1 source=mutex.*positional_translation_enabled=1' |
                Select-Object -Last 1
        }
    } while (($null -eq $palmActivation -or $null -eq $roomScaleActivation) -and
             -not $gameProcess.HasExited)
    if ($null -eq $palmActivation -or $null -eq $roomScaleActivation) {
        throw "PID $($gameProcess.Id) did not report the combined palm + room-scale activation. Check '$probeLog'."
    }

    Write-Host "Palm collision and reconciled room-scale translation are active in PID $($gameProcess.Id)."
    Write-Host "Probe log: $probeLog"
    Write-Host 'Run this combined headset gate, then close the game normally:'
    Write-Host '0. In a tracked menu, point with a controller and activate one normal UI item. Confirm the pointer works and note whether that controller gives the short UI haptic pulse.'
    Write-Host '1. Before touching props, physically translate 5-10 cm in X/Z and crouch/stand once. Room movement, tracked Y and overall embodied feel should match the previously good room-scale build.'
    Write-Host '2. Inspect both imported Rework hand meshes in open space. Scale/orientation should look natural, each mesh must stay on its tracked/resolved palm, and thumb/index/middle/ring/little should visibly articulate. Also close/open the grip once so the authored held-tool finger pose can be judged separately from free curls.'
    Write-Host '3. In open space, move both tracked hands around your torso and head. They must follow normally and must not stop on the player character body.'
    Write-Host '4. Without stick input, physically move/lean the player body gently into a wall until horizontal room-scale motion is rejected, hold light pressure for a few seconds, then slide parallel to it. The view/body must remain vertically stable: no repeated mini-jumps or collision bounce.'
    Write-Host '5. Press each palm slowly into a wall or table, then sweep sideways several times. The visible palm should stop at the surface and slide along it. If it catches or snaps back, note the exact gesture/timing so the reanchor/recovery counters can be correlated with it.'
    Write-Host '6. Pull each hand back out of contact and repeat at another angle. Recovery must be immediate, without a hand remaining stuck or jumping to a body-side anchor unnecessarily.'
    Write-Host '7. Make several quick Grab=6 attempts on small free props that were previously hard to acquire, including one while the palm is lightly collision-constrained near a table/wall. Selection may follow the real controller up to the Rework 18 cm bound and may use the bounded interaction-assist skin while moving toward the target; once held, the object must stay aligned with the owning palm.'
    Write-Host '8. Without grabbing it first, tap or sweep a hand into a light movable prop so the direct hand-nudge path can move it. Note whether the contacting controller gives a subtle interaction haptic.'
    Write-Host '9. Repeat with one long wooden board/bar that previously behaved better. Then try one door, lever or other clearly jointed mechanism and confirm it keeps its native constrained motion instead of becoming a rigid free-body grab. Try several movable interactions so the log has a chance to exercise both Grab=6 and Move=2 ownership.'
    Write-Host '10. If the glowstick is available, toggle it and use it for several seconds. Check the corrected Rework profile specifically: it should no longer sit inside the palm or look obviously undersized, the fingers should wrap plausibly around it, and the off hand should receive the short LightToggle haptic after the native state actually changes. If only the flashlight is available, verify its BP-specific socket/orientation and the same off-hand toggle feedback.'
    Write-Host '11. If a melee weapon is available, land at least one clear hit on an enemy or solid physical prop. The dominant controller should receive the Rework MeleeImpact pulse only after real contact; swinging through empty space must not pulse.'
    Write-Host '12. While holding a small eligible free prop, keep the hand clear for several seconds. The held prop itself must not push its owning palm backward or make the hand freeze; then release it and repeat wall/table contact.'
    Write-Host '13. If a genuinely low ceiling/overhang is nearby, crouch underneath it and attempt to stand. The game must not oscillate, bounce or corrupt body height; note whether standing is blocked/retried until clear. This is optional coverage because not every test area has suitable geometry.'
    Write-Host '14. Repeat the short physical translation/crouch check after the interactions. Physically walk over one low static beam/step that previously blocked room-scale traversal, then traverse an ordinary stair/ledge with the stick. Physical low static geometry should use native step while stick traversal remains normal; wall pressure must not produce repeated mini-jumps. If you can enter a Push/Move interaction, apply brief stick input there too and note whether movement feels constrained rather than full-speed. Then close Black Plague normally.'

    Wait-Process -Id $gameProcess.Id

    if (-not (Test-Path -LiteralPath $probeLog -PathType Leaf)) {
        throw "Probe log disappeared: '$probeLog'."
    }
    $pattern = 'palm_collision enabled=(?<enabled>[01]) source=(?<source>\w+) samples=(?<samples>\d+) published=(?<published>\d+) queries=(?<queries>\d+) contacts=(?<contacts>\d+) constrained=(?<constrained>\d+) tracking_reanchors=(?<reanchors>\d+) recovery_anchors=(?<recoveries>\d+) pullback_recoveries=(?<pullbackRecoveries>\d+) interaction_assist=(?<interactionAssist>\d+) held_body_skips=(?<held>\d+) stale_tracking=(?<stale>\d+) failures=(?<failures>\d+) creates=(?<creates>\d+) destroys=(?<destroys>\d+) world_replacements=(?<worlds>\d+)'
    [uint64]$samples = 0
    [uint64]$published = 0
    [uint64]$queries = 0
    [uint64]$contacts = 0
    [uint64]$constrained = 0
    [uint64]$trackingReanchors = 0
    [uint64]$recoveryAnchors = 0
    [uint64]$pullbackRecoveries = 0
    [uint64]$interactionAssist = 0
    [uint64]$held = 0
    [uint64]$failures = 0
    [uint64]$creates = 0
    $enabledSeen = $false
    $roomScaleApplied = $false
    $trackedCrouch = $false
    [uint64]$physicalStepSuppressed = 0
    [uint64]$physicalCrouchEntries = 0
    [uint64]$physicalCrouchExits = 0
    [uint64]$nativeCrouchEntries = 0
    [uint64]$nativeCrouchExits = 0
    [uint64]$standRetries = 0
    [uint64]$crouchMismatchFrames = 0
    [uint64]$toolsAttached = 0
    [uint64]$toolsNative = 0
    [uint64]$invalidToolPose = 0
    [uint64]$blockedUnsafeGrabs = 0
    [uint64]$grabsAcquired = 0
    [uint64]$grabsReleased = 0
    [uint64]$movesAcquired = 0
    [uint64]$movesReleased = 0
    [uint64]$guardedReleases = 0
    [uint64]$collisionRestoreFailures = 0
    [uint64]$nudgeQueries = 0
    [uint64]$nudgeContacts = 0
    [uint64]$nudgesApplied = 0
    [uint64]$interactPresses = 0
    [uint64]$selectionRefreshes = 0
    [uint64]$selectionRayBatches = 0
    [uint64]$selectionRays = 0
    [uint64]$selectionCandidates = 0
    [uint64]$selectionWinnerMm = 0
    [uint64]$grabEnters = 0
    [uint64]$moveEnters = 0
    [uint64]$grabPending = 0
    [uint64]$movePending = 0
    [uint64]$magneticQueries = 0
    [uint64]$magneticCandidates = 0
    [uint64]$magneticVisibilityRays = 0
    [uint64]$magneticWinners = 0
    [uint64]$mechanismAcquired = 0
    [uint64]$mechanismUpdates = 0
    [uint64]$mechanismRejected = 0
    [uint64]$uiHapticSubmitted = 0
    [uint64]$uiHapticAttempted = 0
    [uint64]$pickupHapticSubmitted = 0
    [uint64]$pickupHapticAttempted = 0
    [uint64]$dropHapticSubmitted = 0
    [uint64]$dropHapticAttempted = 0
    [uint64]$interactionHapticSubmitted = 0
    [uint64]$interactionHapticAttempted = 0
    [uint64]$lightHapticSubmitted = 0
    [uint64]$lightHapticAttempted = 0
    [uint64]$meleeHapticSubmitted = 0
    [uint64]$meleeHapticAttempted = 0
    [uint64]$damageHapticSubmitted = 0
    [uint64]$damageHapticAttempted = 0
    [uint64]$leftHapticSubmitted = 0
    [uint64]$rightHapticSubmitted = 0
    [uint64]$hapticSubmitFailures = 0
    [uint64]$footstepCadence = 0
    [uint64]$footstepAttempts = 0
    [uint64]$footstepSuccess = 0
    [uint64]$footstepRejected = 0
    [uint64]$footstepAbiFailures = 0
    foreach ($line in Get-Content -LiteralPath $probeLog) {
        if ($line -like '*render_world_calls=*' -and
            $line -match 'room_scale_enabled=1 room_scale_sample_valid=1 positional_translation_applied=1') {
            $roomScaleApplied = $true
        }
        if ($line -like '*physical_crouch *' -and
            $line -match 'enabled=1 tracking_valid=1') {
            $trackedCrouch = $true
        }
        if ($line -match 'physical_crouch .* entries=(?<entries>\d+) exits=(?<exits>\d+) .* native_entries=(?<nativeEntries>\d+) native_exits=(?<nativeExits>\d+) stand_retries=(?<standRetries>\d+) mismatch_frames=(?<mismatchFrames>\d+)') {
            $value = [uint64]$Matches.entries
            if ($value -gt $physicalCrouchEntries) { $physicalCrouchEntries = $value }
            $value = [uint64]$Matches.exits
            if ($value -gt $physicalCrouchExits) { $physicalCrouchExits = $value }
            $value = [uint64]$Matches.nativeEntries
            if ($value -gt $nativeCrouchEntries) { $nativeCrouchEntries = $value }
            $value = [uint64]$Matches.nativeExits
            if ($value -gt $nativeCrouchExits) { $nativeCrouchExits = $value }
            $value = [uint64]$Matches.standRetries
            if ($value -gt $standRetries) { $standRetries = $value }
            $value = [uint64]$Matches.mismatchFrames
            if ($value -gt $crouchMismatchFrames) { $crouchMismatchFrames = $value }
        }
        if ($line -like '*body_collision *' -and
            $line -match 'physical_step_suppressed=1') {
            ++$physicalStepSuppressed
        }
        if ($line -match 'spatial tools_attached=(?<toolsAttached>\d+) tools_native=(?<toolsNative>\d+) invalid_tool_pose=(?<invalidToolPose>\d+) blocked_unsafe_grabs=(?<blockedUnsafeGrabs>\d+) grabs_acquired=(?<grabsAcquired>\d+) grabs_released=(?<grabsReleased>\d+) moves_acquired=(?<movesAcquired>\d+) moves_released=(?<movesReleased>\d+) guarded_releases=(?<guardedReleases>\d+) collision_restore_failures=(?<collisionRestoreFailures>\d+) contact_rays=(?<contactRays>\d+) nudge_queries=(?<nudgeQueries>\d+) nudge_contacts=(?<nudgeContacts>\d+) nudges_applied=(?<nudgesApplied>\d+) interact_presses=(?<interactPresses>\d+) selection_refreshes=(?<selectionRefreshes>\d+) selection_ray_batches=(?<selectionRayBatches>\d+) selection_rays=(?<selectionRays>\d+) selection_candidates=(?<selectionCandidates>\d+) selection_discards=(?<selectionDiscards>\d+) selection_winner_mm=(?<selectionWinnerMm>\d+) selection_central_rays=(?<selectionCentralRays>\d+) selection_auxiliary_rays=(?<selectionAuxiliaryRays>\d+) grab_enters=(?<grabEnters>\d+) move_enters=(?<moveEnters>\d+) grab_pending=(?<grabPending>\d+) move_pending=(?<movePending>\d+) magnetic_queries=(?<magneticQueries>\d+) magnetic_candidates=(?<magneticCandidates>\d+) magnetic_visibility_rays=(?<magneticVisibilityRays>\d+) magnetic_winners=(?<magneticWinners>\d+) mechanism_acquired=(?<mechanismAcquired>\d+) mechanism_updates=(?<mechanismUpdates>\d+) mechanism_rejected=(?<mechanismRejected>\d+)') {
            $toolsAttached += [uint64]$Matches.toolsAttached
            $toolsNative += [uint64]$Matches.toolsNative
            $invalidToolPose += [uint64]$Matches.invalidToolPose
            $blockedUnsafeGrabs += [uint64]$Matches.blockedUnsafeGrabs
            $grabsAcquired += [uint64]$Matches.grabsAcquired
            $grabsReleased += [uint64]$Matches.grabsReleased
            $movesAcquired += [uint64]$Matches.movesAcquired
            $movesReleased += [uint64]$Matches.movesReleased
            $guardedReleases += [uint64]$Matches.guardedReleases
            $collisionRestoreFailures += [uint64]$Matches.collisionRestoreFailures
            $nudgeQueries += [uint64]$Matches.nudgeQueries
            $nudgeContacts += [uint64]$Matches.nudgeContacts
            $nudgesApplied += [uint64]$Matches.nudgesApplied
            $interactPresses += [uint64]$Matches.interactPresses
            $selectionRefreshes += [uint64]$Matches.selectionRefreshes
            $selectionRayBatches += [uint64]$Matches.selectionRayBatches
            $selectionRays += [uint64]$Matches.selectionRays
            $selectionCandidates += [uint64]$Matches.selectionCandidates
            $winner = [uint64]$Matches.selectionWinnerMm
            if ($winner -gt 0 -and ($selectionWinnerMm -eq 0 -or $winner -lt $selectionWinnerMm)) {
                $selectionWinnerMm = $winner
            }
            $grabEnters += [uint64]$Matches.grabEnters
            $moveEnters += [uint64]$Matches.moveEnters
            $grabPending += [uint64]$Matches.grabPending
            $movePending += [uint64]$Matches.movePending
            $magneticQueries += [uint64]$Matches.magneticQueries
            $magneticCandidates += [uint64]$Matches.magneticCandidates
            $magneticVisibilityRays += [uint64]$Matches.magneticVisibilityRays
            $magneticWinners += [uint64]$Matches.magneticWinners
            $mechanismAcquired += [uint64]$Matches.mechanismAcquired
            $mechanismUpdates += [uint64]$Matches.mechanismUpdates
            $mechanismRejected += [uint64]$Matches.mechanismRejected
        }
        if ($line -match 'haptics ui_select=(?<uiSubmitted>\d+)/(?<uiAttempted>\d+) pickup=(?<pickupSubmitted>\d+)/(?<pickupAttempted>\d+) drop=(?<dropSubmitted>\d+)/(?<dropAttempted>\d+) interaction=(?<interactionSubmitted>\d+)/(?<interactionAttempted>\d+) light=(?<lightSubmitted>\d+)/(?<lightAttempted>\d+) melee=(?<meleeSubmitted>\d+)/(?<meleeAttempted>\d+) damage=(?<damageSubmitted>\d+)/(?<damageAttempted>\d+) left=(?<leftSubmitted>\d+) right=(?<rightSubmitted>\d+) policy_rejected=(?<policyRejected>\d+) submit_failed=(?<submitFailed>\d+)') {
            $uiHapticSubmitted += [uint64]$Matches.uiSubmitted
            $uiHapticAttempted += [uint64]$Matches.uiAttempted
            $pickupHapticSubmitted += [uint64]$Matches.pickupSubmitted
            $pickupHapticAttempted += [uint64]$Matches.pickupAttempted
            $dropHapticSubmitted += [uint64]$Matches.dropSubmitted
            $dropHapticAttempted += [uint64]$Matches.dropAttempted
            $interactionHapticSubmitted += [uint64]$Matches.interactionSubmitted
            $interactionHapticAttempted += [uint64]$Matches.interactionAttempted
            $lightHapticSubmitted += [uint64]$Matches.lightSubmitted
            $lightHapticAttempted += [uint64]$Matches.lightAttempted
            $meleeHapticSubmitted += [uint64]$Matches.meleeSubmitted
            $meleeHapticAttempted += [uint64]$Matches.meleeAttempted
            $damageHapticSubmitted += [uint64]$Matches.damageSubmitted
            $damageHapticAttempted += [uint64]$Matches.damageAttempted
            $leftHapticSubmitted += [uint64]$Matches.leftSubmitted
            $rightHapticSubmitted += [uint64]$Matches.rightSubmitted
            $hapticSubmitFailures += [uint64]$Matches.submitFailed
        }
        if ($line -match 'vr_footstep cadence=(?<cadence>\d+) attempts=(?<attempts>\d+) success=(?<success>\d+) rejected=(?<rejected>\d+) abi_failures=(?<abiFailures>\d+) pending=(?<pending>[01]) tick=(?<tick>\d+)') {
            $footstepCadence += [uint64]$Matches.cadence
            $footstepAttempts += [uint64]$Matches.attempts
            $footstepSuccess += [uint64]$Matches.success
            $footstepRejected += [uint64]$Matches.rejected
            $footstepAbiFailures += [uint64]$Matches.abiFailures
        }
        if ($line -notmatch $pattern) { continue }
        if ($Matches.enabled -eq '1' -and $Matches.source -eq 'mutex') {
            $enabledSeen = $true
        }
        $samples += [uint64]$Matches.samples
        $published += [uint64]$Matches.published
        $queries += [uint64]$Matches.queries
        $contacts += [uint64]$Matches.contacts
        $constrained += [uint64]$Matches.constrained
        $trackingReanchors += [uint64]$Matches.reanchors
        $recoveryAnchors += [uint64]$Matches.recoveries
        $pullbackRecoveries += [uint64]$Matches.pullbackRecoveries
        $interactionAssist += [uint64]$Matches.interactionAssist
        $held += [uint64]$Matches.held
        $failures += [uint64]$Matches.failures
        $creates += [uint64]$Matches.creates
    }

    Write-Host "Palm totals: samples=$samples published=$published queries=$queries contacts=$contacts constrained=$constrained tracking_reanchors=$trackingReanchors recovery_anchors=$recoveryAnchors pullback_recoveries=$pullbackRecoveries interaction_assist=$interactionAssist held_body_skips=$held failures=$failures creates=$creates physical_step_suppressed=$physicalStepSuppressed"
    Write-Host "Crouch totals: physical_entries=$physicalCrouchEntries physical_exits=$physicalCrouchExits native_entries=$nativeCrouchEntries native_exits=$nativeCrouchExits stand_retries=$standRetries mismatch_frames=$crouchMismatchFrames"
    Write-Host "Spatial totals: tools_attached=$toolsAttached tools_native=$toolsNative invalid_tool_pose=$invalidToolPose blocked_unsafe_grabs=$blockedUnsafeGrabs grabs=$grabsAcquired/$grabsReleased moves=$movesAcquired/$movesReleased guarded_releases=$guardedReleases collision_restore_failures=$collisionRestoreFailures nudge_queries=$nudgeQueries nudge_contacts=$nudgeContacts nudges_applied=$nudgesApplied interact_presses=$interactPresses selection_refreshes=$selectionRefreshes selection_batches=$selectionRayBatches selection_rays=$selectionRays selection_candidates=$selectionCandidates nearest_selection_mm=$selectionWinnerMm grab_enters=$grabEnters grab_pending=$grabPending move_enters=$moveEnters move_pending=$movePending magnetic_queries=$magneticQueries magnetic_candidates=$magneticCandidates magnetic_visibility_rays=$magneticVisibilityRays magnetic_winners=$magneticWinners mechanism_acquired=$mechanismAcquired mechanism_updates=$mechanismUpdates mechanism_rejected=$mechanismRejected"
    Write-Host "Haptic totals: ui=$uiHapticSubmitted/$uiHapticAttempted pickup=$pickupHapticSubmitted/$pickupHapticAttempted drop=$dropHapticSubmitted/$dropHapticAttempted interaction=$interactionHapticSubmitted/$interactionHapticAttempted light=$lightHapticSubmitted/$lightHapticAttempted melee=$meleeHapticSubmitted/$meleeHapticAttempted damage=$damageHapticSubmitted/$damageHapticAttempted left=$leftHapticSubmitted right=$rightHapticSubmitted submit_failed=$hapticSubmitFailures"
    Write-Host "Footstep totals: cadence=$footstepCadence attempts=$footstepAttempts success=$footstepSuccess rejected=$footstepRejected abi_failures=$footstepAbiFailures"
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
    if ($physicalCrouchEntries -eq 0 -or $physicalCrouchExits -eq 0 -or
        $nativeCrouchEntries -eq 0 -or $nativeCrouchExits -eq 0) {
        throw 'No complete tracked physical-crouch/native-state entry and exit cycle was captured.'
    }
    if ($grabsAcquired -eq 0 -or $grabsReleased -eq 0) {
        throw 'No complete Grab=6 acquisition/release cycle was captured. Repeat the gate with at least one small free prop.'
    }
    if ($collisionRestoreFailures -ne 0) {
        throw "Spatial interaction reported $collisionRestoreFailures collision-restore failures."
    }
    if ($hapticSubmitFailures -ne 0) {
        throw "OpenVR rejected $hapticSubmitFailures haptic submissions during the combined gate."
    }
    if ($footstepAbiFailures -ne 0) {
        throw "Native VR footstep dispatch reported $footstepAbiFailures exact-ABI failures."
    }
    if (-not $roomScaleApplied -or -not $trackedCrouch) {
        throw 'The palm run did not prove that the known-good room-scale/tracked-crouch stack remained active. Do not use this session for palm promotion.'
    }
    if ($movesAcquired -eq 0) {
        Write-Warning 'No Move=2 free-body ownership was captured. The run can still close the palm/Grab gate, but Move=2 remains unexercised.'
    }
    if ($interactionAssist -eq 0 -and $interactPresses -ne 0) {
        Write-Warning 'No bounded interaction-assist sample was captured. A future pickup attempt should approach a nearby free prop while the palm is collision-constrained so the new Rework assist path is exercised.'
    }
    if ($nudgesApplied -eq 0) {
        Write-Warning 'No direct hand nudge was applied. Tap a movable prop with an empty hand on a future run if interaction-contact parity still needs headset evidence.'
    }
    if ($uiHapticSubmitted -eq 0) {
        Write-Warning 'No UI-select haptic was accepted by OpenVR. UI haptic headset evidence remains open.'
    }
    if ($pickupHapticSubmitted -eq 0 -or $dropHapticSubmitted -eq 0) {
        Write-Warning 'Pickup/drop haptic submission was not fully observed even though a Grab cycle ran. Haptic headset evidence remains open.'
    }
    if ($interactionHapticSubmitted -eq 0 -and $nudgesApplied -ne 0) {
        Write-Warning 'A hand nudge was applied but no interaction haptic was accepted by OpenVR. Inspect controller tracking/focus and the log.'
    }
    if ($toolsAttached -ne 0 -and $lightHapticSubmitted -eq 0) {
        Write-Warning 'A flashlight/glowstick attachment was observed but no LightToggle haptic was accepted by OpenVR. If the light was toggled during this run, inspect controller tracking/focus and the log.'
    }
    if ($meleeHapticSubmitted -eq 0) {
        Write-Warning 'No confirmed-contact MeleeImpact haptic was accepted by OpenVR. If a melee hit was attempted during this run, inspect controller tracking/focus and the log; otherwise this headset evidence remains open.'
    }
    if ($toolsAttached -eq 0) {
        Write-Warning 'No flashlight/glowstick attachment sample was captured. Tool geometry remains unexercised in this run.'
    }
    if ($standRetries -eq 0) {
        Write-Warning 'No blocked/retried stand was captured. Low-ceiling posture behavior remains an optional open headset edge.'
    }
    if ($footstepCadence -eq 0) {
        Write-Warning 'No >0.85 m accepted-VR footstep cadence event was captured. Surface-footstep headset evidence remains open.'
    } elseif ($footstepSuccess -eq 0) {
        Write-Warning 'VR footstep cadence fired but no native FootStep dispatch succeeded. Inspect rejected/ABI counters before promoting the cadence gate.'
    }
    Write-Host 'Combined palm/room-scale/Grab telemetry passed. Preserve the user comfort, hand/contact, interaction, tool and haptic observations with this log before promoting individual headset-validation gates.'
}
finally {
    if ($null -ne $palmRequest) { $palmRequest.Dispose() }
    if ($null -ne $roomScaleRequest) { $roomScaleRequest.Dispose() }
    if ($null -ne $physicalRequest) { $physicalRequest.Dispose() }
}
