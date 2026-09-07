# Overture backend migration and Black Plague comparison

Reference revision: `E:\penumbra_vr_rework`, Git commit `23c890f` (`v0.1.0`).

This document records the evidence used for the first Overture backend in the
framework. `pvr_overture_backend` is a functional, host-tested gameplay core:
it consumes shared input/settings/tracking, drives room-scale and stick motion,
and delegates HPL collision and jump calls through `OvertureBodyAdapter`. It is
not yet linked into the Rework game executable, deployed to the installed game,
or headset-validated. Those three states must not be conflated.

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

## Ownership established by this phase

| System | Runtime | Overture backend / adapter | Black Plague backend |
|---|---|---|---|
| Tracking space | `VrTrackingSpace`: metres, yaw, calibration, posture/seated offsets and tracking-to-world | supplies source HPL body/feet pose | must map the exact-build body before enabling positional translation |
| Locomotion | Rework constants and pure direction, displacement and rejection policy | `OvertureBackend` sequences HPL body moves | consumes the shared 0.18 m physical-reach policy now; native speed/body mapping remains open |
| Collision | requested/accepted displacement reconciliation | `OvertureBodyAdapter::MoveBodyBy` owns `iCharacterBody::Update` and static-only mode | exact-build capsule/body access is still unproved |
| Jump | edge/held semantics | adapter calls native `Jump` and `SetJumpButtonDown` | existing native intent hook; unique-tick diagnostics still required |
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

## Validation gates

- Framework Debug and Release builds pass with all 24 CTest tests on 2026-09-07.
- Rework's full Release build pipeline also passes unchanged on 2026-09-07:
  project checks, 16 offline shaders, 8,752 visual-reference checks and all 289
  `VRTrackingTest` checks.
- “Backend core tested”, “linked into Overture”, “deployed”, and “validated in
  headset” are reported separately.
- Black Plague positional tracking stays off until body/capsule logging proves
  the exact-build adapter and collision rejection in representative scenes.
