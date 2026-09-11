# Codex handoff — Penumbra VR Framework

This is the operational checkpoint for future coding sessions. Read `AGENTS.md` first. Read `DEBUG_HANDOFF.md` before changing any known regression boundary, and use `docs/REWORK_PORTING_PLAN.md` for the extraction contract.

## Source-of-truth rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR. Do not invent a replacement for demonstrated Rework behavior before locating the original implementation and tests, separating game-neutral policy from game/HPL mechanism, and documenting why direct adaptation would be unsafe or impossible.

The framework is not a one-way Overture port. If another backend demonstrates a stronger game-neutral implementation, preserve the better behavior and move it toward shared runtime policy while keeping per-game layouts, rigs and native state behind adapters/profiles. Black Plague finger articulation is currently the clearest example.

## Repository checkpoint

- Repository: `rubocopter/penumbra_vr_framework`
- Default branch: `main`
- Current integrated checkpoint on GitHub: `b98708c2bc89d06a53407a677919426f4aecedc7` (`feat: add Black Plague body adapter boundary`)
- Framework state: pre-alpha
- Windows/x86 remains the current binary-research target
- Rework `23c890f` is reference-only; Overture build/package no longer depends on that checkout

Always inspect `git status`, HEAD and origin synchronization before editing. Do not assume a local tree is clean just because this checkpoint is clean on GitHub.

## Overture

The autonomous Framework-owned Overture source/build/package integration is complete.

Proven/shared behavior includes:

- tracking space and calibrated height;
- seated/standing composition;
- world yaw/recenter;
- room-scale displacement rejection/reconciliation;
- fixed physical body steps of `0.05 m`;
- locomotion policy of `1.5 m/s` walk and `2.25 m/s` sprint;
- shared direct interaction reach of `0.18 m`.

`OvertureBodyAdapter` remains the source-level HPL boundary for body position, feet height, collision movement and jump. The concrete adapter is under `src/adapters/overture_source`; the Framework-owned host, dependencies and package tooling are under `products/overture`.

The tested autonomous Release executable is 3,302,912 bytes with SHA-256:

`D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`

On 2026-09-10 the user deployed the Framework package over a valid retail installation and completed an initial SteamVR/headset/controller pass. Perceived behavior matched the prior Rework build with no evident regression. This is initial headset validation, not exhaustive equivalence and not a supported-release claim.

Do not reopen Overture source-host migration unless a concrete regression requires the `REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION` comparison.

## Black Plague — established boundaries

The supported research executable hash remains:

`FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`

Previously headset-validated:

- native stereo;
- yaw-aligned rotational HMD tracking;
- keyboard/mouse preservation;
- conservative HMD-aware render-list visibility.

Implemented/code-tested but not automatically headset-validated:

- OpenVR actions and exact-build native input bridge;
- tracked menus;
- provisional procedural gloves;
- controller picking;
- palm-relative free-body grab/release/throw;
- `0.18 m` direct physical reach fallback;
- tool/light attachment groundwork;
- eye-scissor/light remapping.

### Player/body and collision

This reverse-engineering milestone is closed unless contradictory evidence appears.

Confirmed exact-build structure and sequence:

- `cPlayer+0x274` → native `iCharacterBody*`;
- current/previous position at `+0x48/+0x54`;
- active size at `+0xC4`;
- active `iPhysicsBody*` at `+0x23C`;
- `iPhysicsWorld*` at `+0x240`;
- `iCharacterBody::Move` at `D4F50` updates native horizontal acceleration/speed state;
- normal physics boundary is `D460A -> iCharacterBody::Update(D6E00)`;
- first horizontal collision resolution is `D7312 -> CheckShapeWorldCollision(D4830)`;
- live standing shape is a `0.70 x 1.65 x 0.70 m` cylinder, radius `0.35 m`;
- measured physics tick is `dt=1/60`;
- free movement, total block, slide and partial acceptance are live-observed.

Do not create a Framework head collider. The real player body already owns collision.

### Sprint, crouch and jump ownership

Native walk/sprint limits are effectively about `3.0 / 4.5 m/s`. Do not treat analog-input scaling as equivalent to Rework's direct `1.5 / 2.25 m/s` displacement policy.

Native crouch is a real shape/body swap, not just a camera effect:

- standing: `0.70 x 1.65 x 0.70`;
- crouched: `0.70 x 0.95 x 0.70`;
- feet Y remains fixed;
- the active physics-body pointer changes and restores.

The exact Black Plague stand-clearance mechanism is not sufficiently demonstrated. Do not import Overture `CanStand` assumptions.

Jump remains native and separate from horizontal intent:

- state index `3` correlates to `cPlayerMoveState_Jump`;
- `EnterState` prepares vertical force before the next `D6E00`;
- first observed accepted vertical speed is about `+5.53 m/s`;
- apex is about `0.95 m` above baseline;
- landing is resolved in the native body tick, followed by state `3 -> 0`;
- horizontal `requested.y` and `solver.y` stay zero during the jump while final accepted Y changes;
- `+268` is a 25-tick ground-grace counter;
- `+26C` remains only an unclassified alternate jump-eligibility branch;
- `+204=0.3` is a hold threshold, not a hard clamp for `+200`.

Do not move vertical/jump ownership into the shared horizontal contract.

