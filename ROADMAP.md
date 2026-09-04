# Roadmap

Roadmap states are evidence-based. A directory or compiling stub does not make a feature complete.

## Phase 0 — Repository baseline

- [x] Independent Git repository
- [x] Honest project status and architectural boundaries
- [x] Known-build fingerprinting tool
- [x] Initial executable fingerprints recorded
- [ ] Decide project license and attribution policy
- [ ] Record reproducible binary-research workflow

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
- [ ] Apply tracked head view transforms
- [ ] Submit both eyes to OpenVR
- [ ] Preserve functional keyboard and mouse input

Exit criterion: stable in-headset stereo rendering and head tracking in representative gameplay and menus.

## Phase 3 — Shared runtime extraction

- [ ] Extract only demonstrated reusable systems from Overture VR Rework
- [ ] Keep Overture's existing build and tests green during extraction
- [ ] Introduce a minimal backend contract derived from both integrations
- [ ] Add host-independent tests for transforms, actions and settings
- [ ] Preserve copyright, license and provenance information

Exit criterion: Overture and the Black Plague MVP consume the same tested runtime behavior without sharing game-specific addresses or layouts.

## Phase 4 — Black Plague gameplay VR

- [ ] VR actions and controller bindings
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
- [ ] shared manifests and controller bindings
- [ ] package attribution and licenses
- [ ] clean-machine and upgrade testing

Exit criterion: one package safely installs any supported combination of the three games and can fully restore the original installations.
