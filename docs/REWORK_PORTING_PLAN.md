# Penumbra VR Rework porting plan

This plan treats `rubocopter/penumbra_vr_rework` revision `23c890f` as the proven
Overture reference. It is not merely conceptual guidance: when Rework already
solves a VR behavior, its implementation and observed behavior are the primary
source of truth for the Framework. The Framework should port that proven logic,
extracting game-neutral portions and adapting only the game-specific boundaries.
It must not independently reinvent an existing Rework subsystem without a
technical reason documented in the migration audit.

Penumbra VR is GPLv3 or later and records each adapted component in
`THIRD_PARTY.md`.

## Porting rule

For every demonstrated Rework feature, use this order:

1. locate the exact Rework implementation;
2. identify its game-specific dependencies;
3. extract the game-neutral behavior into the shared runtime/interaction layer
   where appropriate;
4. provide a narrow per-game adapter for HPL/entity/physics/render details;
5. preserve the proven behavior and constants unless the target game requires
   a measured change;
6. validate the adapted behavior separately from the source implementation.

A completely new implementation is justified only when Rework has no equivalent,
the target game exposes a technically incompatible interface, or evidence shows
that the Rework implementation cannot safely be adapted. In those cases the
reason must be recorded before the new implementation replaces the proven path.

## Portability boundary

| Rework subsystem | Unified destination | Expected reuse | Per-game work still required |
|---|---|---|---|
| OpenVR poses, optics and compositor lifecycle | shared runtime | high | backend chooses the frame/render boundary |
| tracking-to-world, yaw recenter and height policy | shared runtime | high | backend supplies player/body pose and camera view |
| logical VR input state, action sets and haptics | shared runtime | high | backend maps intents to each game's actions |
| generated bindings for PS VR2, Index, Touch, Pico, WMR and Vive | shared package | nearly direct | remove or remap unavailable game actions |
| settings types, limits and defaults | shared runtime | high | backend or installer supplies configuration storage/UI |
| render scale and per-eye targets | shared runtime | high | backend owns renderer calls and GL-state integration |
| hand pose validity, aim/grip poses and finger curls | shared runtime | high | backend owns HPL model/bone access |
| palm dimensions, sweep/refinement and recovery policy | shared interaction layer | high at the algorithm level | backend wraps HPL physics shapes, contacts and skipped bodies |
| pointer/raycast intent | shared interaction layer | high | backend maps entity queries, focus state and line drawing |
| held-object pose, throw velocity and haptic events | shared interaction layer | medium/high | backend maps ownership and game entity/body APIs |
| flashlight and glowstick grip presentation | shared HPL behavior/config | medium/high | verify each game's resource geometry, light entity and inventory state |
| room-anchored menus, subtitles and cinematics | shared HPL behavior | medium | map each game's draw order and menu state |
| HRTF, occlusion and environmental reverb | shared audio runtime | medium/high | validate the proxy/hook boundary and world ray queries |
| Enhanced visuals v4 calibration, final curve and halo math | shared visual runtime and shader package | high | binary games need mapped material/render-stage hooks; GPU path remains opt-in |
| Large Address Aware requirement | unified installer | direct policy reuse | patch only known x86 PE32 hashes with backup and rollback |
| backup, hash verification, repair and rollback | unified installer | high | manifests and deployed payload differ by game/build |
| Overture melee/weapon state machines | Overture backend only | none for current BP/Requiem scope | do not burden the shared API with unused combat concepts |
| executable addresses, layouts and direct C++ calls | exact-build backend manifests | none | research and validate Black Plague and Requiem independently |

## Current evidence and installed resources

The framework adapts Rework's VR-sized light clipping through
`src/hooks/opengl_eye_scissor.*`: HPL desktop-pixel scissor rectangles are scaled
only within each stereo eye's destination. The host math and OpenGL pixel tests
pass; the medium-distance lamp dropout still requires a headset retest. This
does not port Enhanced visuals or change the original Rework installation.

Black Plague and Requiem share the installed `redist` tree. It contains both
`Penumbra.exe` and `Requiem.exe`, plus the same HPL resource families needed by
the rework: `hud_object_glowstick`, `hud_object_flashlight`, their inventory
items, light/billboard definitions and Requiem-specific cache/expansion data.
The current Rework resources are not byte-identical to the installed Black Plague
resources: the Rework deliberately modifies glowstick and flashlight geometry/configuration.
These files must therefore be treated as attributed overlays with per-game visual
validation, not assumed interchangeable because names match.

## Current extraction: Overture

The first Overture backend milestone is now implemented as `pvr_overture_backend`.
It ports the proven Rework tracking-space, seated/standing calibration, world-yaw,
room-scale rejection/reconciliation and fixed-displacement locomotion sequence
into shared runtime modules. The Overture backend sequences those policies while
a narrow `OvertureBodyAdapter` owns HPL body position, feet height, collision
movement and jump calls.

The extracted locomotion policy preserves Rework's 1.5 m/s normal speed, 2.25 m/s
sprint speed, 0.05 m physical step and accepted/rejected displacement
reconciliation. `src/adapters/overture_source` now supplies the concrete HPL
boundary and is compiled into `Penumbra_vr.exe` by the Framework-owned source
host at `products/overture`. Debug/Release build inputs and the Release package
are self-contained in this repository; Rework `23c890f` is reference-only.
The exact autonomous Release artifact has also passed an initial functional
SteamVR/headset/controller test without an evident Rework regression. Exhaustive
item-by-item equivalence and broader hardware coverage remain separate evidence.

