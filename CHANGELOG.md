# Changelog

- Extracted the demonstrated Overture/Rework palm collision policy into
  `src/runtime/vr_interaction_policy.hpp`: palm dimensions, contact tolerances,
  sweep/refinement limits and recovery predicates now have one Framework-owned
  source of truth. The autonomous Overture product keeps its legacy
  `VRHandCollisionPolicy` API through a compatibility namespace, leaving native
  physics/contact queries and body exclusions for each backend adapter.
- Extracted Rework's semantic haptic event profiles, strength scaling and
  per-event cooldown policy into `src/runtime/vr_haptics.hpp`. Overture now
  consumes that shared policy through its existing `cVRHaptics` boundary, and
  Black Plague's existing pickup/drop feedback uses the same proven profiles
  while device submission remains backend-owned.
- Extracted the demonstrated stable-panel anchor lifetime and transient overlay
  ownership handoff into `src/runtime/vr_panel_policy.hpp`. Black Plague's
  tracked menu anchor and Overture's radio/subtitle overlay now consume the
  shared policy while retaining their renderer/game-specific placement code.
- Completed the shared mine-gallery EFX reference with the remaining accepted
  Rework echo, modulation and room-rolloff fields and expanded its host test to
  lock the full preset.
- Fixed the Overture binding generator's ordered-map construction for Windows
  PowerShell; `-Check` again validates all eight generated controller bindings.

This project is pre-alpha. Entries distinguish implemented infrastructure from features validated live or in a headset.

## Unreleased

### Added

- One-shot Black Plague shadow-install telemetry records `disabled`,
  `environment` or transient `mutex` activation, making a live request
  distinguishable from a silent no-tick session. The synthetic default-off,
  environment and mutex paths are covered by the body-probe test.
- Shared stateless tracking/body planning, physical rejection correction and locomotion anchor carry, consumed by Overture without changing the tested sequence.
- Default-off Black Plague tracking/body shadow diagnostics (`PVR_BP_RECONCILIATION_SHADOW=1`), using existing tracking and native body callbacks; no physical request injection or positional camera translation. Portable tests and Windows x86 host validation pass; live shadow validation remains pending.
- Dedicated Windows CI regression job for the autonomous Framework-owned Overture Release pipeline. It forces a clean full product rebuild and runs the retained project/shader/visual/texture/LAA/`VRTrackingTest` gates after shared-runtime changes.
- Initial `pvr_overture_backend` gameplay core with a narrow HPL body/jump adapter boundary, ported from Rework revision `23c890f`.
- Source-level Overture integration that compiles the Framework backend into `Penumbra_vr.exe`, maps the existing HPL input/settings/tracking types, and implements `OvertureBodyAdapter` with the real `cPlayer` and `iCharacterBody` calls.
- Framework-owned Overture product host under `products/overture`, containing the required Penumbra/HPL/OAL source, pinned Win32 dependencies, resource inputs, build/package gates and retained upstream notices.
- Overture-specific OpenVR binding overlay under `assets/openvr/overture`; shared bindings remain available to the other backends.
- Shared tracking-space, height/seated calibration, world-yaw, room-scale collision reconciliation, fixed-displacement locomotion and interaction-reach policies.
- Shared `runtime::VrAcceptedBodyMotion`, a game-neutral observation of finite body-before/body-after positions and accepted displacement. Overture now consumes it without changing its proven reconciliation behavior; Black Plague exposes the same observation through its exact-build body boundary.
- Exact-build Black Plague player/character-body/native-shape/movement/collision mapping plus read-only telemetry for body/feet position, physics timestep, requested displacement, immediate solver output, final accepted displacement and unapplied HMD/body divergence.
- Live-characterized Black Plague sprint/crouch/jump ownership: native walk/sprint limits around `3.0/4.5 m/s`, physical crouch shape swap preserving feet height, and a separate native Jump-state vertical pipeline.
- First narrow `BlackPlagueBodyAdapter`, binding to the already-owned movement/body-update callsites without introducing another hook or second `D6E00` call. It dynamically re-resolves the current body, publishes existing native horizontal intent and observes accepted displacement after the one native update.
- One-step Black Plague `--launch-vr` / `Start-Black-Plague-VR.cmd` and read-only `--check-vr` preflight.
- Real OpenVR action/pose/skeleton/haptic reader, exact-build native input bridge and tracked desktop-menu panels with right-controller UI pointing.
- Provisional depth-tested procedural gloves, controller-directed native picking and palm-relative free-body grab/throw adapter. Native physics transitions are preserved; joints, palm collisions and definitive tool/light attachment remain pending.
- Per-eye light-scissor remapping at the main executable's `glScissor` import, scoped to the eye context, framebuffer and viewport, with per-frame counters.

### Fixed

- `PenumbraVR.ProbeLauncher` now waits for the actual owner DLL of forwarded
  `LoadLibraryW` to appear in a freshly Steam-started process instead of failing
  immediately during the startup race. The existing process-exit and bounded
  timeout behavior is preserved; live confirmation remains pending.
- The Black Plague shadow validation helper now verifies the fresh probe log and
  fails closed unless the requested session reports
  `body_reconciliation_shadow enabled=1 source=mutex`, preventing a silent
  shadow-off launch from being counted as validation.
