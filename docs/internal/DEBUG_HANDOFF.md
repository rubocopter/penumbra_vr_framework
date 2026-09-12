# Debug handoff — known VR problem boundaries

This file prevents repeated symptom-level fixes from replacing evidence-backed investigation. Read it before revisiting any of these issues.

## 0. Hook and loader lifecycle

The 2026-09-12 maintenance pass made IAT writes transactional when protection
restoration fails, added complete/partial-state handling to OpenGL matrix
telemetry and spatial interaction, and retained the OpenVR loader handle after a
failed `FreeLibrary`. These changes are implemented and statically reviewed but
were not built or executed in that session.

The follow-up lifecycle hardening adds compensating shutdown after launcher
initialization failures and moves spatial interaction/native input teardown to
hook removal followed by callback quiescence waiting. These changes are also
only statically reviewed and do not change the validation state.

The probe DLL remains process-resident by policy. Active-callback counters only
cover wrappers after entry and are not proof that a DLL can be unloaded safely.
A future production bootstrap must retain resident lifetime or introduce a
stronger dispatch/quiescence protocol before calling `FreeLibrary` on the probe.

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
- Shared reconciliation planning/rebase, physical-rejection correction and
  locomotion-anchor carry are implemented in `vr_locomotion.*`. The Black
  Plague shadow consumer remains observation-only and default-off; a separate
  default-off physical request exists at `0xD7281` and is host-tested only.
- PID 28172 live-tested the shadow consumer through the transient mutex with positional translation still zero: stationary/small-head-motion planning, native free/block/slide, recenter/body replacement and the existing ~60 Hz single native tick were observed without a second `D6E00`.
- GitHub Actions has host-tested the current shared extraction on Windows x86: root metadata/Debug/Release tests pass, and the autonomous Overture Release regression job also passes.

### Do not try again

- Do not add a fake head collider.
- Do not translate the camera alone to simulate room-scale.
- Do not tune arbitrary positional multipliers to hide clipping or push-back.
- Do not add another owner for `MoveForward/MoveSideways` or `D460A`.
- Do not call `D6E00` from `BlackPlagueBodyAdapter`.
- Do not treat the shadow physical plan as accepted/rejected movement; it is not injected.

### Next evidence

The shared reconciliation phases and default-off BP shadow consumer are now
**live-tested** through PID 28172. The earlier forwarded-`LoadLibraryW` startup
race is also live-confirmed past the point that previously failed: the fresh
Steam-started process installed the bridge/body adapter and activated the shadow
from the transient mutex.

The shadow-only mode still plans without injection. Separately, the bounded
physical X/Z request boundary is now implemented and host-tested at exact-build
RVA `0xD7281`, immediately before the native X/Z comparison/collision path. It
injects at most `0.05 m` horizontally, forces Y to zero, is one-shot for the
current body, and is consumed by the existing single native tick without a
second `D6E00`. `MoveForward/MoveSideways` remains native acceleration state and
is not used as this metric displacement mechanism.

The next evidence is live validation of that dedicated physical path. Require
queued request → matched injection → native collision solver → measured
acceptance/rejection for stationary/free/block/slide cases, with the acceptance
baseline taken immediately before injection. Keep positional HMD translation
disabled until that mechanism has separate live evidence. Camera/bob remains
separate comfort work.

`tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1` now enforces that
gate rather than treating activation as success. It also classifies stationary,
free, blocked and slide/partial samples from the existing pre-injection physical
request/accepted-displacement fields. PID 18392 deliberately ended after
activation only and the helper correctly failed because no queued,
consumed/injected, reconciled or injected-body telemetry existed. Do not promote
the boundary from `host-tested` on activation evidence alone.

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
- Do not combine speed retuning with the first tracking/body reconciliation live gate.

### Next evidence

Live-validate the host-tested physical displacement boundary first. Then compare a deliberately scoped Black Plague VR locomotion policy against the proven Overture `1.5 / 2.25 m/s` behavior through the adapter, using accepted displacement rather than analog scaling as the correctness boundary.

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

None is required for the shadow-only tracking/body validation milestone. Keep native jump ownership intact. VR jump comfort/tuning can be a later isolated change after the body path is stable.

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

Black Plague and Rework use different DAE resources. Rework's exact grip constants therefore cannot be copied blindly. The Framework now shares the proven attachment composition rule (local model-to-hand orientation followed by translation of the measured grip point to the hand origin), while Black Plague keeps its own measured flashlight/glowstick points and +90-degree X orientation. Existing exact matrix tests prove that this refactor preserves the prior BP placement numerically. The current BP profile is still visibly wrong against provisional hand geometry, so it is not a definitive placement result.

### Do not try again

Do not tune the final socket against provisional hand geometry and then retune it again when definitive meshes/rigs land.

### Next evidence

Integrate definitive hand geometry, measure a per-game tool socket against that geometry, then validate light direction and model placement together.

## Debugging rule

For any repeated defect, write down:

`OBSERVED → REWORK BEHAVIOR → CURRENT FRAMEWORK BEHAVIOR → DIFFERENCE → EVIDENCE → ROOT CAUSE → MINIMAL FIX → VALIDATION`

If root cause is not supported by evidence, stop changing behavior and collect the missing evidence first.