## BlackPlagueBodyAdapter — live-tested initial boundary

The first narrow adapter is implemented and live-tested on PID 8628.

Ownership is intentionally single-owner:

- `NativeInputBridge` owns the `MoveForward` and `MoveSideways` callsites;
- `BodyCollisionProbe` owns `D460A -> D6E00`;
- `BlackPlagueBodyAdapter` binds to those verified owners and receives fan-out; it does not stack another hook on either boundary.

The adapter:

- re-resolves the current `cPlayer+0x274` body on each publish/observe callback;
- publishes the already-computed native horizontal amount through the mapped native movement method;
- never calls `D6E00`;
- observes body/feet after the one native update returns;
- converts before/after positions into `runtime::VrAcceptedBodyMotion`;
- fails closed if the exact initialized owner state no longer matches.

PID 8628 demonstrated:

- all relevant owners and adapter installed together;
- free displacement;
- total blocking;
- sliding/partial acceptance;
- body replacement without stale-pointer caching;
- approximately 60 native body updates/s at `dt=0.016667`;
- no evidence of a second `D6E00` call.

`positional_translation_enabled=0` throughout that validation. Therefore the adapter boundary is live-tested, but active room-scale/tracking-body reconciliation, positional HMD translation, VR speed tuning, physical crouch, jump tuning and camera/bob comfort are not.

## First shared body policy

`runtime::VrAcceptedBodyMotion` is the first body contract used by both Overture and Black Plague. It contains finite before/after positions and accepted displacement only. It must remain free of RVAs, HPL layouts, speed constants and native-update ownership.

Overture consumes it without changing its proven room-scale/locomotion behavior. Black Plague currently uses it as observation only.

## Current milestone — tracking/body reconciliation

Do not add more body/collision probes by default. The next milestone is to connect a minimal shared tracking/body reconciliation policy through the live-tested BP boundary.

Required sequencing:

```text
tracked physical head/body intent
        ↓
shared game-neutral reconciliation policy
        ↓
BlackPlagueBodyAdapter publishes horizontal native intent
        ↓
native D6E00 executes once in its normal physics tick
        ↓
native collision + native vertical/move-state processing
        ↓
adapter observes accepted displacement
        ↓
shared policy reconciles the tracking/body anchor
```

Initial implementation must remain conservative:

1. keep Black Plague positional HMD translation at zero while wiring and host-testing the policy;
2. preserve keyboard/mouse and controller input;
3. preserve native `3.0/4.5 m/s` tuning during the first boundary integration unless the milestone explicitly scopes speed policy;
4. keep jump/vertical native;
5. do not implement physical crouch in the same change;
6. do not solve camera/head-bob/footstep-bob by zeroing constants or inventing offsets;
7. do not add a second body-update or movement-callsite hook;
8. validate free motion, block and sliding before enabling positional HMD translation.

Only after that host boundary is stable should a deliberate headset gate enable and validate physical HMD/body translation.

## Camera/bob

`D790C -> D5F00` and `D7913 -> D6120` are gravity-disabled synchronization calls, not the general active-player camera composition path. Do not reuse them as a camera boundary.

Camera/head-bob/footstep-bob ownership remains separate comfort work. It is not a reason to reopen body ownership, and it should not be changed speculatively while implementing the first tracking/body reconciliation boundary.

## Hands/fingers

Black Plague currently provides the better game-neutral articulation semantics: five independent curls, per-joint curves, spread and thumb opposition. Overture contributes rig-specific bind poses, bone axes, measured deadzone/smoothing and handle poses.

Future extraction direction:

```text
shared articulation output
        ↓
per-game rig/profile adapter
```

Do not degrade Black Plague articulation to match Overture merely because Overture is the historical reference.

## Interaction priorities after body reconciliation

1. palm collision and character/body exclusion validation;
2. jointed mechanisms / doors / levers through native mechanism state;
3. definitive per-game tool/glowstick grip profiles;
4. inventory, notes, menus, HUD and subtitles;
5. comfort/haptics and representative chapter-level validation.

Long bars and mechanisms must not be fixed by arbitrary springs or rigid palm offsets. Map native joint/slider/hinge state.

## Requiem

Requiem remains a future exact-build binary backend. Do not assume Black Plague RVAs, layouts, calling conventions or lifecycle boundaries transfer without evidence.

## Validation discipline

Keep states distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

Compilation and CTest do not imply live or headset validation.

Current root validation count at this checkpoint is 26 CTest tests in the validated Release configuration. Overture retains its 289 historical `VRTrackingTest` checks plus its shader/visual/texture/package gates.

For meaningful changes update the smallest relevant set among:

- `README.md`
- `ROADMAP.md`
- `CHANGELOG.md`
- `CODEX_HANDOFF.md`
- `DEBUG_HANDOFF.md`
- `docs/REWORK_PORTING_PLAN.md`
- `docs/OVERTURE_BACKEND_MIGRATION.md`
- `docs/VR_STARTUP_AND_CONTROLLERS.md`
- `docs/VR_HEADSET_TEST_CHECKLIST.md`
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`
- `docs/BLACK_PLAGUE_PROBE.md`
- `docs/SUPPORTED_BUILDS.md`
- `THIRD_PARTY.md`

Do not mark the Black Plague backend or any executable as `supported` without the required evidence gate.
