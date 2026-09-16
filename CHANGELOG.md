# Changelog

- 2026-09-16: PID 28412 live-tested the default-off Black Plague no-write palm
  query on the supported build: one callback, eight contacts, unchanged native
  memory and continued normal game ticking. The next isolated stage is now
  implemented and host-tested. Black Plague owns `CreateBoxShape`/`DestroyShape`
  through the pinned exact-build ABI, safely reuses or replaces the palm shape
  with physics-world ownership, and feeds native contacts into a game-neutral
  port of Rework `23c890f` sweep/refinement, slide, overlap recovery, reanchor,
  constrained recovery and rotation resolution. `--validate-palm-resolver`
  creates, queries, reuses and destroys the owned shape on the existing
  post-`D6E00` game-thread owner without publishing the resolved pose to
  gameplay. Synthetic tests cover clear sweep, wall slide, overlap recovery,
  rotation blocking, malformed contacts, same-world reuse, world replacement
  and native gameplay-memory guards. Release, Debug and SDK-less Release each
  pass 34/34 CTest; metadata, the supported-image exact-build verifier and the
  autonomous Overture `-Full` regression also pass. Owned palms/resolver remain
  host-tested until the new live gate succeeds.

- 2026-09-16: PID 25484 supplied focused Black Plague headset evidence for the
  current crouch/Y and short-range room-scale candidate. The run produced 12,858
  presentation frames with zero stereo failures, 2,477 meaningful body samples
  (`2407 free / 43 blocked / 27 partial`), three physical crouch entries/exits,
  final native move-state `0` with the `1.65 m` body, `0.961 m` tracked-Y range
  and 650 accepted direct-locomotion samples. The user reported that the session
  felt good. The run also captured stable button-only crouch and a combined
  Hybrid state, but not the later `physical=0 + latch=1 + state 4` release-hold
  sample; that subgate plus blocked-stand/low-ceiling and deliberate wall/slide
  edges remain open. Documentation was consolidated around the current roadmap,
  architecture, handoffs and validation checklist; superseded post-audit plans
  and tracking/body implementation snapshots were removed.

- 2026-09-14: PID 22096 supplied the first positive headset run for the
  presentation-sequence fix in `7f84235`: 15,990 logged frames, 15,887 gameplay
  frames and zero stereo/compositor failures after menu-to-gameplay transition.
  Re-analysis of the same run isolated the remaining short-X/Z comfort defect:
  2,060 meaningful physical samples included 215 blocked and 90 partial cases,
  while the render stream contained repeated prediction resets around rejected
  collision motion. Rework keeps raw horizontal tracking out of presentation
  until body reconciliation; Black Plague needs render-rate continuation between
  its ~60 Hz body ticks, so the Framework now carries the last physical
  reconciliation into render and removes only the prediction component that
  continues into the last rejected direction. Tangential slide and motion away
  from the obstacle remain untouched. Release, Debug and SDK-less Release pass
  34/34 CTest; metadata and the exact-build BP verifier pass; Overture `-Full`
  passes Release/LAA and 289/289 `VRTrackingTest`. This collision-comfort change
  is still **host-tested only** and needs a fresh headset run before the PID
  20520 pullback report can be closed.

- 2026-09-14: invalidated the follow-up Black Plague headset candidate
  `ca099ca` after PID 6016 reproduced OpenVR compositor error 108 on the
  menu-to-gameplay transition. The prior nested-visibility fix was necessary
  but incomplete: a valid presentation snapshot remained reusable after a
  successful world `Submit`, so a later `RenderWorld` without a newly published
  visibility sample could submit the same compositor sequence again. Presentation
  sequences are now single-consumption: sequence 0, an already-submitted
  sequence, or an older sequence is rejected before rendering/submission.
  Telemetry records stale-sequence rejects and now logs every acquisition,
  nested reuse and stale reject. Debug, Release and SDK-less Release each pass
  34/34 root tests, including the new presentation-sequence policy cases, and
  metadata validation passes. The exact-build BP verifier input and mapping are
  unchanged by this presentation-only patch; its script invocation was blocked
  by the local tool safety layer in this iteration, so no new verifier run is
  claimed. Headset validation is still required before this correction is
  promoted.

