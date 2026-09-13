# Roadmap

Roadmap states are evidence-based. A directory, compiling stub, host test, or code path does not make a feature complete. Headset validation is tracked separately from implementation.

## Phase 0 — Repository baseline

- [x] Independent Git repository
- [x] Honest project status and architectural boundaries
- [x] Known-build fingerprinting tool
- [x] Initial executable fingerprints recorded
- [x] Adopt GPLv3-or-later and begin component-level provenance records
- [x] Record reproducible binary-research workflow

Exit criterion: a clean repository in which every implemented feature is testable and every planned feature is labelled as planned.

## Phase 1 — Black Plague binary probe

- [x] Confirm Steam launch followed by repeatable process attachment
- [x] Log and revalidate module identity from inside the process
- [x] Expose required and optional probe capabilities after initialization so
  a partial research stack is visible to the launcher
- [x] Find and validate the imported `SDL_GL_SwapBuffers` frame boundary
- [x] Document calling convention, modified import slot and teardown
- [x] Complete three attach/frame/detach cycles in one live process
- [ ] Run repeated launch/play/exit cycles without crashes

Exit criterion: one whitelisted Black Plague research hash loads the probe, emits frame telemetry and deactivates cleanly. Hook restoration meets the current research requirement; longer user-driven sessions remain deliberately unchecked.

## Phase 2 — Black Plague visual MVP

- [x] Add non-mutating OpenGL matrix telemetry with tested teardown
- [x] Establish initialized-memory inspection for protected Steam executables
- [x] Map the first HPL routine from evidence (`SetMatrix`, static validation)
- [x] Observe and classify the projection matrix during actual 3D gameplay
- [x] Identify the moving view matrix at the OpenGL boundary
- [x] Locate the active camera and projection path statically
- [x] Map `cRenderer3D::RenderWorld` and its sole `cScene::Render` call site statically
- [x] Validate the `RenderWorld` call site as a safe live hook boundary
- [x] Confirm a current OpenGL context, FBO API and target-size limits at the hook boundary
- [x] Implement and host-test transactional per-eye render-target allocation
- [x] Validate transient allocation, binding, resize, restoration and destruction in game
- [x] Revalidate persistent per-eye targets with resident-DLL teardown policy
- [x] Read per-eye optics and a live HMD pose through game-neutral runtime types
- [x] Validate controlled duplicate world rendering without camera mutation
- [x] Size eye targets from the active OpenVR runtime rather than diagnostic constants
- [x] Unit-test OpenVR-to-HPL projection, rigid inverse and eye-view composition
- [x] Host-test byte-exact camera matrix override and restoration
- [x] Validate reversible per-eye projection and IPD offsets in the live game
- [x] Validate native presentation of the static eye textures in the headset
- [x] Apply and live-validate yaw-aligned rotational head tracking
- [x] Implement continuous start/stop presentation and adaptive runtime-sized targets
- [x] Validate the continuous technical lifecycle at runtime-derived resolution
- [x] Map the exact-build render-list update boundary
- [x] Implement a conservative HMD-aware render-list update
- [x] Live-validate HMD-relative portal/frustum visibility correction
- [x] Confirm continuous visual behavior in the headset
- [x] Submit both eyes to OpenVR
- [x] Preserve functional keyboard and mouse input
- [x] Implement one-step Steam launch, VR activation and read-only preflight
- [x] Host-test tracked menu panels and controller-ray projection
- [ ] Implement a reliable desktop monitor mirror
- [ ] Headset-validate medium-distance lamp lighting and scissor-hook performance
- [ ] Achieve acceptable frame pacing at the headset's target refresh rate
- [x] Implement default-off positional tracking from the reconciled game/body boundary
- [ ] Headset-validate positional tracking with game/body calibration
- [ ] Live-validate direct VR startup and menu/gameplay transitions

