# Overture backend migration and Black Plague comparison

Reference revision: `rubocopter/penumbra_vr_rework`, Git commit `23c890f` (`v0.1.0`).

This document records the evidence used for the first Overture backend in the
framework. `pvr_overture_backend` is a functional, host-tested gameplay core:
it consumes shared input/settings/tracking, drives room-scale and stick motion,
and delegates HPL collision and jump calls through `OvertureBodyAdapter`.
`src/adapters/overture_source` implements that interface with real Overture/HPL
types and is linked into a Win32 `Penumbra_vr.exe` from the Framework-owned
source host under `products/overture`. A Release test package exists; it has not
only been built independently, but has since been deployed and exercised with
SteamVR, a real headset and controllers. The first functional pass showed no
evident regression against the previously tested Rework behavior. That evidence
does not constitute an exhaustive feature or hardware validation.

## Evidence inspected before the port

- Framework source, tests and documentation at `122c338`; the working tree was
  clean before this change.
- Black Plague probe logs from 2026-09-07 under
  `%LOCALAPPDATA%\PenumbraVR\logs`, especially sessions 11072 and 12876.
- The current Black Plague `hpl.log` and installed `config/game.cfg`.
- Rework tracking, player, input, hand, interaction and physics sources at
  `23c890f`, plus the installed Overture VR HUD configuration.

The recent Black Plague logs show stable OpenVR/stereo initialization and real
grab/release activity. They do not contain body, capsule, feet or resolved-head
coordinates. Their `native_update_timing` samples report 240 handler callbacks
for 4 simulated seconds in about 2 wall seconds. Source inspection explains
this: Rework registers `cButtonHandler` in both the global and `Default` updater
containers, so this metric counts two handler calls rather than unique player
physics ticks. It is not evidence that the whole simulation runs at 2x.

## Current ownership after the migration and Black Plague mapping

| System | Runtime | Overture backend / adapter | Black Plague backend |
|---|---|---|---|
| Tracking space | `VrTrackingSpace`: metres, yaw, calibration, posture/seated offsets and tracking-to-world | supplies source HPL body/feet pose | exact-build body, shadow reconciliation and physical-request boundary are live-tested; active positional translation awaits its headset gate |
| Locomotion | Rework constants and pure direction, displacement and rejection policy | `OvertureBackend` sequences HPL body moves | native movement/speed ownership is live-characterized; the separate bounded X/Z request is live-tested |
| Collision | requested/accepted displacement reconciliation | `OvertureBodyAdapter::MoveBodyBy` owns `iCharacterBody::Update` and static-only mode | cylinder/body/update/solver ownership and injected request reconciliation are live-tested |
| Jump | edge/held semantics | adapter calls native `Jump` and `SetJumpButtonDown` | native Jump-state vertical ownership and the unique body tick are live-characterized |
| Turn | shared neutral-arm and dead-zone policy | changes tracking-space world yaw | currently changes native player yaw |
| Interaction | shared palm-relative grab/release math already present | HPL entity classification, palm overlap and joints stay backend-side | exact-build direct free-body path; no jointed-body adapter yet |
| Tools | shared behavior is possible after geometry-specific grip data is separated | Rework HUD grip profiles and hand bones | installed Black Plague DAE sockets need headset tuning |

## Head collision / spatial displacement

### REWORK

File: `PenumbraOverture/Player.cpp`, `cPlayer::Update`.

Behavior: horizontal HMD delta advances a desired head anchor. The character
body follows in steps capped at 0.05 m through the native character collision
solver with static-only step handling. The accepted displacement is projected
onto the request; any rejected distance above 0.002 m is removed from the head
anchor. Head/body divergence above 0.8 m rebases the anchor. World scale stays
1.0 in `HPL1Engine/include/game/VRTracking.h`.

### FRAMEWORK BEFORE THIS PHASE

File: `src/backends/black_plague/render_world_probe.cpp`, world-view path.

Behavior: `ComposeYawRecenteredTrackedHeadView` was called with translation
scale 0.0. It provided rotation only. There was no head collider, no
room-scale character-body request and no rejected-motion reconciliation.

### DIFFERENCE