- 2026-09-14: invalidated headset candidate `3333be1` after Black Plague PIDs
  23656 and 21396 both failed their first gameplay stereo transition with
  OpenVR compositor error 108 (`VRCompositorError_AlreadySubmitted`). The new
  visibility-owned presentation sampling could reacquire compositor poses from
  `UpdateRenderList` callbacks reached inside the two eye renders, overwriting
  the pre-RenderWorld snapshot and breaking one-wait/one-submit ownership. The
  backend now validates the gameplay camera before sampling and reuses the
  owning presentation snapshot during nested eye callbacks. Debug, Release and
  SDK-less Release each pass 34/34 root tests; metadata, the supported-image BP
  verifier and Overture `-Full` also pass. A fresh headset run is still required
  before the presentation intervention can be promoted.
- 2026-09-14: completed the offline intervention checkpoint for tracking/body,
  crouch ownership, posture snapshots, interaction lifecycle and probe
  teardown. Added the real NativeInputBridge crouch contract harness, a
  partial-lifecycle install/rollback ledger test, shared hand-contact and play
  mode policies, same-tick body transaction coverage for 72/90/120 Hz render
  against a 60 Hz body tick, repeated samples, recenter, slide and jitter, and
  moved rel32 hook diagnostics outside the suspended-thread region. Debug,
  Release and SDK-less Release each pass 34/34 CTest tests; metadata, the
  supported-image BP verifier and diff checks pass. The Overture -Full gate
  was rerun after fixing one stale extracted helper call and passed the Release
  product build, Large Address Aware check and 289-check `VRTrackingTest`.
- Prepared the next Black Plague headset-validation candidate after the
  constrained Push/Move locomotion mapping. Release, Debug and SDK-less Release
  pass 34/34 CTest, metadata and the supported-image verifier pass, and the
  Overture `-Full` regression passes again. The candidate is launched through
  `tools/Start-BlackPlagueRoomScaleValidation.ps1`; this does not promote the
  new crouch/Y, short-X/Z comfort, constrained locomotion or lifecycle work
  beyond host-tested evidence.
- Mapped and pinned the supported Black Plague image boundary needed for a
  future palm adapter: `CheckShapeWorldCollision` at RVA `0xD4830` with its
  nine-argument x86 ABI, legacy callback/contact layout, physics-body shape and
  matrix accessors, `CreateBoxShape` vtable slot, shape user count and
  destruction route. Added a default-off `--validate-palm-query` diagnostic
  that reuses the current player-body shape on the existing post-`D6E00` game
  thread owner, writes only to DLL/stack output and rejects changes to selected
  native world/body/shape bytes. Its synthetic clear/contact/mutation harness
  is host-tested, bringing root CTest to 34 tests. PID 28412 subsequently
  live-tested that no-write query without selected native-memory changes. The
  separate owned-palm lifecycle/resolver gate described above remains
  disconnected from gameplay pending its own live validation.

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

- Reworked the Black Plague room-scale body reconciliation into one explicit
  pre/post native-tick transaction. The existing `D460A -> D6E00` owner now
  plans from the current B0 and latest tracking sample before the native update,
  `0xD7281` consumes the bounded physical request in that same tick, and the
  adapter reconciles once from B1 using matching tick/body/generation evidence.
  The old post-tick plan-for-next-tick path was removed. Host tests cover
  free-space ramps/stops, repeated presentation samples, 60 Hz physics with
  72/90/120 Hz presentation cadence, block/slide/jitter, recenter and body
  replacement. This is host-tested only; the prior short-motion pullback report
  still requires a fresh headset gate.