**Current milestone note:** native stereo/head rotation, the player/body boundary,
the default-off reconciliation shadow path and the bounded collision-aware
physical X/Z displacement request in metres are now live-tested. PID 26144
demonstrated queue, injection, native collision and matched reconciliation at
`0xD7281` while positional HMD translation remained zero. PID 21548 then passed
the complete active room-scale helper after the combined-tick carry correction;
in-place head tilt no longer caused locomotion. The visual gate still failed:
the world shook continuously and wall rejection felt aggressive. Log evidence
showed centimetre-scale, frequently reversing X/Z presentation changes while
the stick was idle. Rework places its camera at the VR anchor and updates at 90
Hz; the Black Plague consumer instead retained a 60 Hz body sample over the
native smoothed camera. Horizontal render placement now uses the reconciled
anchor plus the newest HMD delta between body ticks. This remains `implemented`
and compiled pending another live/headset validation.
The desktop mirror remains experimental. PID 19192 confirmed that mirror-off
shows 2D menus but suppresses the gameplay world to black, matching the pass
ownership design; mirror-on remains part of the next validation batch.

Exit criterion: stable in-headset stereo rendering and head tracking in representative gameplay and menus, with frame pacing and positional tracking explicitly validated before claiming a complete visual MVP.

## Phase 3 — Shared runtime extraction

- [x] Extract render-scale fallback, visual-calibration and spatial-audio reference behavior
- [x] Import the shared OpenVR action manifest and controller bindings as data
- [x] Extract and unit-test device-independent VR input state, dead-zone and edge routing
- [x] Extract Rework semantic haptic events, profiles, strength scaling and cooldown policy into the shared runtime
- [x] Extract and host-test shared controller-to-finger curl conditioning (skeletal deadzone, grip/trigger fallback windows and smoothing) while preserving per-game rig/articulation ownership
- [ ] Establish and validate controller-profile parity against the proven Overture behavior for PS VR2 Sense, Valve Index/Knuckles, Meta/Oculus Touch, Pico 4, Pico Neo 3, Windows Mixed Reality and HTC Vive, covering action availability, handedness, poses, menu/picking input, haptics and hardware-supported finger articulation
  - [x] Host-gate the shared action manifest and all eight functional binding graphs against the proven Overture profile set; remaining parity work is live/headset behavior per hardware/backend
- [x] Extract Rework palm collision dimensions, sweep/refinement and recovery policy into the shared interaction runtime
- [x] Extract stable VR panel anchoring and transient overlay ownership handoff policy with Overture/Black Plague consumers
- [x] Extract and host-test Rework tracked-menu pointer ownership, aim/grip fallback, off-hand takeover, edge clamping and `0.40` cursor smoothing; Black Plague consumes the shared policy while native menu projection/application remains backend-owned
- [x] Extract and host-test shared attachment-socket composition while retaining measured model grip points/orientation as per-game profile data; Black Plague consumes it for the current flashlight/glowstick sockets
- [ ] Extract remaining demonstrated reusable systems from Overture VR Rework
- [x] Keep Overture's existing build and tests green during extraction
- [x] Port Rework's tracking space, room-scale rejection and locomotion policy
- [x] Introduce the initial narrow Overture body/jump backend contract
- [x] Link the source HPL adapter into the real Overture executable and build a test package
- [x] Migrate the minimal Overture/HPL source host, dependencies and packaging into the Framework
- [x] Remove the Rework working tree from the Overture compile/package dependency graph
- [x] Deploy the autonomous Release overlay and complete an initial functional SteamVR/headset/controller pass without an evident Rework regression
- [x] Extract shared accepted-body-motion observation through `runtime::VrAcceptedBodyMotion`
- [x] Extract and host-test shared tracking/body planning, physical rejection correction and locomotion-anchor carry without changing Overture's intended sequence
- [x] Add the autonomous Overture Release regression pipeline to Windows CI
- [ ] Validate tracking, height, recenter, locomotion, jump and room-scale rejection exhaustively in the headset
- [x] Add host-independent tests for transforms, actions and settings
- [x] Preserve copyright, license and provenance for extracted components