- Black Plague body-adapter installation now respects single-owner callsites. `NativeInputBridge` remains the sole owner of `MoveForward/MoveSideways`, `BodyCollisionProbe` remains the sole owner of `D460A -> D6E00`, and the adapter binds through their verified live status instead of re-validating pristine bytes or stacking another hook.
- Body-adapter mismatch diagnostics now identify concern, RVA, pristine/live instruction bytes, decoded targets and owner state.
- Generic Black Plague controller picking is capped to Rework's `0.18 m` direct physical reach instead of granting all props the native camera-ray distance.
- Shared snap-turn activation uses Rework's post-dead-zone `0.65` threshold; smooth and snap turning both require a neutral sample on gameplay entry.
- Interact ownership remains with the grabbing hand until it releases; a second controller press no longer transfers ownership or masks release.
- Launcher export relocation now uses the actual owner of forwarded Windows exports and rejects a probe loaded from a different build path.
- Eye bindings isolate and restore scissor enable/box state, preventing a previous desktop or light rectangle from clipping the next eye's clear.
- Black Plague stereo rescales desktop-pixel scissor bounds to eye pixels. This targets the reported medium-distance lamp illumination dropout; physical confirmation remains pending.

### Current integration state

- Overture now builds entirely from this repository. `pvr_overture_backend` preserves the proven Rework tracking-space, room-scale rejection/reconciliation and `1.5/2.25 m/s` locomotion policy behind an Overture-specific HPL body/jump adapter. The exact autonomous Release artifact has been deployed and functionally exercised in a headset; exhaustive equivalence remains a separate evidence gate. The autonomous Release pipeline is also now a dedicated CI regression job.
- The migration audit records the explicit `REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION` comparison and remains the authoritative reference for what was ported versus what still requires game-specific mechanism.
- Black Plague positional HMD translation remains disabled. The exact native body/collision boundary and first narrow body adapter are live-tested. Shared tracking/body reconciliation is now connected as a default-off, no-write shadow consumer and is host-tested on Windows x86. The next gate is a local exact-build verification followed by a shadow-only live capture; active room-scale still requires a separately demonstrated collision-aware physical displacement request.
- Black Plague native jump/vertical ownership remains separate from shared horizontal intent. Physical crouch, VR speed tuning and camera/bob comfort are also separate milestones rather than part of the first reconciliation live gate.
- The desktop monitor mirror remains experimental and is not a current supported gameplay feature.
- Rework revision `23c890f` remains the immutable Overture behavioral baseline. Its working tree is not a Framework build or packaging dependency.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with warnings treated as errors at the validated checkpoints.
- The full root CMake configuration now registers **twenty-seven** CTest tests. GitHub Actions passes the established Windows x86 Debug and Release host suite while intentionally excluding only the real-driver `opengl_eye_targets` pixel test from the SDK-less hosted runner.
- GitHub Actions run `34616035023` host-validated feature commit `18c63ef` on Windows Server 2022: metadata/OpenVR assets, Visual Studio 2022 Win32 configuration, Debug build/CTest and Release build/CTest all passed.
- GitHub Actions run `34616820448` repeated the root Windows x86 gate after CI hardening and also passed the new `Overture Release regression` job using `Build-OvertureProduct.ps1 -Configuration Release -Full`.
- The Framework-owned Overture Release pipeline passes project checks, 16 shader compilations, 8,752 CPU visual checks, 231 texture selection/decode checks, 289 `VRTrackingTest` checks, Large Address Aware verification and package validation. This pipeline has now been revalidated after the shared tracking/body extraction by the dedicated Windows CI job.
- The Framework-owned Overture Debug full rebuild also passes Large Address Aware verification and all 289 `VRTrackingTest` checks; only the game project disables legacy `/Gm` to coexist with C++20.
- The user deployed the autonomous Overture Release overlay with `Install-PenumbraVR.bat` and ran the exact packaged executable (SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`) with SteamVR, a real headset and controllers. The first functional pass felt equivalent to the previously tested Rework behavior and exposed no evident regression. This is initial headset validation, not exhaustive feature/hardware coverage or a supported-release claim.
- Black Plague native stereo, yaw-aligned rotational tracking, keyboard/mouse preservation and HMD-aware visibility have previously been validated in the headset.
- Black Plague PID 30896 live-validated the mapped `0.70 x 1.65 m` player body, `dt=1/60`, free movement and collision rejection/slide behavior with positional HMD translation still disabled.
- Black Plague PID 24780 live-validated corrected sprint/jump/crouch action ownership and the native `1.65 m → 0.95 m` crouch shape swap preserving feet height.
- Black Plague PID 29672 completed a 240-tick jump burst, confirming separate native vertical ownership, about `+5.53 m/s` initial accepted vertical speed, ~`0.95 m` apex above baseline and native landing/state restoration.
- Black Plague PID 8628 live-validated the first `BlackPlagueBodyAdapter`: input/body owners and adapter installed together, free movement/block/slide remained intact, body replacement did not leave a stale cached pointer, and the body update remained about 60 Hz with no evidence of a second `D6E00` call.

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
- A runtime-owned VR settings model adapted from Rework, with shared defaults, ranges, enum text values and legacy smooth-turn migration, plus framework monitor-mirror state.
- Persistent monitor-mirror preference in `%LOCALAPPDATA%\PenumbraVR\settings.ini`.

### Changed

- Renamed the GitHub repository from `rubocopter/penumbra_vr` to `rubocopter/penumbra_vr_framework`, while retaining **Penumbra VR** as the public project name, to distinguish this trilogy framework from the original `veryjos/penumbra_vr` Overture mod.
- Moved camera override behavior out of the Black Plague backend into the shared HPL1 adapter; exact camera offsets remain backend-owned.
- Documented Enhanced visuals, audio, action, tracking and locomotion provenance against `rubocopter/penumbra_vr_rework` revision `23c890f`.
- Clarified the extraction rule: Rework remains the proven Overture reference, while a better demonstrated implementation from another backend may become the shared baseline when the reusable behavior is genuinely game-neutral.
