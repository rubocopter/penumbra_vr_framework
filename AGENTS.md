# Penumbra VR Framework — Agent instructions

Read this file before changing code. Then read `docs/internal/CODEX_HANDOFF.md` for the current checkpoint, `docs/internal/DEBUG_HANDOFF.md` for known regression boundaries, `docs/REWORK_PORTING_PLAN.md` for the extraction contract, and `docs/TRILOGY_PARITY_PLAN.md` for the cross-game capability gate.

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

The current correction is implemented and now has focused headset evidence from
PID 25484 (2026-09-16). Shared
`VrPhysicalCrouchPolicy` owns Rework's button latch, tracked-height baseline,
`PhysicalCrouchDepth`, plausible `(0.90, 2.20) m` range and `0.08 m` hysteresis.
The existing Black Plague game-thread owner applies that desired state directly
through `ChangeMoveState(4/0)`, adopts a legacy crouch edge, retries a blocked
stand, and reports desired move-state/body correlation. It adds no hook.
Rendering now uses shared `VrTrackingSpace` for
continuous HMD Y and `HeightOffset`, with the reconciled body position as the
feet anchor; a physical crouch does not also inherit the native full camera
drop. PID 25484 recorded calibrated standing, three physical crouch entries and
three exits, native move-state/body correlation, final state `0` with the
`1.65 m` body and VR ownership released, a stable button-only state-`4` crouch,
`0.961 m` of tracked render-Y range, 650 accepted direct-locomotion samples and
5,488 active room-scale render samples. The user reported that the session felt
good. The run also captured the Hybrid combined state (`physical=1`, latch=1,
state `4`), but no later periodic sample with the physical source released while
the latch remained set, so that one Hybrid release-hold subgate remains open.
Blocked-stand/low-ceiling behavior also remains a separate headset edge case.
Do not rerun the whole focused gate merely to re-prove the already captured
crouch/Y/stick evidence. Tracking-world-yaw turn ownership is already aligned
with Rework at code level and the accepted-body VR footstep cadence is now
host-tested with deferred dispatch from the existing game-thread input owner
after native update return; keep its surface-sound telemetry/headset check and
final body-bob presentation as separate validation gates.

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
rejected direction. Tangential motion and retreat are preserved. PID 25484
headset-exercised this filter with 2,477 meaningful body samples (`2407 free /
43 blocked / 27 partial`) and the user reported that the session felt good.
Treat this as positive headset comfort evidence for the filter while keeping
deliberate wall/slide edge cases and the more specific PID 20520 pullback
symptom distinct unless a run explicitly checks them.

Black Plague palm collision has also advanced beyond the research-only stage.
PID 28412 live-tested the default-off no-write native query and PID 8644
live-tested the backend-owned palm-shape lifecycle plus the isolated
Rework-derived resolver. The supported-image verifier now pins Black Plague's
independent character and exact `skip_body` exclusions. The gameplay path is
implemented and host-tested: each hand publishes its held body, resolved palms
drive visible hands/owned-body motion/tools while aim stays raw, `Grab=6`
retains the rigid palm-relative free-body path, and eligible free `Move=2`
bodies preserve their picked contact and follow the palm through the
Rework-derived force path. Exact Rework `23c890f` comparison after the reported
pickup difficulty restored its separate acquisition rule: selection may follow
the raw controller by at most `0.18 m` from a collision-stopped palm, while the
actual hold remains resolved. Recognized one-joint `cGameLever` hinge/slider
and one-joint `cGameSwingDoor` hinge bodies consume the shared mechanism servo
while native `Move::Enter/Leave` retain lifecycle ownership; Wheel, unknown and
multi-joint mechanisms remain native. PID 23000
reached this gameplay path in the headset, but severe FPS loss and a
right-controller dropout made the run inconclusive. PID 4720 also cannot judge
room-scale regressions because the old focused palm helper started with physical
displacement, room-scale and positional translation disabled. The helper now
composes palms with the known-good room-scale/crouch stack and verifies that
composition in telemetry. PID 26940 reached that combined path and exposed two
separate regressions: inherited HPL client-array/VBO state corrupted the imported
hand draw into black/rainbow triangles, and physical-only wall pressure entered
Black Plague's native step-climb phase and produced repeated vertical mini-jumps.
The shared hand renderer now isolates/restores the relevant GL client and unpack
state and has a real-driver hostile-state regression; the exact BP backend now
owns a pinned `0xD7361` step boundary and skips only native step climbing on a
physical-HMD-only tick with no direct/native horizontal motion. Gravity/jump and
the sole `D6E00` update remain native, while stick/native movement retains normal
step behavior. Both fixes are host-tested only. The next palm gate is therefore
a clean-reboot run of the combined helper with both controllers healthy, stable
hand texture plus five-finger articulation, gentle wall-pressure/slide without
vertical bounce, ordinary stick stair/ledge traversal, representative contact
plus `Grab=6`/`Move=2`, short physical X/Z + crouch checks before/after contact,
and at least one native jointed mechanism. Do not promote gameplay palms or the
new collision adaptation beyond host-tested until that clean evidence exists.

