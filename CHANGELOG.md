# Changelog

This project is pre-alpha. Entries distinguish implemented infrastructure from features validated in a headset.

## Unreleased

### Added

- Initial `pvr_overture_backend` gameplay core with a narrow HPL body/jump adapter boundary, ported from Rework revision `23c890f`.
- Source-level Overture integration that compiles the Framework backend into `Penumbra_vr.exe`, maps the existing HPL input/settings/tracking types, and implements `OvertureBodyAdapter` with the real `cPlayer` and `iCharacterBody` calls.
- Framework-owned Overture product host under `products/overture`, containing the required Penumbra/HPL/OAL source, pinned Win32 dependencies, resource inputs, build/package gates and retained upstream notices.
- Overture-specific OpenVR binding overlay under `assets/openvr/overture`; shared bindings remain available to the other backends.
- Shared tracking-space, height/seated calibration, world-yaw, room-scale collision reconciliation, fixed-displacement locomotion and interaction-reach policies.
- Black Plague HMD anchor/current-position, physical-delta and explicit positional-scale telemetry for the pending exact-build body adapter.
- Exact-build Black Plague player/character-body/native-shape/movement/collision
  mapping plus read-only telemetry for body/feet position, physics timestep,
  requested displacement, immediate solver output, final accepted displacement
  and unapplied HMD/body divergence. The probe is live-tested for the mapped
  body/collision and native jump pipeline; positional translation is still
  disabled.
- One-step Black Plague `--launch-vr` / `Start-Black-Plague-VR.cmd` and read-only `--check-vr` preflight.
- Real OpenVR action/pose/skeleton/haptic reader, exact-build native input bridge and tracked desktop-menu panels with right-controller UI pointing. These paths remain headset-validation work unless explicitly marked otherwise.
- Provisional depth-tested procedural gloves, controller-directed native picking and palm-relative free-body grab/throw adapter. Native physics transitions are preserved; joints, palm collisions and tool/light attachment remain pending.
- Per-eye light-scissor remapping at the main executable's `glScissor` import, scoped to the eye context, framebuffer and viewport, with per-frame counters.

### Fixed

- Generic Black Plague controller picking is capped to Rework's 0.18 m direct physical reach instead of granting all props the native camera-ray distance.
- Shared snap-turn activation uses Rework's post-dead-zone 0.65 threshold; smooth and snap turning both require a neutral sample on gameplay entry.
- Interact ownership remains with the grabbing hand until it releases; a second controller press no longer transfers ownership or masks release.
- Launcher export relocation now uses the actual owner of forwarded Windows exports and rejects a probe loaded from a different build path.
- Eye bindings isolate and restore scissor enable/box state, preventing a previous desktop or light rectangle from clipping the next eye's clear.
- Black Plague stereo rescales desktop-pixel scissor bounds to eye pixels. This targets the reported medium-distance lamp illumination dropout; physical confirmation remains pending.

### Current integration state

- `pvr_overture_backend` now ports the proven Rework tracking-space, room-scale rejection/reconciliation and 1.5/2.25 m/s locomotion policy behind an Overture-specific HPL body/jump adapter. That adapter is linked into a real `Penumbra_vr.exe` from the Framework-owned source host. The exact autonomous Release artifact has been deployed and functionally exercised in a headset; exhaustive equivalence remains a separate evidence gate.
- The migration audit records the explicit REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION comparison and is the authoritative reference for what was ported versus what still requires a game-specific adapter.
- Black Plague positional HMD translation remains disabled. The exact native
  character-body and collision boundary is mapped and live-characterized;
  implementation of the narrow displacement adapter remains the next step.
- The desktop monitor mirror is intentionally not a current supported feature. Existing code/commands remain experimental and must not be counted as a validated gameplay capability.
- Rework revision `23c890f` remains the immutable behavioral baseline. Its working tree and the earlier consumer-side integration delta are no longer build or packaging dependencies and may be reset without affecting the Framework product.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with warnings treated as errors.
- Twenty-five host-independent tests pass in OpenVR Release, OpenVR Debug and
  the no-OpenVR Release configuration, including the exact-build BP
  body/collision observation test.
- The Framework-owned Overture Release pipeline passes: project checks, 16 shader compilations, 8,752 CPU visual checks, 231 texture selection/decode checks, 289 `VRTrackingTest` checks, Large Address Aware verification and package validation.
- The Framework-owned Overture Debug full rebuild also passes Large Address Aware verification and all 289 `VRTrackingTest` checks; only the game project disables legacy `/Gm` to coexist with C++20.
- The user deployed the autonomous Release overlay with `Install-PenumbraVR.bat` and ran the exact packaged executable (SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`) with SteamVR, a real headset and controllers. The first functional pass felt equivalent to the previously tested Rework behavior and exposed no evident regression. This is initial headset validation, not exhaustive feature/hardware coverage or a supported-release claim.
- Black Plague native stereo, yaw-aligned rotational tracking, keyboard/mouse preservation and HMD-aware visibility have previously been validated in the headset. Those results remain valid; newer controller/gameplay and lighting corrections are not automatically headset-validated by compilation or host tests.

### Earlier work in this release

- Continuous Black Plague stereo-presentation start/stop lifecycle using the already validated yaw-aligned rotation tracking.
- Runtime-derived per-eye resolution with proportional allocation fallback.
- Shared HPL1 camera-matrix transaction adapter.
- Host-independent references and tests for Enhanced visuals v4 calibration and the accepted spatial-audio behavior from Overture VR Rework.
- Pure, idempotent x86 PE32 Large Address Aware inspection/transformation with unit tests. No installed executable is modified yet.
- Shared OpenVR action manifest and controller bindings, including PSVR2 Sense.
- Exact-build LAA observations for the installed Black Plague and Requiem Steam executables.
- Unified installer transaction and rollback design.
- Repository metadata validation for exact-build manifests, the compiled build catalogue and OpenVR action/binding references.
- Windows x86 CI for metadata validation plus Debug and Release builds/tests without the external OpenVR SDK.
- Exact-build static mapping of Black Plague's `cScene::UpdateRenderList` call and `cRenderer3D::UpdateRenderList` target.
- A reversible visibility-camera transaction and conservative symmetric cull frustum covering both asymmetric eyes, with a five-degree pose-age guard.
- A Black Plague render-list hook that uses the previous valid tracked pose only during continuous stereo and reports update, failure and restoration telemetry.
- Rework-derived stereo scheduling that defaults continuous presentation to two eye world passes and permits an optional third monitor-mirror pass.
- Runtime launcher commands to enable or disable the monitor mirror while the probe is attached, plus per-frame pass and frame-time-ownership telemetry.
- A device-independent VR input router adapted from Rework, covering radial dead-zone scaling, context and handedness edge latching, pose-loss releases and the 500 ms SteamVR action-idle grace period.
- A runtime-owned VR settings model adapted from Rework, with shared defaults, ranges, enum text conversion, malformed-value normalization, legacy migration and framework monitor-mirror state.
- Persistent monitor-mirror preference in `%LOCALAPPDATA%\PenumbraVR\settings.ini`.

### Changed

- Renamed the GitHub repository from `rubocopter/penumbra_vr` to `rubocopter/penumbra_vr_framework`, while retaining **Penumbra VR** as the public project name, to distinguish this trilogy framework from the original `veryjos/penumbra_vr` Overture mod.
- Moved camera override behavior out of the Black Plague backend into the shared HPL1 adapter; exact camera offsets remain backend-owned.
- Documented Enhanced visuals, audio, action, tracking and locomotion provenance against `rubocopter/penumbra_vr_rework` revision `23c890f`.
