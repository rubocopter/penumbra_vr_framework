# Penumbra VR Framework — Agent instructions

Read this file before changing code. Then read `docs/internal/CODEX_HANDOFF.md` for the current checkpoint, `docs/internal/DEBUG_HANDOFF.md` for known regression boundaries, and `docs/REWORK_PORTING_PLAN.md` for the extraction contract.

## Core rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR.

Do not invent a replacement for behavior that Rework already demonstrates. First locate the exact Rework implementation and tests, separate game-neutral behavior from HPL/game-specific dependencies, port the proven behavior, and adapt only the narrow game boundary that differs.

A new algorithm is acceptable only when:

- Rework has no equivalent;
- the target exposes a technically incompatible interface; or
- evidence demonstrates that direct adaptation is unsafe or incorrect.

Record that reason in the relevant migration/research document.

Rework is not automatically the best implementation of every game-neutral subsystem. When another backend demonstrates a stronger implementation, prefer the best evidenced behavior and move that behavior toward the shared runtime while keeping per-game rig/layout/state details behind profiles or adapters. Black Plague finger articulation is the current example: preserve its richer game-neutral articulation semantics rather than degrading it to Overture's simpler rig behavior.

## Before editing

1. Inspect the current repository state and relevant owning files.
2. Read `docs/internal/CODEX_HANDOFF.md` and `docs/internal/DEBUG_HANDOFF.md`.
3. For a Rework-derived behavior, inspect the exact Rework implementation before writing new code.
4. Preserve existing ownership boundaries unless target evidence proves they are insufficient.
5. Treat every exact-build address, structure field, signature and calling convention as evidence-backed data, never as a generic HPL contract.
6. Do not reopen completed reverse-engineering milestones unless new evidence contradicts them.

## Ownership

Runtime owns game-neutral VR policy: tracking transforms, logical input, settings semantics, renderer-neutral view data, accepted-body-motion policy, and proven locomotion/tracking/interaction algorithms.

Backends/adapters own game-specific renderer entry points, HPL object access, body/collision state, player classes, entity queries, native input calls and exact-build binary details.

Do not put game RVAs, binary signatures or player-layout assumptions into shared runtime code.

For exact-build hooks, one callsite has one owner. Other systems must bind through an explicit fan-out/status boundary rather than stacking independent hooks over the same instruction.

## Validation states

Keep these states distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A compile, unit test, static inspection or synthetic binary test does not imply live or headset validation.

For regressions use this sequence before changing behavior:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Do not patch a visible symptom until the difference and likely boundary are identified.

## Current priority

The autonomous Overture source/build/package integration is complete. The exact Framework Release executable with SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` has passed an initial functional SteamVR/headset/controller test without an evident Rework regression. Do not reopen that migration or retune its behavior without a specific regression and the comparison above.

Black Plague body/collision ownership is also no longer an open mapping problem. The exact-build player body, native update sequence, horizontal collision boundary, crouch shape swap and jump ownership are live-characterized. The first narrow `BlackPlagueBodyAdapter` is live-tested on the supported research build. It binds to the existing `NativeInputBridge` movement owner and `BodyCollisionProbe` update owner, never calls `D6E00` itself, dynamically re-resolves the current body and observes accepted displacement through `runtime::VrAcceptedBodyMotion`.

The next gameplay milestone is **tracking/body spatial reconciliation through that live-tested boundary**:

1. preserve the single-owner callsite model;
2. keep Black Plague positional HMD translation at zero while the reconciliation policy is host-tested;
3. connect shared tracking/body policy only through measured native intent and accepted displacement;
4. validate free motion, blocking and sliding before enabling any positional HMD translation;
5. keep jump/vertical state native and keep physical crouch, speed tuning and camera/bob work as separate validation gates.

## Black Plague constraints

Do not add a fake head collider, guessed camera offset or arbitrary movement multiplier.

Do not call `iCharacterBody::Update(D6E00)` from the adapter. The native physics loop owns that update exactly once per tick.

Do not make horizontal VR intent responsible for the native Jump state's vertical pipeline. The live jump burst demonstrates separate ownership.

Do not enable positional HMD translation merely because the adapter is live-tested. The adapter boundary is validated; active tracking/body reconciliation is not.

Do not force doors, levers, joints or other mechanism bodies through the free-body grab path; map their native state instead.

Do not copy Overture RVAs, model-specific grip values or body layouts into Black Plague/Requiem.

The monitor mirror remains experimental and is not a current gameplay milestone.

## Reuse and scope discipline

Prefer the smallest change that advances the current validation gate. Do not create a large generic abstraction until a real second backend needs it.

Do not spend the current milestone on the production installer, mirror polishing, speculative Requiem implementation, broad speed/jump/crouch retuning, or stylistic rewrites of proven Rework logic.

## Documentation

After meaningful changes, update only the relevant documentation, but keep these consistent:

- `README.md`
- `ROADMAP.md`
- `CHANGELOG.md`
- `docs/internal/CODEX_HANDOFF.md`
- `docs/internal/DEBUG_HANDOFF.md`
- `docs/REWORK_PORTING_PLAN.md`
- `docs/OVERTURE_BACKEND_MIGRATION.md`
- `docs/VR_STARTUP_AND_CONTROLLERS.md`
- `docs/VR_HEADSET_TEST_CHECKLIST.md`
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`
- `docs/BLACK_PLAGUE_PROBE.md`
- `docs/SUPPORTED_BUILDS.md`
- `THIRD_PARTY.md`

Do not mark support or headset validation without the corresponding evidence.
