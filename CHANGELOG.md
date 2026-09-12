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

### Changed

- Split renderer-neutral eye/target types from the OpenVR session API, and
  narrowed the Black Plague body adapter to explicit owner-status, request and
  callback headers. RVAs, native ownership and runtime behavior are unchanged.
- Added an explicit Black Plague probe capability bitmap and launcher report so
  successful core initialization no longer hides unavailable input, body,
  adapter, ownership or interaction subsystems.
- VR settings updates now use a same-directory temporary copy and atomic
  replacement, preserving unrelated INI sections and avoiding partially saved
  profiles.
- Ignored the local `work/` research scratch directory and removed a historical
  root patch whose changes are already represented by tracked source and
  documentation.

### Fixed

- Made IAT pointer replacement fail transactionally when page-protection
  restoration fails instead of reporting success with a writable hook page.
- Hardened OpenGL telemetry and Black Plague spatial-interaction hook lifecycle:
  partial installs are reported, rollback errors are retained and every hook is
  considered during teardown.
- Hardened shutdown after successful initialization: launcher capability-query
  and required-capability failures now perform compensating shutdown before
  returning failure. Black Plague spatial interaction and native input teardown
  now remove owned hooks before waiting for in-flight callbacks to quiesce.
- Reject signed `SettingsVersion` values and retain the OpenVR loader handle
  when `FreeLibrary` fails so shutdown can be retried accurately.

These maintenance changes are implemented and statically reviewed in this
working tree. No build, test executable, game, SteamVR or headset process was
run in this session, so they have not advanced beyond `implemented`.

### Added

- Extracted the demonstrated tool attachment socket composition into shared
  `vr_grab_pose` policy: per-game model-to-hand orientation and measured model
  grip points now compose through one runtime helper. Black Plague's flashlight
  and glowstick consume that helper with their existing measured sockets, while
  exact matrix tests prove numerical parity with the previous backend-local
  implementation. Release build and all 30 root CTest tests pass; definitive
  hand/tool geometry and light direction remain headset validation gates.
- Re-audited the remaining Rework VR-only systems after that extraction and
  documented the current host-only frontier: dimming, staged loading, physical
  crouch, game UI flows and jointed mechanisms all still require either a real
  second backend consumer or target-specific live/native evidence before a new
  shared abstraction is justified.
- Extracted Rework's controller-to-finger curl conditioning into shared runtime:
  the measured `0.08` skeletal deadzone, grip/trigger fallback closing windows
  and ~70 ms exponential smoothing now have one host-tested implementation.
  Overture consumes the full policy while retaining its rig order, bind poses,
  handle geometry and forced-grab presentation. Black Plague applies the shared
  skeletal deadzone/smoothing before its richer per-finger articulation; its
  non-skeletal fallback remains unchanged until normalized grip/trigger analogs
  are exposed by the backend. The full Overture Release regression and all 30
  root CTest tests pass; no new live/headset validation is claimed.
- Extracted Rework's game-neutral tracked-menu pointer policy into shared runtime
  helpers and applied it to Black Plague: configured-hand ownership now falls
  back to the other tracked hand, controller aim falls back to grip when needed,
  panel-plane hits clamp to the nearest UI edge, and cursor motion uses Rework's
  `0.40` smoothing factor. Release build and all 30 root CTest tests pass; this
  is host-tested only and does not claim headset validation.
- Host-side SteamVR controller-profile parity gate: the shared action manifest
  and all eight functional binding graphs are checked against the preserved
  Overture mappings. The gate caught and restored the Rework-proven left PS VR2
  Sense `L2` UI-select route while leaving hardware behavior for live/headset
  validation.
- Imported the existing Spanish localization payloads for Black Plague and
  Requiem under `assets/localization`, preserving their original `leeme.txt`
  attribution notices. A small localization manifest fixes their exact hashes
  and install-relative destinations, and the metadata gate now verifies both
  payload integrity and attribution-file presence for future unified-installer
  deployment.
- Complete Framework persistence for the shared Rework-derived VR settings schema, plus a host-tested 18-row editor policy matching the Overture menu's ordering, formatting, step sizes, wrapping, clamps and snap/smooth row dependencies.
- An explicit Black Plague VR-setting capability map exposes only backend-wired editor controls; persisted-but-unwired settings remain unavailable to a future in-game page until their backend application exists.
- An offline Black Plague VR configuration surface is available through `PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`. It shows only backend-consumed Rework-derived controls plus monitor mirror, supports snap/smooth dependent rows, saves through the shared settings store and resets only supported controls so persisted-but-unwired values are preserved.
- Exact canonical/LAA build variants for the observed Black Plague and Requiem executables. Catalogue/manifests preserve the canonical semantic build identity while fingerprinting the one-bit transformed executable independently.
- One-shot Black Plague shadow-install telemetry records `disabled`,
  `environment` or transient `mutex` activation, making a live request
  distinguishable from a silent no-tick session. The synthetic default-off,
  environment and mutex paths are covered by the body-probe test.