The corresponding Black Plague movement problem remains a binary-adapter task:
feeding its native acceleration/cap system cannot be made Rework-equivalent by
simply multiplying input. The next BP movement step is to map the exact native
body/collision boundary and route the shared displacement policy through it.

## Black Plague gameplay priority

The current Black Plague gameplay work is deliberately incremental. Generic
controller picking is capped to the shared Rework direct physical reach of
`0.18 m`. Free-body grabbing uses the shared palm-relative pose/release policy,
while jointed mechanisms remain a separate backend problem.

The reported long-object behavior is consistent with the current rigid free-body
pose model: bars/tables without a jointed/mechanism state remain rigidly attached
to the palm. This should not be “fixed” by introducing arbitrary spring/offset
behavior; jointed objects need their native mechanism state mapped.

The reported glowstick pose is also geometry-specific. Rework's exact grip
numbers have been identified, but Black Plague uses a different DAE, so those
numbers are not copied blindly. The safe next step is a shared grip-profile type
with a measured Black Plague socket.

The reported head/world displacement issue is not currently attributable to a
Framework head collider. Positional HMD translation is disabled in Black Plague
(`0.0` scale), and the Framework does not yet own a verified BP head/body capsule.
The next safe step is exact-build body/capsule mapping plus collision-resolution
telemetry, followed by controlled headset validation. Do not enable speculative
camera translation as a substitute.

The desktop mirror is intentionally outside the current gameplay milestone.
Existing mirror code may be revisited later by porting the proven Rework mirror
path, but its current Framework implementation is not considered supported or
validated.

## Extraction order

### Current priority: Black Plague body/collision boundary

1. Preserve the completed Overture source host and its initially headset-validated
   Release hash; return to it only for a concrete regression or explicit broader
   validation pass.
2. Map the exact Black Plague body/capsule and collision-resolution boundary;
   add telemetry before enabling positional HMD translation.
3. Route Black Plague locomotion through a measured body displacement adapter,
   preserving the shared Rework locomotion policy rather than its native speed caps.
4. Complete palm collision, jointed mechanisms and tool/light attachment.
5. Validate the long-object and glowstick behavior with the definitive per-game
   geometry/state adapters.
6. Continue inventory, notes, subtitles and room-anchored UI validation.
7. Repeat only exact-build binary research for Requiem where evidence does not
   transfer from Black Plague.

This order proves each boundary in isolation and keeps a failure in a game
adapter from being mistaken for a shared-runtime defect.

## Extracted so far

- `src/runtime/vr_tracking_space.*`, `src/runtime/vr_locomotion.*`,
  `src/runtime/vr_interaction_policy.*` and `src/backends/overture/*` port Rework's
  complete tracking-space boundary, seated calibration, room-scale body-step/
  rejection algorithm and fixed 1.5/2.25 m/s locomotion behind a narrow HPL body
  adapter. The real source-game linkage, Framework-owned source host and Release
  package are complete, and the exact autonomous artifact has an initial
  functional headset validation. Exhaustive Overture coverage remains separate.
  The exact Black Plague comparison is in `docs/OVERTURE_BACKEND_MIGRATION.md`.

- `src/runtime/render_target_policy.*` preserves the Rework scale range,
  default scale and allocation fallback without depending on HPL types.
- `src/graphics/visual_calibration.*` is the CPU reference for the accepted v4
  tone, ambient, dark-diffuse, sharpening and glowstick-halo behavior. The GPU
  shader and renderer-stage adapter are still pending.
- `src/audio/spatial_audio.*` owns the HRTF config text, distance/occlusion
  low-pass calculation and mine-gallery EFX preset. It does not yet claim a
  safe binary audio hook.
- `src/deployment/pe_large_address.*` performs the one-bit PE32 transformation
  in memory. Transactional file replacement remains installer work.
- `src/adapters/hpl1/camera_matrix_override.*` owns the byte-exact camera
  transaction; exact-build backends provide the layout.
- `src/runtime/stereo_render_policy.*` makes the optional monitor mirror explicit,
  but the mirror is not currently a supported/validated gameplay feature.
- `src/runtime/vr_grab_pose.*` preserves the selected palm/body transform and
  uses a five-sample median with minimum history and bounded tracking-discontinuity
  release. The Black Plague adapter restores its exact-build character-collision
  flag on every owned release.
- `src/runtime/vr_input_state.*` owns logical actions, radial move dead-zone
  scaling, context/handedness edge latching, pose-loss releases and the 500 ms
  action-idle grace period without depending on HPL or OpenVR types.
- `src/runtime/vr_settings.*` owns the Rework defaults, limits, enum text values
  and legacy smooth-turn migration, plus framework monitor-mirror state.
- `assets/openvr` contains the shared action schema and bindings for PSVR2 Sense,
  Vive, Index, Oculus, Pico and Windows motion controllers;
  `assets/openvr/overture` preserves the exact Overture product mappings used
  by its package.
