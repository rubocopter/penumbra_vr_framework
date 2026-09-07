# Penumbra VR Rework porting plan

This plan treats `rubocopter/penumbra_vr_rework` revision `23c890f` as a proven
Overture reference, not as a binary that can be dropped into every game.
Penumbra VR is GPLv3 or later and records each adapted component in
`THIRD_PARTY.md`.

Current integration policy: retain the framework's responsive per-finger OpenVR
input, adapt Rework interaction/tool behavior at verified boundaries, and tune
final sockets only after the definitive hand meshes are present. The externally
downloaded TurboSquid hand was evaluated as a reference candidate but no mesh,
rig, weights, textures or poses were imported.

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

## Installed-resource evidence

The framework now adapts Rework's VR-sized light clipping through
`src/hooks/opengl_eye_scissor.*`: HPL desktop-pixel scissor rectangles are scaled
only within each stereo eye's destination. The host math and OpenGL pixel tests
pass; the medium-distance lamp dropout still requires a headset retest. This
does not port Enhanced visuals or change the original Rework installation.

Black Plague and Requiem share the installed `redist` tree. It contains both
`Penumbra.exe` and `Requiem.exe`, plus the same HPL resource families needed by
the rework: `hud_object_glowstick`, `hud_object_flashlight`, their inventory
items, light/billboard definitions and Requiem-specific cache/expansion data.
This makes common loaders and grip configuration plausible.

The current rework copies are not byte-identical to the installed Black Plague
resources: the rework deliberately modifies its glowstick and flashlight
geometry/configuration. Therefore these files must be treated as an attributed
overlay with per-game visual validation, not assumed to be interchangeable
because their names match.

## Extraction order

### Immediate priority: PSVR2 Sense playability (2026-09-05)

The user explicitly prioritizes playing with PSVR2 Sense, reusing the working
Overture Rework implementation. As of 2026-09-06, real OpenVR polling, native
intent consumption, tracked menus, provisional gloves and free-body grab/throw
are connected and code-tested. Tool attachment, palm collision and articulated
mechanisms remain pending. Existing bindings and host tests alone must not be
described as a completed/headset-validated Rework port.

The next input implementation should proceed through these acceptance gates:

1. Adapt Rework's `HPL1Engine/sources/input/SteamVRInput.cpp` into the shared
   runtime: manifest registration, action handles, handed gameplay/UI sets,
   hand/aim poses, button/stick sampling and haptics. Keep missing-controller
   and initialization failures nonfatal to the existing keyboard/mouse path.
2. Map Black Plague's input/update and UI-context boundaries from binary
   evidence. Adapt the intent consumption in Rework's
   `PenumbraOverture/ButtonHandler.cpp`: movement, turning, interaction,
   inventory, notebook, lights, pause and menu navigation. Update inputs once
   per game tick, never once per rendered eye. Test focus/tracking loss and
   context changes without leaving a held action stuck.
3. Port the spatial interaction separately, using `PlayerHands.*` and
   `PlayerState_Interact_VR.*` as references: visible hands, controller-directed
   selection, grabbing/releasing objects and tool/light attachment. This
   requires per-game entity/physics/render integration, not different PSVR2
   button definitions.

Do not invent Black Plague addresses, replace these systems with an undocumented
keyboard-emulation layer, or bulk-copy Overture game classes. Report separately
whether actions are merely readable, basic gameplay is connected, and spatial
interaction is headset-validated. The lamp-fix retest remains an independent
pending validation, not a claim that the controller work is already complete.

### Overall sequence

1. Finish stable stereo and head tracking in Black Plague using the shared
   runtime types.
2. Port the device-independent input state, pose validity, haptic interface,
   action manifest generator and controller bindings.
3. Preserve keyboard/mouse while validating one rendered controller hand and
   one aim ray.
4. Port palm collision policy behind a small backend physics-query interface;
   then add selection, grab, release and throw behavior.
5. Adapt flashlight/glowstick attachment using each installed resource's
   measured grip point and light origin.
6. Adapt inventory, notebook, subtitles and room-anchored full-screen UI.
7. Port the package hash manifest and transactional installer mechanics, with
   one payload manifest per exact supported executable.
8. Reuse the resulting runtime in Overture and repeat only the binary-facing
   research for Requiem.

This order proves each boundary in isolation and keeps a failure in a game
adapter from being mistaken for a shared-runtime defect.

## Extracted so far

- `src/runtime/vr_tracking_space.*`, `src/runtime/vr_locomotion.*`,
  `src/runtime/vr_interaction_policy.*` and
  `src/backends/overture/*` port Rework's complete tracking-space boundary,
  seated calibration, room-scale body-step/rejection algorithm and fixed
  1.5/2.25 m/s locomotion behind a narrow HPL body adapter. The backend core is
  code-tested; source-game linkage, deployment and headset equivalence remain
  separate pending gates. The exact Black Plague comparison is in
  `docs/OVERTURE_BACKEND_MIGRATION.md`.

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
- `src/adapters/hpl1/camera_matrix_override.*` now owns the byte-exact camera
  transaction; exact-build backends provide the layout.
- `src/runtime/stereo_render_policy.*` makes Rework's optional monitor mirror
  explicit: continuous headset-only rendering owns frame time on the first eye
  and needs two world passes, while mirroring keeps a separately timed desktop
  pass. The Black Plague hook consumes this policy; headset validation is pending.
- `src/runtime/vr_grab_pose.*` preserves the selected palm/body transform and
  now improves Rework's single-sample throw with a five-sample median, minimum
  history and bounded tracking-discontinuity release. The Black Plague adapter
  restores its exact-build character-collision flag on every owned release.
- `src/runtime/vr_input_state.*` owns logical actions, radial move dead-zone
  scaling, context/handedness edge latching, pose-loss releases and the 500 ms
  action-idle grace period without depending on HPL or OpenVR types. Runtime
  OpenVR polling and exact-build native intent consumption are now connected in
  code; see `VR_STARTUP_AND_CONTROLLERS.md` for unvalidated behavior and limits.
- `src/runtime/vr_settings.*` owns the Rework defaults, limits, enum text
  values and legacy smooth-turn migration, plus framework monitor-mirror state.
  Black Plague now loads handedness, move scale/dead-zone, turn mode/angle/speed/
  dead-zone, menu distance/scale, render scale and mirror from the framework
  INI. The selected hand drives its gameplay/UI action set and pointer; the
  opposite hand carries the light tool. Storage/application of height and the
  remaining settings is pending.
- `assets/openvr` contains the shared action schema and bindings for PSVR2 Sense,
  Vive, Index, Oculus, Pico and Windows motion controllers. These files are
  consumed by the runtime reader and copied beside the probe on each build.
  Physical interaction and unified installer registration remain separate work.
