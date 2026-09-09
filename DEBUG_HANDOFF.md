# Debug handoff — known VR problem boundaries

This file prevents repeated symptom-level fixes from replacing evidence-backed investigation. Read it before revisiting any of these issues.

## 1. Black Plague positional head/body movement

### Observed

The user previously reported world displacement / apparent collision behavior while moving physically or interacting with long objects.

### Established facts

- Framework positional HMD translation is currently disabled (`0.0`).
- The framework does not currently own a verified Black Plague head/body capsule.
- Therefore the observed behavior cannot be explained by a Framework head collider that does not exist.
- Rework resolves physical head displacement through the native character body, 0.05 m steps, collision acceptance/rejection and head-anchor reconciliation.

### Do not try again

- Do not add a fake head collider.
- Do not translate the camera alone to simulate room-scale.
- Do not tune arbitrary positional multipliers to hide clipping or push-back.

### Next evidence

Map the exact Black Plague body/player structure, capsule representation, native movement/update boundary and collision-resolution result. Add telemetry for requested displacement, accepted displacement, body/feet position and head/body divergence before enabling translation.

## 2. Black Plague locomotion speed / timing

### Observed

VR movement has felt faster than the expected Rework behavior.

### Established facts

- Rework uses direct displacement policy: 1.5 m/s walk, 2.25 m/s sprint, with fixed 0.05 m physical steps.
- Black Plague's native movement path uses acceleration/limits around 3.0 / 4.5 m/s.
- Scaling the analog input changes acceleration/input magnitude; it does not make the native path Rework-equivalent.
- A timing diagnostic once showed 240 ButtonHandler callbacks for roughly 4 simulated seconds in about 2 wall seconds, but source inspection showed the handler is registered in two updater containers. That metric counts handler callbacks and is not evidence that the whole simulation runs at 2x.

### Do not try again

- Do not declare the issue fixed by changing `MoveSpeed` alone.
- Do not modify gravity, simulation rate or jump constants without measured evidence.

### Next evidence

Map the unique native player/body update tick and its accepted displacement. Then route Rework's shared displacement policy through a Black Plague body adapter.

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
