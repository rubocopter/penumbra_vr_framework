# Penumbra VR Framework — Agent instructions

Read this file before changing code. Then read `CODEX_HANDOFF.md` for the current checkpoint and `docs/REWORK_PORTING_PLAN.md` for the Rework extraction contract.

## Core rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR.

Do not invent a replacement for behavior that Rework already demonstrates. First locate the exact Rework implementation and tests, separate game-neutral behavior from HPL/game-specific dependencies, port the proven behavior, and adapt only the narrow game boundary that differs.

A new algorithm is acceptable only when:
- Rework has no equivalent;
- the target exposes a technically incompatible interface; or
- evidence demonstrates that direct adaptation is unsafe or incorrect.

Record that reason in the relevant migration/research document.

## Before editing

1. Inspect the current repository state and relevant owning files.
2. Read `CODEX_HANDOFF.md`.
3. For a Rework-derived behavior, inspect the exact Rework implementation before writing new code.
4. Preserve the existing ownership boundary unless the real target integration proves it is insufficient.
5. Treat every exact-build address, structure field, signature and calling convention as evidence-backed data, never as a generic HPL contract.

## Ownership

Runtime owns game-neutral VR policy: tracking transforms, logical input, settings semantics, renderer-neutral view data, and proven locomotion/tracking/interaction algorithms.

Backends/adapters own game-specific renderer entry points, HPL object access, body/collision state, player classes, entity queries, native input calls and exact-build binary details.

Do not put game RVAs, binary signatures or player-layout assumptions into shared runtime code.

## Validation states

Keep these states distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A compile, unit test, static inspection or synthetic binary test does not imply headset validation.

For regressions use this sequence before changing behavior:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Do not patch a visible symptom until the difference and likely boundary are identified.

## Current priority

Finish the Overture adapter first:

1. link `OvertureBodyAdapter` into the real Overture executable;
2. build/deploy a test Overture package;
3. validate tracking space, calibrated height, seated/standing, recenter/yaw, locomotion, sprint, jump and room-scale rejection against Rework;
4. record any measured difference before changing constants.

Only after that return to Black Plague body/capsule research.

## Black Plague constraints

Do not enable positional HMD translation until the exact body/capsule and collision-resolution path is mapped and instrumented.

Do not use a guessed camera offset, fake head collider or arbitrary movement multiplier as a substitute for the native body contract.

Do not force doors, levers, joints or other mechanism bodies through the free-body grab path; map their native state instead.

Do not copy Overture RVAs or model-specific grip values into Black Plague/Requiem.

The monitor mirror is experimental and not a current gameplay milestone.

## Reuse and scope discipline

Prefer the smallest change that advances the current validation gate. Do not create a large generic abstraction until a real second backend needs it.

Do not spend the current milestone on the production installer, mirror polishing, speculative Requiem implementation, or stylistic rewrites of proven Rework logic.

## Documentation

After meaningful changes, update only the relevant documentation, but keep these consistent:

- `README.md`
- `ROADMAP.md`
- `CODEX_HANDOFF.md`
- `docs/REWORK_PORTING_PLAN.md`
- `docs/OVERTURE_BACKEND_MIGRATION.md`
- `docs/VR_STARTUP_AND_CONTROLLERS.md`
- `docs/VR_HEADSET_TEST_CHECKLIST.md`
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`
- `docs/BLACK_PLAGUE_PROBE.md`
- `docs/SUPPORTED_BUILDS.md`
- `THIRD_PARTY.md`

Do not mark support or headset validation without the corresponding evidence.