The framework had not implemented the system that could safely couple physical
head movement to the player body. Therefore the reported Black Plague
push-back cannot honestly be attributed to an over-large framework head
collider: no such collider exists. Current logs also cannot identify which
native body/camera change caused that observation.

### CAUSE

The architectural cause is a missing exact-build body adapter and missing
position telemetry. Enabling camera translation alone would let the view cross
solid geometry and reproduce the tracking-origin divergence that Rework
explicitly corrects.

### SOLUTION

`VrTrackingSpace`, the 0.05/0.8/0.002 policy and accepted-motion reconciliation
are now in the common runtime and exercised through the Overture backend.
Black Plague now logs the raw HMD anchor/current position, horizontal physical
delta and the explicit positional scale (still 0.0). Positional translation
remains disabled until its character body, feet/capsule state and collision
update can be mapped and logged. The next BP step is an exact-build adapter for
those operations, followed by headset tests at standing and crouched heights.

## Movement, running and jump

### REWORK

Files: `PenumbraOverture/ButtonHandler.cpp` and `Player.cpp`.

Behavior: the stick produces one head-relative world vector, consumed once by
the player update. Normal direct displacement is 1.5 m/s times the MoveSpeed
setting, sprint is 1.5x (2.25 m/s at defaults), and Move/Push states use 0.5
m/s. Jump uses one `justPressed` native start plus a held flag; authored HPL
values such as `JumpStartForce=350` and `MaxJumpCount=0.3` remain game data.

### FRAMEWORK BEFORE THIS PHASE

File: `src/backends/black_plague/native_input_bridge.cpp`.

Behavior: VR stick values are merged into native Move calls. Black Plague's
authored forward caps are 3.0 m/s walking and 4.5 m/s running. Scaling an input
amount changes acceleration, not those terminal speeds. The twice-registered
button handler may also submit held analog input twice per player tick.

### DIFFERENCE / CAUSE

The paths are not equivalent. Rework writes one fixed displacement at half the
native walking speed; the binary backend feeds the legacy acceleration/cap
system. A settings multiplier alone cannot guarantee Rework speed or jump
distance.

### SOLUTION

The exact Rework displacement constants and head-relative direction are now
common runtime policy and are used by `OvertureBackend`. Black Plague must not
claim equivalent locomotion until an exact-build body displacement/speed
adapter is validated. Its current input path remains available, but the issue
is explicitly open rather than hidden by a guessed multiplier.

## Interaction and pickup distance

### REWORK

File: `PenumbraOverture/PlayerState_Misc_VR.cpp`.

Behavior: ordinary props, doors and mechanisms require palm-shape overlap plus
line of sight. Collision-constrained versus raw palm reach is capped at 0.18 m.
Only entities classified as inventory items get magnetic aim assistance, with
per-item ranges up to 2.35 m and hand/head occlusion checks.

### FRAMEWORK BEFORE THIS PHASE

File: `src/backends/black_plague/spatial_interaction.cpp`, `HookedRay`.

Behavior: the native camera pick ray was redirected to the controller but kept
its full native length (normally 1.9 m) for every interactable class.

### DIFFERENCE / CAUSE

Large props could become valid while the hand was still far away because a
camera-distance ray had been turned into an unrestricted hand ray.

### SOLUTION

The Black Plague fallback ray is now capped to the shared 0.18 m Rework reach
and emits `contact_rays`/`contact_reach_m` telemetry. This deliberately favors
physical acquisition until the binary adapter can safely classify inventory
items and implement Rework's separate magnetic path. Palm collision, jointed
mechanisms and headset validation remain pending.

## Held objects and throw

Both implementations use a rigid palm-relative transform for free bodies and
disable player collision during ownership. The framework keeps Rework's pose
behavior but improves release sampling with a five-sample median and tracking
discontinuity guard. Rework also has dedicated joint/slider/hinge states; the
Black Plague backend currently rejects jointed and parented bodies. Long free
bodies following the palm rigidly is therefore expected, while mechanisms
still need their own adapter rather than being forced through free grab.

## Glowstick

Rework does not attach the glowstick at its light node. Its installed HUD data
defines `VrGripPoint="0.0 0.0078 -0.078"`, `VrRotOffset="4.71 0 0"` and
`VrScale=1.55`, then `PlayerHands.cpp` composes that profile with a measured
finger-cylinder grip socket. Black Plague currently anchors its original,
different DAE by a measured embedded node. The resources are not byte-identical,
so copying Rework's numbers blindly would mix two geometries. A shared grip
profile type and a BP-specific measured grip point are the next safe change;
the mirror remains outside this milestone.

