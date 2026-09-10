# Debug handoff — known VR problem boundaries

This file prevents repeated symptom-level fixes from replacing evidence-backed investigation. Read it before revisiting any of these issues.

## 1. Black Plague positional head/body movement

### Observed

The user previously reported world displacement / apparent collision behavior while moving physically or interacting with long objects.

### Established facts

- Framework positional HMD translation is currently disabled (`0.0`).
- Static initialized-image evidence now maps `cPlayer+0x274` to the native
  `iCharacterBody`, position `+0x48`, size `+0xC4`, active physics body
  `+0x23C` and world `+0x240`.
- The native HPL constructor selects sphere only when diameter approximately
  equals height; otherwise it creates a rotated cylinder. There is no separate
  Framework head collider or verified capsule object.
- The normal physics boundary is `D460A -> iCharacterBody::Update(D6E00)`;
  initial horizontal resolution is `D7312 -> CheckShapeWorldCollision(D4830)`.
- Read-only telemetry now correlates body/feet before/after, requested position,
  solver output, final accepted displacement, timestep and unapplied HMD offset.
  It was observed in PID 30896: stable identity, `0.70 x 1.65 m` cylinder,
  `dt=1/60`, free motion and three real collision-rejection cases. This is
  `live-tested` for the boundary only.
- Therefore the observed behavior cannot be explained by a Framework head collider that does not exist.
- Rework resolves physical head displacement through the native character body, 0.05 m steps, collision acceptance/rejection and head-anchor reconciliation.

### Do not try again

- Do not add a fake head collider.
- Do not translate the camera alone to simulate room-scale.
- Do not tune arbitrary positional multipliers to hide clipping or push-back.

### Next evidence

The live collision checklist is complete. Before enabling translation, identify
the native owners of camera/head-bob and footstep-bob, acceleration/deceleration,
jump and crouch; the collision telemetry cannot attribute those effects.

The first ownership attempt (PID 25776) is invalid: it installed the body probe
but rejected the ownership probe before it changed an instruction. The host hash
was the expected `FD316F...`; the rejection came from a probe-RVA mistake, not
from a different executable. `0x52CD` is `push 1`; the held-jump `CALL rel32`
is `0x52CF -> 0x9A890`. The corrected probe fail-closes only after validating
all eight calls and target signatures. Do not interpret any action in that log
as ownership telemetry, and do not retest symptoms until the corrected probe is
built and deployed. PID 24780 then installed both probes and is valid action/body
ownership evidence: native sprint begin/end each fired once, jump fired once
with 172 held updates, and crouch swapped the active body from 1.65 m to 0.95 m
height while retaining the feet position. Do not infer physical crouch from it.
`5347` is pressed and `538A` is release/not-held; toggle crouch legitimately
produces both. The zero `D790C/D7913` counts are expected for the gravity-active
player path, not proof that camera ownership is absent.

## 2. Black Plague locomotion speed / timing

### Observed

VR movement has felt faster than the expected Rework behavior.

### Established facts

- `cPlayer::MoveForward/MoveSideways` accumulate native speed, while the single
  `D460A -> D6E00` tick consumes/decelerates it and resolves collision. A BP
  adapter must not call `D6E00` directly: the normal physics tick would then
  update the same body twice.

- Rework uses direct displacement policy: 1.5 m/s walk, 2.25 m/s sprint, with fixed 0.05 m physical steps.
- Black Plague's native movement path uses acceleration/limits around 3.0 / 4.5 m/s.
- Scaling the analog input changes acceleration/input magnitude; it does not make the native path Rework-equivalent.
- A timing diagnostic showed 240 `HookedUpdate` callbacks for roughly 4 simulated seconds in about 2 wall seconds. That metric is specifically the hooked `cButtonHandler::Update` callback, while the body probe observed the normal 60 Hz `D460A` tick; it is not evidence that physics runs at 2x. The exact update-container ownership remains unproven.
- The real character timestep is now observable at the unique normal
  physics-world callsite `D460A`; `iCharacterBody::Move(D4F50)` only updates
  native acceleration/speed state and does not accept a displacement directly.

### Do not try again

- Do not declare the issue fixed by changing `MoveSpeed` alone.
- Do not modify gravity, simulation rate or jump constants without measured evidence.

### Next evidence

Do not infer equivalence from analog-input scaling. PID 29672 now closes native
jump characterization: state 3 enters before the next body tick, effective
initial vertical speed is ~5.53 m/s, apex ~0.95 m, and landing precedes the
following state `3 -> 0`. `+268` is the 25-tick ground-grace counter, not
current grounded; `+26C` remains unclassified. `+204` is a Jump hold threshold,
not a maximum: held `+200` exceeds it and release restores the count to it.
Keep jump/vertical ownership native when implementing horizontal policy. Camera
and bob remain separate work, not a blocker for the narrow body adapter.

## 3. Black Plague held-object collision

### Established facts

- Exact build: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- `iPhysicsBody +0x3C8` controls the native character-aware collision behavior observed at the mapped world/ray/contact boundaries.
- The current grab path snapshots the original value, disables it only while an eligible free body is owned, then restores the exact original value on release.
- Parent/joint bodies are excluded from the free-body path.
- The binary has no separate Rework-era player-only collision filter; this is deliberately conservative and still needs real-motor validation.

### Next evidence

Headset-test only small free bodies first. Confirm zero `collision_restore_failures`, no player displacement, correct restoration and no regressions to normal world collisions.

## 4. Long bars, doors and mechanisms

### Established facts

Rigid free-body palm-relative attachment is working conceptually for eligible bodies. Long tables/bars following the palm is expected from that model. Doors, levers, joints and parented bodies must use their native mechanism state.

### Do not try again

Do not add arbitrary springs, offsets or special-case rigid-body behavior to make a mechanism look physical.

### Next evidence

Map the exact joint/slider/hinge state and update boundary for one representative mechanism, then build a dedicated backend adapter.

## 5. Glowstick / flashlight placement

### Established facts

Black Plague and Rework use different DAE resources. Rework's exact grip constants therefore cannot be copied blindly. The current BP socket makes the glowstick follow the hand but places it visibly inside the provisional hand mesh.

### Do not try again

Do not retune the socket against provisional hand geometry and then retune it again when final hand meshes arrive.

### Next evidence

Integrate definitive hand geometry, measure the tool socket against that geometry, then validate light direction and model placement together.

## Debugging rule

For any repeated defect, write down:

`OBSERVED → REWORK BEHAVIOR → CURRENT FRAMEWORK BEHAVIOR → DIFFERENCE → EVIDENCE → ROOT CAUSE → MINIMAL FIX → VALIDATION`

If root cause is not supported by evidence, stop changing code and collect the missing evidence first.
