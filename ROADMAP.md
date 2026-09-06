# Roadmap

Roadmap states are evidence-based. A directory or compiling stub does not make a feature complete.

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
- [x] Find and validate the imported `SDL_GL_SwapBuffers` frame boundary
- [x] Document calling convention, modified import slot and teardown
- [x] Complete three attach/frame/detach cycles in one live process
- [ ] Run repeated launch/play/exit cycles without crashes

Exit criterion: one whitelisted Black Plague research hash loads the probe, emits frame telemetry and deactivates cleanly. Hook restoration now meets this criterion with the DLL intentionally resident until process exit; longer user-driven sessions remain deliberately unchecked.

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
- [x] Implement the Rework-derived optional monitor mirror and default two-pass schedule
- [x] Live-validate the two-pass path and runtime monitor-mirror toggle telemetry
- [x] Implement and host-test eye-resolution light scissoring and GL-state isolation
- [ ] Headset-validate medium-distance lamp lighting and scissor-hook performance
- [ ] Achieve acceptable frame pacing at the headset's target refresh rate
- [ ] Apply positional tracking with game/body calibration
- [x] Submit both eyes to OpenVR
- [x] Preserve functional keyboard and mouse input
- [x] Implement one-step Steam launch, VR activation and read-only preflight
- [x] Host-test tracked menu panels and controller-ray projection
- [ ] Live-validate direct VR startup and menu/gameplay transitions

Exit criterion: stable in-headset stereo rendering and head tracking in representative gameplay and menus.

## Phase 3 — Shared runtime extraction

- [x] Extract render-scale fallback, visual-calibration and spatial-audio reference behavior
- [x] Import the shared OpenVR action manifest and controller bindings as data
- [x] Extract and unit-test device-independent VR input state, dead-zone and edge routing
- [ ] Extract remaining demonstrated reusable systems from Overture VR Rework
- [ ] Keep Overture's existing build and tests green during extraction
- [ ] Introduce a minimal backend contract derived from both integrations
- [x] Add host-independent tests for transforms, actions and settings
- [ ] Preserve copyright, license and provenance information

Exit criterion: Overture and the Black Plague MVP consume the same tested runtime behavior without sharing game-specific addresses or layouts.

## Phase 4 — Black Plague gameplay VR

- [x] Connect OpenVR action polling to the imported manifest and bindings
- [x] Code-test native intents, tracked menus and provisional depth-tested gloves
- [x] Integrate/code-test free-body palm-relative grab, release and bounded throw
- [ ] Complete palm collision, jointed mechanisms and tool/light attachment
- [ ] room-scale body/head relationship
- [ ] hands, grabbing and interactions
- [ ] inventory, notes, menus, HUD and subtitles
- [ ] comfort settings and haptics
- [ ] representative chapter-level validation

Exit criterion: a documented playable alpha for an exact Black Plague build.

## Phase 5 — Requiem backend

- [ ] Repeat exact-build research rather than assuming Black Plague RVAs
- [ ] Reuse validated HPL-level hooks where binary evidence permits
- [ ] Adapt Requiem-specific gameplay and UI behavior
- [ ] Complete stereo, input and representative-level validation

Exit criterion: a documented playable alpha for an exact Requiem build.

## Phase 6 — Unified installer and release

- [ ] installation discovery and manual selection
- [ ] build compatibility report
- [ ] transactional backup, install, verify and rollback
- [x] Implement and unit-test the x86 PE Large Address Aware byte transformation
- [ ] Gate LAA behind known hashes and a transactional backup/rollback operation
- [x] import the shared action manifest and controller bindings
- [ ] register and deploy action assets through the installer
- [ ] package attribution and licenses
- [ ] clean-machine and upgrade testing

Exit criterion: one package safely installs any supported combination of the three games and can fully restore the original installations.