- Hardened Black Plague crouch and probe lifecycle ownership. Native crouch
  queries are preserved outside an active VR ownership window; VR-owned posture
  is tied to session/player generation and keeps `ChangeMoveState(4/0)` as the
  game-thread application boundary, while the shared crouch policy distinguishes
  desired/effective stance and blocked stand release. Probe startup/shutdown now
  retains an explicit partial-state component ledger, failed rollback/shutdown
  remains retryable, launcher remote-call timeout is reported as indeterminate,
  and rel32 hook setup allocates/opens thread resources before suspending peers.
  These changes are host-tested only; the bridge/lifecycle fault harnesses pass,
  while headset posture validation remains pending.

- Corrected Black Plague VR crouch ownership after PID 24948 showed that the
  previous release/press compensation could alternate the native `1.65/0.95 m`
  body while failing to hold the game's real crouch/stealth state. Static
  exact-build decoding identifies `cPlayer::ChangeMoveState` at `0x9C750`; the
  original normal-state handlers prove move-state `4` is crouch, `0` is walk
  and `3` remains jump. The existing game-thread owner now applies the shared
  Rework desired crouch directly through `ChangeMoveState(4/0)` rather than
  feeding it back through configurable `0x9CFA0/0x9CFD0` press/release
  callbacks. Probe telemetry and the focused helper now require native move
  state, collider shape, button-only latch and Hybrid composition to agree.
  Release build, exact-image verification and 30/30 host tests pass; headset
  validation remains pending.

- Fixed the Black Plague direct metric locomotion gate exposed by PID 17612.
  Full left-stick deflection reached OpenVR frame telemetry while every
  `locomotion_*` field remained zero. Exact-build decoding showed that native
  `MoveForward/MoveSideways` return on `amount == 0` before their later
  `cPlayer+0x264 = 1` write, so that byte cannot be a permission oracle after VR
  analog is removed from the native axes. The bridge now evaluates only the
  exact pre-Move state predicate, preserves real native-axis priority, queues
  accepted VR motion through the existing `0xD7281` owner and mirrors `+0x264`
  after successful direct publication. The exact-image verifier pins the
  relevant instructions and x86 Release compilation passes. Headset validation
  remains pending.
- Corrected the indexed-state layout used by that pre-Move predicate after PID
  28996 showed the first fix still could not publish locomotion. Full controller
  analog continued to reach the runtime, but the replicated lookup had dropped
  the `0x200` portion of both field pairs and swapped vector/index ownership.
  The bridge now follows the exact image: `vector +0x2C4 / index +0x2BC` and
  `vector +0x2D8 / index +0x2D0`; the exact-build verifier pins those loads.
  PID 8092 headset-validated the corrected path: stick locomotion worked in all
  directions, the helper reported `direct_locomotion=True`, all four physical
  outcome classes and `201/201/201/201` queue/consume/inject/match evidence,
  with native crouch-shape recovery also passing. Physical crouch by tracked
  height remains a separate feature gate.
- Replaced Black Plague's direction-dependent native VR acceleration during the
  transient room-scale gate with Rework's direct metric locomotion policy. PID
  11804 confirmed that tracking-only heading chose the correct visual direction
  but exposed slower backward/lateral native axes when the hidden body and HMD
  headings differed. Full-deflection stick now requests `1.5 m/s` walking or
  `2.25 m/s` sprinting in the current HMD world direction. The request is merged
  into the existing `0xD7281` collision owner, physical motion receives priority
  inside the existing `0.05 m` combined bound, and accepted physical/locomotion
  components are separated before anchor reconciliation. Keyboard and rejected
  native player states retain their native path. New probe fields expose both
  requested/injected/accepted components. This is host-tested only.
- Replaced Black Plague's camera-derived stick-heading remap with the proven
  Rework tracking boundary: horizontal locomotion yaw is now measured directly
  from the recenter HMD anchor to the current raw HMD orientation. Pitch/roll
  cannot steer walking, invalid/near-vertical samples fail closed, and recenter
  no longer needs to repair a stale rendered-camera basis. The shared tracking
  math has a host test. PID 11804 headset-exercised this correction and confirmed
  the requested direction; the axis-speed mismatch above remained.
