# Penumbra VR Framework — Agent instructions

Read this file before changing code. Use `ARCHITECTURE.md` and `docs/DESIGN_DECISIONS.md` for ownership/invariants, `docs/TRILOGY_PARITY_PLAN.md` for cross-game capability state, `docs/SUPPORTED_BUILDS.md` for evidence/build status, `docs/REWORK_PORTING_PLAN.md` for the extraction contract, and `ROADMAP.md` for current work order.

## Core rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR.

Do not invent a replacement for behavior that Rework already demonstrates. First locate the exact Rework implementation and tests, separate game-neutral behavior from HPL/game-specific dependencies, port the proven behavior, and adapt only the narrow game boundary that differs.

A new algorithm is acceptable only when Rework has no equivalent, the target exposes a technically incompatible interface, or evidence demonstrates that direct adaptation is unsafe or incorrect. Record that reason in the relevant durable migration/research document.

Rework is not automatically the best implementation of every game-neutral subsystem. When another backend demonstrates stronger reusable behavior, prefer the best evidenced behavior and move it toward shared runtime only after a real second consumer proves the abstraction. Black Plague's richer finger articulation is the current example.

Treat Rework as the proven baseline, not as the ceiling of the project. Penumbra VR Framework is the forward integration line: when Black Plague, Requiem or Framework work produces a demonstrably better game-neutral behavior, validate it, promote it to shared runtime when the abstraction is proven, and make the Framework-owned Overture product consume that shared improvement too. Overture must continue to benefit from reusable advances made after Rework rather than remaining frozen at `23c890f` behavior. Keep game-specific mechanics, binary details and profile data in their owning backend/profile instead of forcing them into this feedback loop.

## Before editing

1. Inspect `git status`, branch, HEAD/upstream relation and the relevant owning files.
2. Read the durable documents listed above for the area being changed.
3. For Rework-derived behavior, inspect the exact Rework implementation before writing new code.
4. Verify current code/evidence before treating a documented item as pending or complete.
5. Preserve existing ownership boundaries unless target evidence proves they are insufficient.
6. Treat every exact-build address, structure field, signature and calling convention as evidence-backed data, never as a generic HPL contract.
7. Do not reopen completed reverse-engineering milestones unless new evidence contradicts them.
8. Preserve unrelated local work; never clean/reset/revert it as part of another task.

## Ownership

Runtime owns game-neutral VR policy: tracking transforms, logical input, settings semantics, renderer-neutral view data, accepted-body-motion policy, and proven locomotion/tracking/interaction algorithms.

Backends/adapters own game-specific renderer entry points, HPL object access, body/collision state, player classes, entity queries, native input calls and exact-build binary details.

Profiles/data own demonstrated controller bindings, rig/bone mappings, tool/model sockets and other per-game/per-model values that are not universal policy.

Do not put game RVAs, binary signatures, player layouts or guessed HPL contracts into shared runtime code.

For exact-build hooks, one callsite has one owner. Other systems bind through explicit fan-out/status boundaries rather than stacking independent hooks over the same instruction.

## Validation and regressions

Keep these states distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A compile, unit test, static inspection or synthetic binary test does not imply live or headset validation. Evidence does not transfer automatically between games or unrelated capability families.

For regressions use this sequence before changing behavior:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Do not patch a visible symptom until the difference and likely ownership boundary are identified.

## Black Plague constraints

- Do not add a fake head collider, guessed camera offset or arbitrary movement multiplier.
- Do not call `iCharacterBody::Update(D6E00)` from the adapter. The native physics loop owns that update exactly once per tick.
- Do not make horizontal VR intent responsible for the native Jump state's vertical pipeline.
- Do not bypass the live-tested `0xD7281` body boundary for positional HMD translation/direct metric locomotion.
- Do not force doors, levers, joints or other mechanisms through the free-body grab path; preserve mapped native lifecycle/constraint ownership.
- Do not copy Overture RVAs, model-specific grip values or body layouts into Black Plague or Requiem.
- Keep mirror/focus behavior as its own presentation gate while it remains experimental; do not let mirror work redefine headset pacing.

The detailed Black Plague capability/evidence state changes frequently. Read `docs/TRILOGY_PARITY_PLAN.md`, `docs/SUPPORTED_BUILDS.md` and `ROADMAP.md` instead of carrying PID/session chronology in this file.

## Reuse and scope discipline

Prefer the smallest change that advances the current validation gate. Do not create a broad generic abstraction until a real second backend needs it.

Extraction is not parity by itself. A Rework capability counts as ported to a target only when that backend consumes the shared behavior (or a documented incompatible native equivalent) and target evidence is recorded in `docs/TRILOGY_PARITY_PLAN.md`.

Do not spend a Black Plague parity milestone on the production installer, speculative Requiem gameplay implementation, broad speed/jump/crouch retuning or stylistic rewrites of proven Rework behavior. Requiem gameplay becomes active after the framework-readiness gate in `docs/TRILOGY_PARITY_PLAN.md` is satisfied; non-invasive exact-build reconnaissance may proceed when it does not displace that work.

## Documentation policy

Versioned documentation describes durable architecture, current capability/evidence state, real validation procedures, compatibility and maintainership contracts.

Temporary investigation, debugging chronology, discarded hypotheses, prompts and agent continuity belong under ignored `work/`. If `work/LOCAL_DOCS.md` exists, it describes the local documentation boundary; local notes are never authoritative over code and versioned state documents.

After meaningful changes, update only the versioned documents that own the affected fact. Avoid repeating the same status in multiple files. Do not mark support or headset validation without corresponding evidence.