## Real Overture source integration (2026-09-10)

### REWORK

`cButtonHandler::Update` reads `cVRInputState`, handles yaw/recenter/play mode
and produces `vr_moveVec`. `cPlayer::Update` owns the room-scale/body sequence,
calls `iCharacterBody::Update(afTimeStep)`, reconciles rejected collision motion,
consumes stick movement once and updates the tracking player pose. The updater
registers `cButtonHandler` globally and in `Default`, while `cPlayer` runs only
in `Default`.

### FRAMEWORK

`OvertureBackend` already owned the equivalent tracking, turn, seated/standing,
room-scale and locomotion policy behind `OvertureBodyAdapter`, but the adapter
had no implementation using real `cPlayer`/`iCharacterBody` objects and its move
method did not receive the HPL update timestep.

### DIFFERENCE

The host-tested backend could not call the real collision solver or jump path,
and the Rework executable still contained a second inline copy of the same VR
policy. A direct input call also has to account for HPL's double handler
registration in `Default` without suppressing the single global call in menus.

### CAUSE

`iCharacterBody::Update` is an Overture/HPL call that requires the current
simulation timestep. The updater registration and real body lifetime are also
game-specific facts; they do not belong in the shared runtime.

### SOLUTION

- `OvertureBodyAdapter::MoveBodyBy` now receives `delta_seconds`; the backend
  forwards the already validated player-frame timestep without changing its
  displacement policy.
- `HplOvertureBodyAdapter` maps body position, feet height, `vr_velocity`,
  `vr_stepstaticonly`, `iCharacterBody::Update`, `Jump` and
  `SetJumpButtonDown` directly to the proven Rework types and calls.
- `OvertureSourceIntegration` converts only the existing HPL input, settings and
  matrix types, synchronizes the runtime tracking state back to `cVRTracking`,
  resets at the real player/world lifecycle boundaries, and suppresses only the
  duplicate `Default` input invocation that precedes the same player update.
- Rework's inline movement/turn/play-mode copy is removed from the consumer;
  HPL-specific crouch, player-state gating, footsteps, entity interaction and
  rendering remain owned by Overture.
- The constants and sequencing remain unchanged: 0.05 m physical step, 0.002 m
  rejection epsilon, 0.8 m head/body rebase, 1.5 m/s normal, 2.25 m/s sprint and
  0.5 m/s constrained movement.

## Black Plague boundary comparison (2026-09-10)

Static inspection of the allowlisted initialized BP research image identifies the same
high-level HPL chain but not the Rework extensions. `cPlayer+0x274` is the real
`iCharacterBody`; `cPlayer::MoveForward/Sideways` feed
`iCharacterBody::Move(D4F50)`, which updates native acceleration and speed.
`iPhysicsWorld::Update(D45B0)` calls the body once at `D460A -> D6E00` with its
physics timestep. That body update submits its requested transform at
`D7312 -> CheckShapeWorldCollision(D4830)`, then performs native step, gravity
and attachment work before exposing the final accepted position.

This means the reusable policy is still the Framework's tracking delta, 0.05 m
step clamp, accepted-distance projection, rejection reconciliation, speed,
sprint, yaw/recenter and seated/standing logic. The BP adapter must own the
player/body offsets, HPL sphere-or-cylinder representation, exact x86 ABI,
injection point, crouch-size state, jump and solver differences. BP has no
evidence for Overture's added `vr_velocity`, `vr_stepstaticonly`,
`CollidePlayer` or `IsPlayer` fields/arguments, so none may be copied by layout.
The probe measures before/requested/solver/final positions. Those mappings,
jump ownership and the narrow body adapter are live-characterized. The separate
default-off X/Z injection boundary at `0xD7281` is live-tested in PID 26144;
PID 24956 live-exercised the default-off active room-scale consumer through that
boundary and exposed a Black Plague-only sequencing defect: its combined native
tick reused already reconciled physical displacement in locomotion carry. The
adapter/shadow partition is corrected and compiles in the affected Release
targets, but the revision still requires live/headset validation. Overture's
proven two-update sequence was not changed.

