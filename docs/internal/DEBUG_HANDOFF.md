# Debug handoff — known VR problem boundaries

This file prevents repeated symptom-level fixes from replacing evidence-backed investigation. Read it before revisiting any of these issues.

## 1. Black Plague positional head/body movement

### Observed

The user previously reported world displacement / collision discomfort while moving physically in VR. Leaning the HMD can move the view while the native body remains elsewhere, producing substantial discomfort.

### Established facts

- Framework positional HMD translation is currently disabled (`0.0`).
- `cPlayer+0x274` maps to the native `iCharacterBody` on the supported exact build.
- Current/previous position, active size, physics body and physics world are mapped and live-observed.
- The active standing player shape is a `0.70 x 1.65 x 0.70 m` cylinder, radius `0.35 m`.
- The normal body update is `D460A -> iCharacterBody::Update(D6E00)` at `dt=1/60`.
- Horizontal collision begins at `D7312 -> CheckShapeWorldCollision(D4830)` and later native phases apply step/gravity/state work.
- PID 30896 live-observed free movement, total block, slide/partial acceptance and up to ~`0.436 m` unapplied HMD/body divergence.
- The first narrow `BlackPlagueBodyAdapter` is now live-tested. It never calls `D6E00`; it binds to the existing movement/body-update owners and observes accepted displacement after the one native tick.
- PID 8628 live-observed free movement, total blocking, sliding and body replacement with the adapter active. Native body updates remained ~60 Hz; no second `D6E00` was introduced.
- `runtime::VrAcceptedBodyMotion` is shared by Overture and Black Plague as the game-neutral accepted-displacement observation.

### Do not try again

- Do not add a fake head collider.
- Do not translate the camera alone to simulate room-scale.
- Do not tune arbitrary positional multipliers to hide clipping or push-back.
- Do not add another owner for `MoveForward/MoveSideways` or `D460A`.
- Do not call `D6E00` from `BlackPlagueBodyAdapter`.

### Next evidence

The body/collision mapping and narrow adapter are complete enough to stop probing by default. The next milestone is a deliberately scoped **tracking/body reconciliation** path through the live-tested adapter while positional HMD translation remains disabled during host validation.

Required evidence for the next live/headset gate:

1. shared policy publishes only through the existing horizontal intent boundary;
2. native body update remains exactly once per physics tick;
3. accepted displacement still distinguishes free movement, block and slide;
4. head/body anchor reconciliation uses accepted movement rather than speculative camera translation;
5. only after those hold should positional HMD translation be enabled and tested in headset.

Camera/head-bob/footstep-bob ownership remains separate comfort work and is not a reason to reopen body ownership.

## 2. Black Plague locomotion speed / timing

### Observed

VR movement has felt substantially faster than Overture/Rework and native walking/bobbing is uncomfortable in-headset.

### Established facts

- `cPlayer::MoveForward/MoveSideways` feed native movement state through `iCharacterBody::Move(D4F50)`.
- The native body update consumes/decelerates movement once at `D460A -> D6E00`.
- Effective native walk/sprint limits are about `3.0 / 4.5 m/s`.
- Rework uses direct VR displacement policy of `1.5 / 2.25 m/s`, with `0.05 m` physical steps.
- Scaling analog input only changes native input/acceleration magnitude; it is not equivalent to Rework's locomotion policy.
- The earlier ratio≈2 timing diagnostic measured `cButtonHandler::Update` callbacks, not physics. The body probe consistently observes the real body tick at ~60 Hz.
- The first adapter intentionally preserves native tuning. Speed policy has not yet been ported to Black Plague.

### Do not try again

- Do not declare the issue fixed by changing `MoveSpeed` alone.
- Do not alter simulation rate or gravity to compensate for perceived speed.
- Do not combine speed retuning with the first tracking/body reconciliation change unless the milestone explicitly scopes it.

### Next evidence