Exit criterion: the shared runtime contains the proven, game-neutral behavior required by the current integrations while each game-specific body/render/input boundary remains explicit. Overture source linkage and the Framework-owned build host are complete; Black Plague now exercises the shared reconciliation policy through a live-tested default-off shadow path over its live-tested binary body adapter.

## Phase 4 — Black Plague gameplay VR

- [x] Connect OpenVR action polling to the imported manifest and bindings
- [x] Code-test native intents, tracked menus and provisional depth-tested gloves
- [ ] Headset-validate the supported controller profiles on Black Plague and close any per-profile feature gaps before controller parity is claimed
- [x] Integrate/code-test free-body palm-relative grab, release and bounded throw
- [x] Limit the generic prop-pick fallback to Rework's `0.18 m` physical reach
- [ ] Complete palm collision, jointed mechanisms and definitive tool/light geometry/profile validation
- [x] Statically map and host-test telemetry for the exact-build player/character-body/native-shape/movement/collision path
- [x] Live-validate the mapped body, active shape, physics timestep and requested/accepted displacement telemetry
- [x] Live-characterize sprint, crouch shape ownership and native jump/vertical ownership
- [x] Live-test the narrow `BlackPlagueBodyAdapter` and shared accepted-displacement observation
- [x] Connect and host-test shared tracking/body reconciliation through the adapter as a default-off shadow path while positional HMD translation remains gated
- [x] Live-validate the shadow-only reconciliation path with translation still zero through the exact-build/mutex validation route
- [x] Implement and host-test a bounded collision-aware physical displacement request separately from native analog movement
- [x] Live-validate that physical request through stationary/free/block/slide cases before enabling room-scale
- [x] Implement a transient, fail-closed room-scale consumer through that reconciled request
- [ ] Revalidate corrected render-rate positional movement, stationary comfort, blocking/sliding and head/body reconciliation in the headset
- [ ] Decide and validate Black Plague VR walk/sprint tuning after reconciliation is stable
- [ ] Complete the active player-camera/head-bob/footstep-bob ownership map needed for comfort work
- [ ] Implement/validate physical crouch without assuming Overture stand-clearance semantics
- [ ] Validate long-body interaction and mechanism-specific states
- [ ] Inventory, notes, menus, HUD and subtitles
- [x] Persist the complete shared VR settings schema and host-test the Rework-derived editor policy
- [x] Define the Black Plague settings capability map from backend-wired behavior
- [x] Provide an offline Black Plague VR settings editor through `PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`
- [ ] Integrate a dedicated Black Plague VR settings page after a safe native-menu insertion boundary is demonstrated
- [ ] Comfort settings and haptics
- [ ] Representative chapter-level validation

Exit criterion: a documented playable alpha for an exact Black Plague build with physical interaction, body/room-scale movement and representative headset validation.

## Phase 5 — Requiem backend

- [ ] Repeat exact-build research rather than assuming Black Plague RVAs
- [ ] Reuse validated HPL-level hooks where binary evidence permits
- [ ] Adapt Requiem-specific gameplay and UI behavior
- [ ] Reuse the shared controller-profile parity matrix and validate Requiem-specific native intent routing without regressing any supported controller profile
- [ ] Complete stereo, input and representative-level validation

Exit criterion: a documented playable alpha for an exact Requiem build.

## Phase 6 — Unified installer and release

- [ ] Installation discovery and manual selection
- [ ] Build compatibility report
- [ ] Transactional backup, install, verify and rollback
- [x] Implement and unit-test the x86 PE Large Address Aware byte transformation
- [x] Record and recognize exact canonical/LAA hash pairs for Black Plague and Requiem
- [ ] Apply LAA only through the known-build-gated transactional backup/rollback path
- [x] Import the shared action manifest and controller bindings
- [ ] Register and deploy action assets through the installer
- [x] Import Black Plague/Requiem Spanish translation payloads with exact hashes and original attribution notices
- [ ] Deploy and roll back the Spanish translation payloads through the unified installer transaction
- [ ] Package attribution and licenses
- [ ] Clean-machine and upgrade testing

Exit criterion: one package safely installs any supported combination of the three games and can fully restore the original installations.
