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

The autonomous Overture source/build/package integration is complete. The exact Framework Release executable with SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` has passed an initial functional SteamVR/headset/controller test without an evident Rework regression. The autonomous Overture Release regression is also now a dedicated Windows CI gate and passed after the shared tracking/body extraction. Do not reopen that migration or retune its behavior without a specific regression and the comparison above.

Black Plague body/collision ownership is also no longer an open mapping problem. The exact-build player body, native update sequence, horizontal collision boundary, crouch shape swap and jump ownership are live-characterized. The first narrow `BlackPlagueBodyAdapter` is live-tested on the supported research build. It binds to the existing `NativeInputBridge` movement owner and `BodyCollisionProbe` update owner, never calls `D6E00` itself, dynamically re-resolves the current body and observes accepted displacement through `runtime::VrAcceptedBodyMotion`.

The shared tracking/body reconciliation phases and default-off Black Plague shadow consumer are implemented and **live-tested** through the existing adapter. PID 28172 confirmed transient-mutex activation, stationary/small-head-motion planning, native free/block/slide observations, body replacement/recenter handling and the existing ~60 Hz native body tick while positional HMD translation remained zero. The shadow path still performs no body/camera writes.

The collision-aware physical X/Z displacement request in metres at exact-build
RVA `0xD7281` is now **live-tested** in PID 26144. The session demonstrated
queue → injection → native collision resolution → matched reconciliation with
free, blocked and slide/partial outcomes while positional HMD translation stayed
zero. Re-analysis with the corrected real-HMD stationary classifier also found
12 sub-2 mm stationary/jitter samples. The boundary retained the `0.05 m`
horizontal clamp, body/generation matching and the existing single native tick.

PID 21548 repeated the default-off active room-scale gate after the combined-tick
carry correction. Queue/injection/reconciliation matched 201/201, all four
physical outcomes and the native crouch/stand recovery passed, and in-place head
tilt no longer made the character walk. That session still had continuous world
shake. The renderer was subsequently changed to place X/Z directly at the fresh
reconciled anchor and advance it with only the HMD delta observed since that body
sample, matching Rework's render-rate ownership more closely.

PID 13672 headset-exercised that render-placement correction. The user reported
that the previous continuous "earthquake" sensation was gone and overall comfort
was much improved. The helper observed stationary, free and slide/partial cases,
but its final blocked check produced a false negative after the user had performed
the wall-block case. The helper had classified rejection from total accepted
vector magnitude; Rework `23c890f` uses accepted displacement projected onto the
requested direction. The classifier now follows that proven rule. This is a
validation-tool correction, not a gameplay collision change. The remaining
room-scale concern was then isolated further in PID 11804. The tracking-only
heading correction sent the player in the visually requested direction and the
user reported stable general comfort, but speed still depended on the hidden
native body orientation: an HMD-forward vector mapped to native backward/side
movement inherited that axis's acceleration/cap. The next build therefore uses
Rework's direct `1.5/2.25 m/s` horizontal displacement during the transient
room-scale gate, merges it with the physical request at the existing `0xD7281`
owner, bounds the combined step to `0.05 m`, and partitions accepted physical
and locomotion motion before reconciliation. This is implemented and host-tested
only. PID 17612 then proved that full left-stick analog reached the runtime but
the first direct-locomotion gate never queued it. Clearing the VR amount from
native `MoveForward/MoveSideways` made those methods return on `amount == 0`
before their later `cPlayer+0x264` write, so that byte could not be the intended
permission oracle. The backend now evaluates their exact pre-Move predicate
(two current-state virtual gates plus `+0x268/+0x26C`) without entering native
acceleration, keeps any real native axis higher priority, queues accepted metric
motion through `0xD7281`, and mirrors `+0x264` only after successful publication.
The exact-image verifier pins this boundary and x86 Release compilation passes.
PID 28996 then exposed a second defect in the replicated predicate: the first
implementation dropped the `0x200` part of both indexed-state field pairs and
swapped vector/index ownership. The bridge now reads exactly
`vector +0x2C4 / index +0x2BC` and `vector +0x2D8 / index +0x2D0`.

PID 8092 supplied headset evidence for that correction and direct metric
locomotion. The helper reported `direct_locomotion=True`,
queue/consume/inject/match `201/201/201/201`, all four physical outcome classes,
mirror-on presentation and native crouch-shape recovery; the user reported that
stick locomotion moved correctly and felt substantially better. This closes the
technical stick/collision route for that build. It does not close positional
tracking comfort: PID 20520 later reported that short physical X/Z motion could
still feel as if the world pulled back toward its prior position.

PID 20520 also exercised the first physical-crouch integration. Tracked-height
entry worked, but standing physically left the native body crouched until a
second crouching gesture. The helper's aggregate counters falsely passed that
session. Rework `23c890f` owns one persistent desired crouch state and applies
the native move state explicitly; the failed Framework build instead fed
held/released policy into Black Plague's configurable legacy toggle path.

PID 23260 then showed that the configurable native release dispatch could leave
the body crouched, and PID 24948 showed that compensating with release/second
press could alternate the `1.65/0.95 m` shape without holding Black Plague's
real crouch/stealth state. Exact-build decoding identifies
`cPlayer::ChangeMoveState` at `0x9C750`; the original handlers prove state `4`
is crouch, state `0` is walk and state `3` is jump.

The current correction is implemented and **host-tested** only. Shared
`VrPhysicalCrouchPolicy` owns Rework's button latch, tracked-height baseline,
`PhysicalCrouchDepth`, plausible `(0.90, 2.20) m` range and `0.08 m` hysteresis.
The existing Black Plague game-thread owner applies that desired state directly
through `ChangeMoveState(4/0)`, adopts a legacy crouch edge, retries a blocked
stand, and reports desired move-state/body correlation. It adds no hook.
Rendering now uses shared `VrTrackingSpace` for
continuous HMD Y and `HeightOffset`, with the reconciled body position as the
feet anchor; a physical crouch does not also inherit the native full camera
drop. The next headset gate is the focused run in
`tools/Start-BlackPlagueRoomScaleValidation.ps1`. It must prove two correlated
physical entry/exit cycles with move-state `4/0` and `0.95/1.65 m` shapes, a
stable button-only crouch/stealth interval, Hybrid composition, continuous Y,
final native standing state, short X/Z comfort and no regression of the PID
8092 stick path. Keep tracking-world-yaw turn ownership and final
footstep/body-bob behavior as separate validation gates.

PID 22096 then supplied positive headset evidence for the presentation owner in
`7f84235`: the closed run contains 15,990 frames, 15,887 gameplay frames and no
stereo/compositor failures, so the prior menu-to-gameplay error-108 regression
is no longer the active blocker. The same run also supplied headset evidence for ordinary posture modes and all-direction direct locomotion. The log provided the first useful
collision-comfort correlation for the remaining short-X/Z pullback: 2,060
meaningful physical samples include 215 blocked and 90 partial outcomes, with
render prediction resets occurring around a subset of those rejected solves.
Rework presents the reconciled horizontal anchor rather than unvalidated raw
X/Z. Black Plague still needs render-rate continuation between its ~60 Hz body
ticks, so the current Framework change carries the last physical reconciliation
to render and removes only the prediction component continuing into the last
rejected direction. Tangential motion and retreat are preserved. This filter is
**host-tested only**; do not close the PID 20520 comfort report until a fresh
headset run confirms short motion, wall block and slide without pullback.

## Black Plague constraints

Do not add a fake head collider, guessed camera offset or arbitrary movement multiplier.

Do not call `iCharacterBody::Update(D6E00)` from the adapter. The native physics loop owns that update exactly once per tick.

Do not make horizontal VR intent responsible for the native Jump state's vertical pipeline. The live jump burst demonstrates separate ownership.

Do not bypass the live-tested `0xD7281` body boundary when enabling positional
HMD translation. Active room-scale still requires its own headset validation;
the live boundary evidence does not by itself validate the final camera/body
integration.

Do not force doors, levers, joints or other mechanism bodies through the free-body grab path; map their native state instead.

Do not copy Overture RVAs, model-specific grip values or body layouts into Black Plague/Requiem.

The monitor mirror remains experimental and is not a current gameplay milestone.
PID 19192 confirmed that mirror-off suppresses the gameplay world to black while
native 2D menus remain visible, which matches the current pass ownership. The
room-scale helper persists mirror on for the next batch. Mirror-on behavior and
the reported Alt+Tab/focus-loss menu-black boundary remain unresolved.

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
