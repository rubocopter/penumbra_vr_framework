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
- [ ] Implement a reliable desktop monitor mirror
- [ ] Headset-validate medium-distance lamp lighting and scissor-hook performance
- [ ] Achieve acceptable frame pacing at the headset's target refresh rate
- [ ] Apply positional tracking with game/body calibration
- [x] Submit both eyes to OpenVR
- [x] Preserve functional keyboard and mouse input
- [x] Implement one-step Steam launch, VR activation and read-only preflight
- [x] Host-test tracked menu panels and controller-ray projection
- [ ] Live-validate direct VR startup and menu/gameplay transitions

**Current milestone note:** the mirror is intentionally out of the active gameplay path. Existing mirror code/commands must not be treated as a supported feature until the headset/desktop behavior is reliable. Positional tracking remains disabled until the exact Black Plague body/capsule and collision path are mapped.

Exit criterion: stable in-headset stereo rendering and head tracking in representative gameplay and menus, with frame pacing and positional tracking explicitly validated before claiming a complete visual MVP.

## Phase 3 — Shared runtime extraction

- [x] Extract render-scale fallback, visual-calibration and spatial-audio reference behavior
- [x] Import the shared OpenVR action manifest and controller bindings as data
- [x] Extract and unit-test device-independent VR input state, dead-zone and edge routing
- [ ] Extract remaining demonstrated reusable systems from Overture VR Rework
- [x] Keep Overture's existing build and tests green during extraction
- [x] Port Rework's tracking space, room-scale rejection and locomotion policy
- [x] Introduce the initial narrow Overture body/jump backend contract
- [x] Link the source HPL adapter into the real Overture executable and build a test package
- [x] Migrate the minimal Overture/HPL source host, dependencies and packaging into the Framework
- [x] Remove the Rework working tree from the Overture compile/package dependency graph
- [x] Deploy the autonomous Release overlay and complete an initial functional SteamVR/headset/controller pass without an evident Rework regression
- [ ] Validate tracking, height, recenter, locomotion, jump and room-scale rejection in the headset
- [x] Add host-independent tests for transforms, actions and settings
- [x] Preserve copyright, license and provenance for extracted components

Exit criterion: the shared runtime contains the proven, game-neutral behavior required by the current Overture integration milestone, while each game-specific body/render/input boundary remains explicit. Overture source linkage and the Framework-owned build host are complete, and the exact autonomous Release artifact has passed an initial functional headset test. Exhaustive item-by-item equivalence and broader hardware coverage remain separate from this completed integration milestone.

## Phase 4 — Black Plague gameplay VR

- [x] Connect OpenVR action polling to the imported manifest and bindings
- [x] Code-test native intents, tracked menus and provisional depth-tested gloves
- [x] Integrate/code-test free-body palm-relative grab, release and bounded throw
- [x] Limit the generic prop-pick fallback to Rework's 0.18 m physical reach
- [ ] Complete palm collision, jointed mechanisms and tool/light attachment
- [x] Statically map and host-test telemetry for the exact-build Black Plague player/character-body/native-shape/movement/collision path
- [x] Live-validate the mapped BP body, active shape, physics timestep and requested/accepted displacement telemetry (PID 30896; room-scale remains disabled)
- [ ] Complete final player-camera/entity ownership capture; PID 24780 live-validated the corrected action/body probe, while its gravity-disabled camera/entity sites are correctly zero for the active player path
- [ ] Implement the narrow BP body adapter and first shared horizontal player-policy extraction; native jump is live-characterized and remains a separate native move-state operation
- [ ] Resolve head/body spatial reconciliation and physical movement without speculative camera translation
- [ ] Validate long-body interaction and mechanism-specific states
- [ ] inventory, notes, menus, HUD and subtitles
- [ ] comfort settings and haptics
- [ ] representative chapter-level validation

Exit criterion: a documented playable alpha for an exact Black Plague build with physical interaction, body/room-scale movement and representative headset validation.

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