- Shared stateless tracking/body planning, physical rejection correction and locomotion anchor carry, consumed by Overture without changing the tested sequence.
- Default-off Black Plague tracking/body shadow diagnostics (`PVR_BP_RECONCILIATION_SHADOW=1` or transient validation mutex), using existing tracking and native body callbacks; no physical request injection or positional camera translation. Portable/Windows host tests pass and PID 28172 live-tested the mutex path with positional translation still zero.
- Default-off Black Plague physical-displacement validation path at exact-build RVA `0xD7281`. It injects a one-shot bounded X/Z request (maximum `0.05 m`, Y zero) immediately before native horizontal collision comparison, preserves the original instructions and single `D6E00` owner, and records queue/injection/acceptance telemetry from a pre-injection baseline. PID 26144 live-tested the boundary with positional HMD translation still zero.
- `tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1` verifies the supported initialized executable, launches through Steam, holds the transient physical-validation mutex and fails closed unless the fresh probe log proves a non-zero queued plan, consumed/injected pre-collision request, matched reconciliation, injected body telemetry, the native `dt~=1/60` body tick and sampled stationary/free/block/slide-or-partial cases. The stationary classifier now tolerates sub-2 mm real-HMD jitter instead of requiring zero injection; Rework `23c890f` reacts to any non-zero HMD delta. PID 18392 confirmed that activation alone is rejected, while PID 26144 supplies the completed live evidence.
- Default-off Black Plague active room-scale validation. A dedicated transient request is accepted only together with the live-tested physical-displacement owner; a fresh body-generation-matched shadow sample supplies the reconciled horizontal camera offset, which is applied consistently to head view, visibility and controller space. Samples expire after 250 ms, Y/jump remain native and no additional body tick is introduced. The affected Release targets compile; test executables were not run, so this path remains `implemented`.
- `tools/Start-BlackPlagueRoomScaleValidation.ps1` enables mirror-on persistence, holds both transient validation mutexes and requires fresh evidence for camera application, non-zero X/Z offset, mirror gameplay frames, the native `1.65 -> 0.95 -> 1.65 m` crouch shape sequence, and new free/block/slide outcomes after standing again.
- `PenumbraVR.ProbeLauncher.exe --set-vr-mirror on|off` changes the persisted mirror choice without requiring a running game or an in-game VR settings page.
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
- Real OpenVR action/pose/skeleton/haptic reader, exact-build native input bridge and tracked desktop-menu panels consuming the shared Rework pointer policy.
- Provisional depth-tested procedural gloves, controller-directed native picking and palm-relative free-body grab/throw adapter. Native physics transitions are preserved; joints, palm collisions and definitive tool/light attachment remain pending.
- Per-eye light-scissor remapping at the main executable's `glScissor` import, scoped to the eye context, framebuffer and viewport, with per-frame counters.

### Fixed

- Hardened Black Plague exact-build hook lifecycle handling: partial installs are
  rejected as incomplete, published module identity is kept stable across
  reinstall attempts, and rollback/removal paths preserve callback targets that
  in-flight wrappers may still require. Release build and all 30 root CTest
  tests pass.
- When the Black Plague monitor mirror is disabled, continuous stereo clears the desktop gameplay backbuffer to black instead of leaving stale desktop contents that produced the reported growing white-point artifact. PID 19192 confirmed that gameplay is black while native 2D menus remain visible because they do not execute the suppressed world pass.
- `PenumbraVR.ProbeLauncher` now waits for the actual owner DLL of forwarded
  `LoadLibraryW` to appear in a freshly Steam-started process instead of failing
  immediately during the startup race. The existing process-exit and bounded
  timeout behavior is preserved; PID 28172 subsequently live-confirmed startup past this boundary.
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
- Black Plague active positional HMD translation remains default-off and unvalidated. Its transient validation implementation now consumes the live-tested `0xD7281` route and applies only the reconciled horizontal offset; Release compilation passes, while host, live and headset validation remain pending.
- Black Plague native jump/vertical ownership remains separate from shared horizontal intent. Physical crouch, VR speed tuning and camera/bob comfort are also separate milestones rather than part of the first reconciliation live gate.
- The desktop monitor mirror remains experimental and is not a current supported gameplay feature. PID 19192 confirmed mirror-off gameplay black plus visible native menus. Mirror-on and Alt+Tab/focus recovery remain in the next headset batch because the tracked-menu capture path depends on the desktop framebuffer/focus.
- Rework revision `23c890f` remains the immutable Overture behavioral baseline. Its working tree is not a Framework build or packaging dependency.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with warnings treated as errors at the validated checkpoints.
- The full root CMake configuration now registers **thirty** CTest tests. The established hosted Windows x86 suite still intentionally excludes only the real-driver `opengl_eye_targets` pixel test from the SDK-less runner; newer local tests remain host validation until a subsequent CI run covers them.
- Local offline validation on 2026-09-12 passed the Release build and all **30/30** root CTest tests, including the real-driver `opengl_eye_targets` test. No game, SteamVR or headset process was launched for this validation. The earlier full autonomous Overture Release regression passed the project, 16-shader, 8,752 visual-reference, 231-texture decode and Large Address Aware gates plus **289 `VRTrackingTest` checks with 0 failures**.
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
- Black Plague PID 28172 live-validated the reconciliation shadow request through `source=mutex` with positional translation disabled. Stationary/small physical HMD deltas, native free/block/slide, recenter/body replacement and the existing ~60 Hz single body tick were observed without the shadow writing camera/body position.
- Black Plague PID 26144 live-validated the physical-displacement boundary at `0xD7281` with positional translation zero: queue/injection/matched reconciliation and the native `dt~=1/60` tick were observed. Corrected classification of the same log yields 12 stationary/jitter, 37 free, 2 blocked and 15 slide/partial samples.

### Earlier work in this release

- Continuous Black Plague stereo-presentation start/stop lifecycle using the already validated yaw-aligned rotation tracking.
- Runtime-derived per-eye resolution with proportional allocation fallback.
- Shared HPL1 camera-matrix transaction adapter.
- Host-independent references and tests for Enhanced visuals v4 calibration and the accepted spatial-audio behavior from Overture VR Rework.
- Pure, idempotent x86 PE32 Large Address Aware inspection/transformation with unit tests and exact host-verified Black Plague/Requiem transformed hashes. No installed executable is modified by this work.
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