- Corrected the Black Plague physical-displacement helper's blocked/slide
  classification after PID 13672 produced a false-negative blocked result even
  though the wall-block case had been performed. The helper now mirrors Rework
  `23c890f`: accepted displacement is projected onto the requested direction,
  and only that component reduces rejected distance. Native lateral collision
  correction can no longer hide a substantially rejected direct request. This
  changes validation classification only; gameplay collision behavior is
  unchanged.
- Corrected the Black Plague active room-scale presentation after PID 21548
  passed the technical gate but exposed continuous world shake. In stick-zero
  telemetry the reconciled X/Z offset changed by a median 15.77 mm between
  logged samples and repeatedly reversed direction. Unlike Rework `23c890f`,
  the backend was retaining a ~60 Hz body offset over Black Plague's native
  smoothed/bobbed camera. Rendering now derives horizontal camera placement
  directly from the fresh reconciled head anchor and advances it with the HMD
  delta since that body sample. Camera, visibility and controller space consume
  the same placement; discontinuities fail closed, Y/jump remain native and no
  body tick or hook is added. Separate telemetry records the reconciled offset,
  render prediction and final head anchor.
- Corrected Black Plague room-scale anchor carry after PID 24956 exposed a
  sequence difference from Rework `23c890f`. Black Plague's one owned native
  tick contains both native locomotion and the previously queued physical
  request; the shadow consumer was carrying the anchor with that whole combined
  displacement after already reconciling the physical component. It now removes
  the matched physical displacement before Rework's locomotion-only carry while
  retaining the actual whole-tick body position for camera space and telemetry.
  New telemetry reports the partition as `locomotion_carry`. The same session
  reported character walking during in-place head tilt; the correction has not
  yet been tested in a headset and is not claimed to resolve that symptom.
- Relaxed the room-scale helper's redundant post-crouch collision repetition.
  PID 24956 proved `1.65 -> 0.95 -> 1.65 m`, immediate room-scale recovery, 414
  free and 25 slide/partial samples after standing; requiring another blocked
  sample after the already proven global blocked case added no distinct shape
  evidence. The helper now requires meaningful post-recovery physical movement.
- Corrected the VR settings transaction flush. The documented all-null
  `WritePrivateProfileStringW` cache-flush call returns zero even when it
  performs the flush, so treating that value as failure rejected mirror/config
  saves and surfaced `ERROR_FILE_NOT_FOUND`. The transaction now performs that
  cache flush without interpreting its return and then durably flushes the
  temporary file handle before atomic replacement.
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

These maintenance changes and both room-scale corrections compile in both
Release configurations. No test executable, game, SteamVR or headset process was
run while preparing the presentation correction; PID 13672 later
headset-exercised it and the user reported the prior continuous world shake gone.
PID 11804 headset-exercised the heading correction and reported stable general
comfort plus correct visual direction, but did not capture fresh blocked or
slide/partial evidence. The subsequent direct metric locomotion change compiles
in Release and its combined-request/reconciliation host tests pass; it has not
yet been live/headset-tested in Black Plague.

### Added

- Added Rework-derived crouch ownership as shared runtime policy. It preserves
  `23c890f`'s single button latch, Hybrid OR composition, plausible
  `(0.90, 2.20) m` raw-HMD range, upward-settling standing baseline, configured
  crouch depth (`0.25 m` default) and `0.08 m` exit hysteresis. Black Plague
  exposes `CrouchMode`, `PhysicalCrouchDepth` and `HeightOffset`; its existing
  game-thread input owner applies the desired stance through Black Plague's
  exact `cPlayer::ChangeMoveState` boundary, retries a blocked stand and logs policy/body
  correlation without adding another hook. Rendering now uses shared
  `VrTrackingSpace` for continuous tracked Y from the reconciled feet anchor.
  PID 20520 exercised the previous edge-only integration and exposed a false
  helper pass: policy exits and the native `1.65/0.95 m` shape were not aligned.
  The corrected latch, desired/native synchronization, vertical placement and
  stricter validator are implemented and host-tested only.