Finger articulation points in the other direction. BP's existing shared
`VrHandArticulation` output (independent curls, three joint curves, spread and
thumb opposition) is currently the better demonstrated response and must not
be replaced with Overture's mesh-specific animation. Overture's bind-pose bone
mapping, optional input deadzone/smoothing and radius-dependent hold pose remain
valuable adapter/profile behavior. A future small boundary should map the BP
runtime articulation onto each real mesh and let Overture retain only those
rig-specific details.

## Framework-owned source host (2026-09-10)

The first linked checkpoint used the Rework working tree as the consumer of the
Framework adapter. That proved the real boundary but left Rework structurally in
the build graph. The production-development boundary is now inverted:

```text
products/overture/PenumbraOverture + HPL1Engine + OALWrapper
    -> src/adapters/overture_source
    -> src/backends/overture
    -> src/runtime
```

The Framework now owns the three Win32 projects, their required Overture/HPL/OAL
source, pinned headers/libraries/runtime DLLs, the necessary data and shader
overlays, the legacy tracking test, and all validation/build/package scripts.
The exact product bindings are a deliberate overlay at `assets/openvr/overture`
because the shared Framework bindings are not byte-equivalent to the proven
Overture mappings. Projects and scripts calculate paths from their location and
reject references to `penumbra_vr_rework` or the former sibling-checkout bridge.

Generated build/package output, repository history/CI, HPL tools and samples,
non-Windows dependencies, obsolete project variants, and unrelated authoring
utilities were not migrated. The detailed source-controlled/generated split,
license files and exclusions are recorded in
`products/overture/SOURCE_PROVENANCE.md`.

No behavior or constant was changed for this ownership migration. The only
build-compatibility adjustment is `MinimalRebuild=false` in the Overture game
project's Debug compiler settings because legacy `/Gm` is incompatible with the
adapter's required `/std:c++20` on current MSVC.

## Validation gates

- Framework Debug, Release and no-OpenVR Release build with all targets,
  including the Black Plague probe, and all 25 CTest tests pass in each current
  configuration on 2026-09-10. The focused Overture test also
  verifies that the real-adapter boundary receives the exact frame timestep.
- The Framework-owned Overture Release/Win32 rebuild passes: project
  checks, 16 offline shaders, 8,752 visual-reference checks, 231 texture/decode
  checks, Large Address Aware verification and all 289 `VRTrackingTest` checks.
- The Overture Debug/Win32 full rebuild also passes Large Address Aware
  verification and all 289 `VRTrackingTest` checks after the scoped `/Gm`
  compatibility correction.
- The packaged executable is
  `E:\penumbra_vr\products\overture\build\package\Release\PenumbraVR\Penumbra_vr.exe`,
  3,302,912 bytes, SHA-256
  `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`.
- On 2026-09-10 the user deployed the package through its included
  `Install-PenumbraVR.bat` onto a valid retail Overture installation and ran it
  with SteamVR, a real headset and controllers. The installed/tested executable
  hash matched the Framework package exactly:
  `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`.
  The user reported behavior perceived as equivalent to the previously tested
  Rework build and no evident regression during this first functional pass.
- “Backend core tested”, “linked into Overture”, “startup live-tested”,
  “deployed”, and “validated in headset” are reported separately. Current state:
  implemented, linked and host-tested from the standalone Framework source host;
  deployed and headset-validated for an initial functional integration pass; not
  exhaustively validated across every feature/controller/scenario and not supported.
- Black Plague positional tracking stays off. The body/collision and jump
  boundaries are live-characterized, but room-scale policy awaits the narrow
  adapter and its separate headset validation.

## Remaining exhaustive headset coverage

The autonomous integration milestone no longer needs another deployment proof.
If Overture validation is expanded later, record each item explicitly rather
than inferring it from the successful first pass: tracking orientation and scale;
calibrated height; standing and seated baselines; recenter and both turn modes;
head-relative walk, sprint and constrained movement; jump edge/hold behavior;
physical 0.05 m body following; rejected room-scale motion at walls/doors;
head/body rebase; menus and return to gameplay; and additional controller
families. Record any measured difference before changing a constant.