Stabilize the shared tracking/body reconciliation path first. Then compare a deliberately scoped Black Plague VR locomotion policy against the proven Overture `1.5 / 2.25 m/s` behavior through the adapter, using accepted displacement rather than analog scaling as the correctness boundary.

## 3. Black Plague jump

### Established facts

- Jump dispatch selects state index `3`, source-correlated with `cPlayerMoveState_Jump`.
- `EnterState` establishes vertical force before the following `D6E00`.
- First observed accepted vertical speed is ~`+5.53 m/s`; apex is ~`0.95 m` above baseline.
- Landing is resolved by the native body tick before state `3 -> 0` on the next state update.
- `requested.y` and the first horizontal solver Y stay zero throughout the burst; final accepted Y changes in the later native vertical/state path.
- `+268` is a 25-tick ground-grace counter.
- `+26C` remains an unclassified alternate jump-eligibility branch.
- `+204=0.3` is a Jump hold threshold, not a hard maximum for `+200`.

### Do not try again

- Do not fold vertical movement into the shared horizontal intent contract.
- Do not copy Overture jump constants into Black Plague without a separate design/validation milestone.
- Do not rename derived acceleration regimes as “gravity constants” without native ownership evidence.

### Next evidence

None is required for the first tracking/body reconciliation milestone. Keep native jump ownership intact. VR jump comfort/tuning can be a later isolated change after body reconciliation is stable.

## 4. Black Plague crouch

### Established facts

- Crouch is a real native body/shape transition, not merely a camera offset.
- Standing size: `0.70 x 1.65 x 0.70`.
- Crouched size: `0.70 x 0.95 x 0.70`.
- Feet Y remains fixed while body centre changes.
- The active physics-body pointer changes and restores.
- The exact Black Plague stand-clearance mechanism remains insufficiently demonstrated.

### Do not try again

- Do not import Overture `CanStand` semantics into Black Plague without evidence.
- Do not implement physical crouch as a camera-only height change.

### Next evidence

Physical crouch should be a separate milestone after tracking/body reconciliation. Map or expose the native stand-clearance mechanism before shared physical crouch is allowed to force a stand transition.

## 5. Black Plague held-object collision

### Established facts

- Exact build: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- `iPhysicsBody +0x3C8` controls the native character-aware collision behavior observed at the mapped world/ray/contact boundaries.
- The current grab path snapshots the original value, disables it only while an eligible free body is owned, then restores the exact original value on release.
- Parent/joint bodies are excluded from the free-body path.
- The binary has no separate Rework-era player-only collision filter demonstrated yet; the current implementation is intentionally conservative.

### Next evidence

Headset-test only small free bodies first. Confirm zero collision-restore failures, no player displacement, correct restoration and no regressions to normal world collisions.

## 6. Long bars, doors and mechanisms

### Established facts

Rigid free-body palm-relative attachment is appropriate only for eligible free bodies. Long tables/bars rigidly following the palm is a limitation of that model. Doors, levers, sliders and joints must use native mechanism state.

### Do not try again

Do not add arbitrary springs, offsets or special-case rigid-body behavior to make a mechanism look physical.

### Next evidence

Map the exact joint/slider/hinge state and update boundary for one representative mechanism, then build a dedicated backend adapter.

## 7. Glowstick / flashlight placement

### Established facts

Black Plague and Rework use different DAE resources. Rework's exact grip constants therefore cannot be copied blindly. The current BP socket follows the hand but is visibly wrong against provisional hand geometry.

### Do not try again

Do not tune the final socket against provisional hand geometry and then retune it again when definitive meshes/rigs land.

### Next evidence

Integrate definitive hand geometry, measure a per-game tool socket against that geometry, then validate light direction and model placement together.

## Debugging rule

For any repeated defect, write down:

`OBSERVED → REWORK BEHAVIOR → CURRENT FRAMEWORK BEHAVIOR → DIFFERENCE → EVIDENCE → ROOT CAUSE → MINIMAL FIX → VALIDATION`

If root cause is not supported by evidence, stop changing behavior and collect the missing evidence first.