Offline presentation work has also advanced while the headset gate remains
blocked. The supported image now pins gameplay 2D ownership through
`OnPostSceneDraw (0xE1C40) -> GetDrawer (0xDF730) -> DrawAll (0xF3920)`, with
the sole `DrawAll` callsite at `0xEE042`. Because the original Black Plague
`DrawAll` has no Rework VR arguments and always resets ortho/clears its queue,
the host-tested adaptation defers gameplay compositor submit to that callsite,
captures the native 800x600 draw once into transparent RGBA and alpha-composites
it into both eye targets before submission. Do not add a second HUD/subtitle
producer or call native `DrawAll` independently per eye. The new overlay path is
host-tested only; require nonzero overlay/deferred-submit telemetry, zero overlay
failures and visible native HUD/subtitles in headset before promotion or before
wiring `SubtitleScale` at the mapped product-owned message boundary.

Offline haptic parity has also advanced without adding a second native input
owner. `LightToggle` now observes the exact flashlight/glowstick active state
around the existing `cButtonHandler::Update` owner and emits the shared Rework
event to the off hand only after native state changes. `Damage` is owned through
one exact-build wrapper over all ten direct `cPlayer::Damage (0x9BB80)`
callsites; the original method remains authoritative and both-hand feedback is
emitted only when `cPlayer+0x310` health actually decreases. Both paths are
exact-image verified and host-tested only. `MeleeImpact` now follows Rework's
confirmed-contact semantics through three exact-build owners inside Black
Plague's native melee attack: enemy `cGameEntity::Damage` at `0x603D5` and the
two post-collision `HitBody (0x5F000)` calls at `0x60747/0x608CF`. The native
resolver remains authoritative and its enemy-body exclusion is mirrored so the
enemy path cannot double-pulse. This boundary is exact-image verified and
host-tested only; require headset evidence that real hits pulse the dominant
controller and empty swings do not before promotion.

Black Plague HRTF startup is now consumed at the launcher boundary and is
**host-tested only**. Rework `23c890f` writes `alsoft.ini` next to the executable
before OpenAL opens its device; Framework writes the same shared
`auto/true/false` configuration before a fresh Steam launch. If the game is
already running, a mismatched saved mode is rejected rather than pretending a
post-device-open change took effect. Exact-image research confirms Black Plague
already contains its own OpenAL/EFX environment path, but that native setup uses
an environment bus gain of `1.0` and does not demonstrate Rework's added
mine-gallery preset/`0.32` trim or distance-HF low-pass behavior. Keep
occlusion/reverb open until a narrow target-owned runtime boundary is proved;
do not stack speculative OpenAL hooks over the native audio owners.

Black Plague also now consumes the transferable Rework Enhanced Visuals eye
stage and this is **host-tested only**. Exact Rework `23c890f` source separates
the per-eye RGBA16F + 2x MSAA/resolve + v4 final treatment from its HPL-specific
ambient/light material programs. `OpenGlEnhancedEyeStage` implements only that
transferable eye stage and the BP persistent-eye owner selects it through the
existing `EnhancedVisuals` setting, with the old direct eye target as a graceful
fallback. The real-driver WGL regression and 36/36 Release CTest pass. Keep the
remaining HPL material/light response and headset image/performance validation
open; do not call the whole Enhanced Visuals family complete from this stage.

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

Extraction is not parity by itself. A Rework capability counts as ported to a target only when that backend consumes the shared behavior (or a documented incompatible native equivalent) and the corresponding target evidence is recorded. Keep `docs/TRILOGY_PARITY_PLAN.md` current when a capability moves between reference/shared/consumed/validated states.

Do not spend the current milestone on the production installer, speculative Requiem gameplay implementation, broad speed/jump/crouch retuning, or stylistic rewrites of proven Rework logic. Requiem gameplay becomes the active milestone after the Black Plague framework-readiness gate in `docs/TRILOGY_PARITY_PLAN.md` is satisfied; exact-build reconnaissance may still proceed when it does not displace the active Black Plague parity work.

## Documentation

After meaningful changes, update only the relevant documentation, but keep these consistent:

- `README.md`
- `ROADMAP.md`
- `CHANGELOG.md`
- `docs/internal/CODEX_HANDOFF.md`
- `docs/internal/DEBUG_HANDOFF.md`
- `docs/REWORK_PORTING_PLAN.md`
- `docs/TRILOGY_PARITY_PLAN.md`
- `docs/OVERTURE_BACKEND_MIGRATION.md`
- `docs/VR_STARTUP_AND_CONTROLLERS.md`
- `docs/VR_HEADSET_TEST_CHECKLIST.md`
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`
- `docs/BLACK_PLAGUE_PROBE.md`
- `docs/SUPPORTED_BUILDS.md`
- `THIRD_PARTY.md`

Do not mark support or headset validation without the corresponding evidence.