- Added Black Plague probe telemetry for the fresh movement yaw consumed by the
  native-input stick remap. PID 13672 headset-exercised the render-rate anchor
  correction and the user reported the prior continuous world shake gone, but
  later testing found that forward stick direction can feel offset unless
  recentered. That evidence led to the tracking-only heading correction above.
  The next headset gate keeps `movement_yaw_valid` and `movement_yaw_rad` beside
  raw controller input to verify the new basis before further locomotion or turn
  ownership changes.
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
- Black Plague direct locomotion now maps exact-build action states `1` (Push) and `2` (Move) to the shared Rework-derived constrained `0.5 m/s` policy. The mapping is pinned by exact-image evidence (`MaxPushSpeed`, the Push `ChangeMoveState(2)` transition and the Move native body callback) and covered by the native-input contract harness; it remains host-tested only.
- Default-off Black Plague tracking/body shadow diagnostics (`PVR_BP_RECONCILIATION_SHADOW=1` or transient validation mutex), using existing tracking and native body callbacks; no physical request injection or positional camera translation. Portable/Windows host tests pass and PID 28172 live-tested the mutex path with positional translation still zero.
- Default-off Black Plague physical-displacement validation path at exact-build RVA `0xD7281`. It injects a one-shot bounded X/Z request (maximum `0.05 m`, Y zero) immediately before native horizontal collision comparison, preserves the original instructions and single `D6E00` owner, and records queue/injection/acceptance telemetry from a pre-injection baseline. PID 26144 live-tested the boundary with positional HMD translation still zero.
- `tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1` verifies the supported initialized executable, launches through Steam, holds the transient physical-validation mutex and fails closed unless the fresh probe log proves a non-zero queued plan, consumed/injected pre-collision request, matched reconciliation, injected body telemetry, the native `dt~=1/60` body tick and sampled stationary/free/block/slide-or-partial cases. The stationary classifier now tolerates sub-2 mm real-HMD jitter instead of requiring zero injection; Rework `23c890f` reacts to any non-zero HMD delta. PID 18392 confirmed that activation alone is rejected, while PID 26144 supplies the completed live evidence.
- Default-off Black Plague active room-scale validation. A dedicated transient request is accepted only together with the live-tested physical-displacement owner; a fresh body-generation-matched shadow sample supplies the reconciled horizontal anchor. PID 21548 captured all physical classes and crouch recovery and confirmed that the carry correction removed locomotion from in-place head tilt, but continuous world shake prevented headset validation. The latest implementation places X/Z from that anchor and continues it with the current render pose; samples expire after 250 ms, Y/jump remain native and no additional body tick is introduced.
- `tools/Start-BlackPlagueRoomScaleValidation.ps1` enables mirror-on persistence, holds both transient validation mutexes and requires fresh evidence for camera application, non-zero reconciled X/Z offset, render-rate HMD continuation, mirror gameplay frames, the native `1.65 -> 0.95 -> 1.65 m` crouch shape sequence, and meaningful physical movement after standing again.
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
- Black Plague active positional HMD translation remains default-off outside its
  validation request. PID 21548 live-tested the carry partition, PID 13672
  headset-exercised stable render-rate horizontal placement, PID 11804 confirmed
  correct HMD-relative direction, and PID 8092 headset-validated the corrected
  direct Rework `1.5/2.25 m/s` locomotion plus the room-scale technical gate
  through the same single `0xD7281` collision owner. This is still a research
  validation state rather than a supported-release claim.
- Black Plague native jump ownership remains separate from shared horizontal intent. Physical crouch ownership and tracked-Y presentation are implemented and host-tested after PID 20520; final headset comfort plus camera/body/footstep bob remain separate milestones.
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
