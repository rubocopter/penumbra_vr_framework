# Penumbra VR Rework porting plan

This plan treats `rubocopter/penumbra_vr_rework` revision `23c890f` as a proven
Overture reference, not as a binary that can be dropped into every game.
Penumbra VR is GPLv3 or later and records each adapted component in
`THIRD_PARTY.md`.

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
- `assets/openvr` contains the shared action schema and bindings for PSVR2 Sense,
  Vive, Index, Oculus, Pico and Windows motion controllers. These files are
  imported data only until runtime polling and installer registration exist.
