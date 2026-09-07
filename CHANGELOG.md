# Changelog

This project is pre-alpha. Entries distinguish implemented infrastructure from
features validated in a headset.

## Unreleased

### Added

- Initial `pvr_overture_backend` gameplay core with a narrow HPL body/jump
  adapter boundary, ported from Rework revision `23c890f`.
- Shared tracking-space, height/seated calibration, room-scale collision
  reconciliation, fixed-displacement locomotion and interaction-reach policies.
- Black Plague HMD anchor/current-position, physical-delta and explicit
  positional-scale telemetry for the pending exact-build body adapter.
- One-step Black Plague `--launch-vr` / `Start-Black-Plague-VR.cmd` and read-only
  `--check-vr` preflight; Steam launch, exact-path process matching and saved mirror.
- Real OpenVR action/pose/skeleton/haptic reader, exact-build native input bridge
  and tracked desktop-menu panels with right-controller UI pointing. Code/GL
  tested; not yet live-validated.
- Provisional depth-tested procedural gloves, controller-directed native picking
  and palm-relative free-body grab/throw adapter. Native physics transitions are
  preserved; joints, palm collisions and tool/light attachment remain pending.
- Synthetic-image tests execute the spatial adapter's native-call boundary;
  real-OpenGL tests cover glove pixels, occlusion and restored GL state.
- Regression coverage for native intents, legacy x86 query forwarding, menu rays
  and actual OpenGL menu pixels/state; repeatable initialized-image input verifier.

- Per-eye light-scissor remapping at the main executable's `glScissor` import,
  scoped to the eye context, framebuffer and viewport, with per-frame counters.
- Conservative scissor math and real-OpenGL pixel-coverage/lifecycle regression
  tests; a headset checklist is recorded in `docs/VR_LIGHTING_VALIDATION.md`.

### Fixed

- Generic Black Plague controller picking is capped to Rework's 0.18 m direct
  physical reach instead of granting all props the native camera-ray distance.
- Shared snap-turn activation now uses Rework's post-dead-zone 0.65 threshold;
  smooth and snap turning both require a neutral sample on gameplay entry.
- Interact ownership remains with the grabbing hand until it releases; a second
  controller press no longer transfers ownership or masks release.
- Launcher export relocation now uses the actual owner of forwarded Windows
  exports and rejects a probe loaded from a different build path.
- Eye bindings now isolate and restore scissor enable/box state, preventing a
  previous desktop or light rectangle from clipping the next eye's clear.
- Black Plague stereo now rescales desktop-pixel scissor bounds to eye pixels.
  This targets the reported medium-distance lamp illumination dropout; physical
  confirmation remains pending. No Enhanced visuals shaders are enabled by it.

### Earlier work in this release

- Continuous Black Plague stereo-presentation start/stop lifecycle using the
  already validated yaw-aligned rotation tracking.
- Runtime-derived per-eye resolution with proportional allocation fallback.
- Shared HPL1 camera-matrix transaction adapter.
- Host-independent references and tests for Enhanced visuals v4 calibration and
  the accepted spatial-audio behavior from Overture VR Rework.
- Pure, idempotent x86 PE32 Large Address Aware inspection/transformation with
  unit tests. No installed executable is modified yet.
- Shared OpenVR action manifest and controller bindings, including PSVR2 Sense.
  Runtime polling and provisional procedural hands are connected in the latest
  work; a complete Rework hand/interaction port remains pending.
- Exact-build LAA observations for the installed Black Plague and Requiem Steam
  executables.
- Unified installer transaction and rollback design.
- Repository metadata validation for exact-build manifests, the compiled build
  catalogue and OpenVR action/binding references.
- Windows x86 CI for metadata validation plus Debug and Release builds/tests
  without the external OpenVR SDK.
- Host-independent known-build catalogue and SHA-256 tests.
- Exact-build static mapping of Black Plague's `cScene::UpdateRenderList` call
  and `cRenderer3D::UpdateRenderList` target after the first continuous-session
  visibility report.
- A reversible visibility-camera transaction and conservative symmetric cull
  frustum covering both asymmetric eyes, with a five-degree pose-age guard.
- A Black Plague render-list hook that uses the previous valid tracked pose only
  during continuous stereo and reports update, failure and restoration telemetry.
- Rework-derived stereo scheduling that defaults continuous presentation to two
  eye world passes and permits an optional third monitor-mirror pass.
- Runtime launcher commands to enable or disable the monitor mirror while the
  probe is attached, plus per-frame pass and frame-time-ownership telemetry.
- A device-independent VR input router adapted from Rework, covering radial
  dead-zone scaling, context and handedness edge latching, pose-loss releases
  and the 500 ms SteamVR action-idle grace period.
- A runtime-owned VR settings model adapted from Rework, with shared defaults,
  ranges, enum text conversion, malformed-value normalization, legacy migration
  and framework monitor-mirror state.
- Persistent monitor-mirror preference in `%LOCALAPPDATA%\PenumbraVR\settings.ini`;
  successful live toggles save it and continuous startup reapplies it.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with
  warnings treated as errors.
- Twenty-four host-independent tests pass in OpenVR Release, OpenVR Debug and
  the no-OpenVR Release configuration.
- Static presentation and yaw-aligned rotational tracking passed their bounded
  `512x512` headset sessions.
- Continuous presentation completed a 120.6-second PS VR2 runtime session at
  `3400x3468` per eye: 6,847 tracked stereo frames, no allocation fallback,
  camera-restoration failure or compositor-submit error, and clean stop/teardown.
  The user reported functional keyboard/mouse input and generally good image
  quality, but also missing geometry on HMD turns and nearby object popping.
  A source-matching update boundary and corrective hook are now implemented. A 110.3-second
  runtime run exercised the new update every sampled active frame with zero
  failures and exact camera restoration; the user confirmed that the missing
  walls and nearby-object popping no longer occurred. SteamVR recorded 31.99% reprojection
  in the first run and 33.15% in the corrective run at a 90 Hz target, so frame
  pacing also remains open work.
- The two-pass continuous schedule and optional monitor-mirror schedule pass
  host-independent tests and produced their expected pass/frame-time telemetry
  in a live Black Plague process without stereo or camera-restoration errors.
  Mirror contents and the frame-pacing difference still need explicit visual
  measurement. The logical input router remains host-tested only.

### Changed

- Renamed the GitHub repository from `rubocopter/penumbra_vr` to
  `rubocopter/penumbra_vr_framework`, while retaining **Penumbra VR** as the
  public project name, to distinguish this trilogy framework from the original
  `veryjos/penumbra_vr` Overture mod.
- Moved camera override behavior out of the Black Plague backend into the shared
  HPL1 adapter; exact camera offsets remain backend-owned.
- Documented Enhanced visuals, audio, action and tracking provenance against
  `rubocopter/penumbra_vr_rework` revision `23c890f`.
