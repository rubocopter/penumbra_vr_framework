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
- [x] Add host-tested stereo/world/hand/compositor timing telemetry and cache
  per-context presentation GL capability discovery for the graphics baseline
- [ ] Implement a reliable desktop monitor mirror
- [ ] Headset-validate medium-distance lamp lighting and scissor-hook performance
- [ ] Achieve acceptable frame pacing at the headset's target refresh rate
- [x] Implement default-off positional tracking from the reconciled game/body boundary
- [ ] Headset-validate positional tracking with game/body calibration
- [x] Live-validate direct VR startup and menu/gameplay transitions (PID 22096)

**Current milestone note:** native stereo/head rotation, the player/body boundary,
the default-off reconciliation shadow path and the bounded collision-aware
physical X/Z displacement request in metres are now live-tested. PID 26144
demonstrated queue, injection, native collision and matched reconciliation at
`0xD7281` while positional HMD translation remained zero. PID 21548 then passed
the complete active room-scale helper after the combined-tick carry correction;
in-place head tilt no longer caused locomotion, but continuous world shake still
failed comfort. Horizontal render placement was moved to the reconciled anchor
plus the newest HMD delta between body ticks. PID 13672 headset-exercised that
correction and the user reported the continuous shake gone with a much improved
feel. PID 11804 then headset-exercised the tracking-only heading correction: the
requested direction was correct and general stability remained good, but speed
still varied because the remapped vector inherited native forward/back/side
acceleration from a differently oriented hidden body. Rework's direct
`1.5/2.25 m/s` displacement is now implemented for the transient room-scale
gate through the existing `0xD7281` owner. Physical and stick requests share one
bounded `0.05 m` tick and their accepted components are partitioned before
anchor reconciliation. PID 17612 showed that the first publication gate blocked
the route completely even though full left-stick analog reached the runtime:
zeroing the VR native axis made `MoveForward/MoveSideways` return before their
later `+0x264` write. The bridge now evaluates the exact pre-Move state predicate
instead and mirrors `+0x264` only after a direct request is queued. PID 28996
then exposed incorrect indexed-state field mapping in that predicate; the exact
`+0x2BC/+0x2C4` and `+0x2D0/+0x2D8` vector/index ownership is now pinned by the
verifier. PID 8092 headset-validated the corrected path: direct locomotion was
observed, all four physical outcomes passed and queue/consume/inject/match was
`201/201/201/201`. The user reports correct stick locomotion and substantially
improved overall feel. This is headset evidence for the technical
stick/collision route, while positional comfort remains open: PID 20520 later
reported a pullback sensation during short physical X/Z movement.
PID 20520 also exposed that the first physical-crouch integration could enter
but did not reliably synchronize native standing on exit; its aggregate helper
result was a false pass. The first correction moved Rework's button latch and
desired posture into shared policy, drove Black Plague's existing crouch
pressed/released dispatches from the game-thread owner and added continuous
tracked Y from the reconciled feet anchor. That iteration was host-tested before
the later headset runs below exposed the remaining native ownership problem.
PID 23260 then proved that physical/button policy itself was working but exposed
one more backend-specific distinction: the exact native crouch entries are
pressed/released dispatches and preserve the game's hold/toggle setting. PID
24948 showed that compensating inside those callbacks was still the wrong
ownership boundary: the body repeatedly changed between `1.65/0.95 m`, but the
game did not hold its real crouch/stealth state. Exact-build decoding now pins
`cPlayer::ChangeMoveState` at `0x9C750`; the original crouch handlers prove
state `4` is crouch and state `0` is walk. The current build applies the shared
Rework desired state directly through that native transition and the
focused helper now requires collider shape and move-state to agree, including a
stable button-only crouch and Hybrid latch. PID 22096 then sustained the current
presentation path through gameplay with zero stereo failures and exposed a more
specific short-X/Z comfort boundary: render-rate prediction could continue into
the direction rejected by the previous physical solve before the next body tick
snapped back. The filter removes only that rejected prediction component,
preserving tangential slide and motion away from the wall. PID 25484 then
supplied focused headset evidence: 12,858 presentation frames with zero stereo
failures, 2,477 meaningful body samples (`2407 free / 43 blocked / 27 partial`),
physical crouch entries/exits `3/3`, final native standing state, `0.961 m`
tracked-Y range and 650 accepted direct-locomotion samples. The user reported
that the session felt good. The automatic focused gate remains incomplete only
where the log failed to capture the Hybrid release-hold interval after the
combined physical+latch state; blocked-stand/low-ceiling and deliberate
wall/slide edges remain separate validation work. Tracking-world-yaw turn
ownership remains separate.
The current 2026-09-17 host candidate goes one step further for the reported
wall-contact pullback/mini-jump symptom: horizontal rendering no longer advances
with any raw HMD X/Z between native body ticks and instead presents only the
reconciled head anchor, matching Rework's ownership more closely. This replaces
the current-code use of the headset-tested rejected-direction prediction filter,
so the new placement needs fresh headset evidence. The same candidate also adds
the Rework-derived direct hand nudge path and the secondary-color GL-state fix;
both are host-tested only.
The desktop mirror remains experimental. PID 19192 confirmed that mirror-off
shows 2D menus but suppresses the gameplay world to black, matching the pass
ownership design; mirror-on remains part of the next validation batch.

Exit criterion: stable in-headset stereo rendering and head tracking in representative gameplay and menus, with frame pacing and positional tracking explicitly validated before claiming a complete visual MVP.

## Phase 3 — Shared runtime extraction

Cross-game completion is governed by `docs/TRILOGY_PARITY_PLAN.md`. A checked
extraction item means reusable policy/data exists and has the stated tests; it
does **not** mean Black Plague parity unless the Black Plague backend actually
consumes that behavior or has a documented incompatible equivalent.

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
  - [x] Extract and host-test Rework's item-only magnetic targeting policy (range/bias profiles, cone/scoring, ranked visibility sampling) while keeping entity classification and physics queries game-owned
  - [x] Extract and host-test Rework's free/slider/hinge servo math and velocity caps while keeping joint selection and per-game mechanism profiles/adapters game-owned
  - [ ] Map Black Plague item classification/visibility and one representative native jointed mechanism before consuming those shared policies there
  - [ ] Keep dimmer, staged loading/fade and game UI lifecycle product-owned until a second backend exposes a compatible ownership boundary
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

Exit criterion: the shared runtime contains the proven, game-neutral behavior required by the current integrations while each game-specific body/render/input boundary remains explicit. Every extracted Rework capability is also classified in the trilogy parity ledger as consumed, target-specific, not applicable or still open; extraction alone is never used to claim target parity.

## Phase 4 — Black Plague gameplay VR

- [x] Connect OpenVR action polling to the imported manifest and bindings
- [x] Code-test native intents and tracked menus
- [x] Import and host-test the Rework right/left hand rigs in the Framework renderer, preserving the richer shared five-finger articulation and depth-tested resolved-palm placement
- [ ] Headset-validate the supported controller profiles on Black Plague and close any per-profile feature gaps before controller parity is claimed
- [x] Integrate/code-test free-body palm-relative grab, release and bounded throw
- [x] Limit the generic prop-pick fallback to Rework's `0.18 m` physical reach
- [x] Port Rework `23c890f` collision-stopped-hand acquisition semantics: visible/held palms stay resolved while target intent follows the raw controller by at most `0.18 m`
- [ ] Complete palm collision, jointed mechanisms and definitive tool/light geometry/profile validation
  - [x] Pin the supported BP `CheckShapeWorldCollision` ABI, callback/contact
    layout, shape accessors and reference-counted destruction; host-test a
    default-off no-write query on the existing body-update owner
  - [x] Live-test the no-write query in PID 28412 without native-memory changes
  - [x] Implement and host-test backend-owned palm-shape lifecycle plus the
    Rework sweep/refinement/recovery resolver behind a default-off gate
  - [x] Live-test `--validate-palm-resolver` (PID 8644: create/reuse/query/
    destroy balanced, `user_count=0`, gameplay memory unchanged)
  - [x] Pin and host-test Black Plague's independent character/`skip_body`
    filters at `0xD4830`; `collideCharacter=false` rejects all character bodies
    while `skip_body` rejects exactly one body, matching Rework's query contract
  - [x] Publish the owning held body per hand and wire resolved palms into
    tracked gameplay hands/interaction while keeping aim on raw tracking
  - [x] Host-test distinct Black Plague interaction ownership: `Grab=6` keeps
    rigid palm-relative free-body placement; free-body `Move=2` preserves the
    picked contact point and follows the palm with the Rework-derived force path;
    jointed/mechanism bodies remain native
  - [x] Replace the provisional procedural hand presentation with generated
    renderer data from the proven Rework DAE rigs and diffuse material; keep
    `runtime::ArticulateVrHand` as pose authority and host-test real GL drawing
    plus caller-state/depth restoration. PID 26940 exposed inherited HPL VBO/
    client-color-array state corrupting the indexed hand draw; the renderer now
    isolates that shared GL client state and a real-driver regression proves both
    texture coloration and visible per-finger articulation under hostile state.
  - [ ] Headset-validate palm contact plus `Grab=6` / `Move=2` placement after a
    clean reboot and compare palm-collision on/off frame pacing; PID 23000 is
    inconclusive because the run also had severe FPS loss and a controller drop;
    PID 4720 also does not count because the old focused helper launched palms
    with room-scale/physical displacement disabled. The helper now composes the
    palm gate with the validated room-scale/crouch stack and checks that state;
    PID 26940 reached the combined path but exposed black/rainbow hand rendering
    and repeated vertical mini-jumps under physical wall pressure. The next run
    must verify the host-tested GL-state fix, five independent finger channels,
    stable physical wall contact with `physical_step_suppressed`, normal stick
    stairs/ledges, representative free props/mechanisms and frame pacing before
    either corrected path is promoted beyond host-tested.
- [x] Statically map and host-test telemetry for the exact-build player/character-body/native-shape/movement/collision path
- [x] Live-validate the mapped body, active shape, physics timestep and requested/accepted displacement telemetry
- [x] Live-characterize sprint, crouch shape ownership and native jump/vertical ownership
- [x] Live-test the narrow `BlackPlagueBodyAdapter` and shared accepted-displacement observation
- [x] Connect and host-test shared tracking/body reconciliation through the adapter as a default-off shadow path while positional HMD translation remains gated
- [x] Live-validate the shadow-only reconciliation path with translation still zero through the exact-build/mutex validation route
- [x] Implement and host-test a bounded collision-aware physical displacement request separately from native analog movement
- [x] Live-validate that physical request through stationary/free/block/slide cases before enabling room-scale
- [x] Implement a transient, fail-closed room-scale consumer through that reconciled request
- [x] Revalidate corrected render-rate positional movement, stationary comfort, blocking/sliding and head/body reconciliation in the headset (PID 8092 technical gate)
- [ ] Headset-validate the PID 26940 physical-only step-climb suppression against gentle wall pressure while confirming direct/native locomotion retains normal stair/ledge stepping
- [ ] Decide and validate Black Plague VR walk/sprint tuning after reconciliation is stable
- [ ] Complete the active player-camera/head-bob/footstep-bob ownership map needed for comfort work
- [x] Implement Rework-derived physical crouch policy and explicit desired/native stance synchronization through Black Plague's existing input owner
- [x] Headset-validate continuous tracked-Y correlation and focused short-range X/Z comfort (PID 25484)
- [ ] Capture the remaining Hybrid release-hold interval and blocked-stand/low-ceiling crouch recovery
- [ ] Exercise deliberate wall/slide edge cases for the current rejected-direction prediction filter
- [ ] Validate long-body interaction and mechanism-specific states
- [ ] Consume the shared Rework magnetic-item policy through a mapped Black Plague item classifier/visibility boundary
- [ ] Consume/adapt the shared slider/hinge mechanism policy through mapped representative Black Plague native joint/state/update boundaries
- [ ] Inventory, notes, menus, HUD and subtitles
- [ ] Add the optional Framework-native radial/quick-access UX only after the current headset/body/interaction validation gates are closed
  - [ ] Keep radial lifecycle, stick dead-zone/sector selection, handedness, cancellation and selection haptics in shared runtime code
  - [ ] Keep available actions/items and activation behind narrow per-game adapters; do not encode HPL layouts, RVAs or product inventory semantics in the shared radial policy
  - [ ] Preserve the original inventory/menu path as a fallback rather than replacing it
  - [ ] Consume the same shared radial behavior in Overture and Black Plague, then carry it into Requiem instead of creating per-game implementations
- [x] Persist the complete shared VR settings schema and host-test the Rework-derived editor policy
- [x] Define the Black Plague settings capability map from backend-wired behavior
- [x] Provide an offline Black Plague VR settings editor through `PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`
- [ ] Integrate a dedicated Black Plague VR settings page after a safe native-menu insertion boundary is demonstrated
- [x] Consume shared haptic policy for Black Plague pickup/drop, tracked UI select and successful direct-hand contact; host-test focus/pose/cooldown rejection plus per-event OpenVR submission telemetry
- [x] Extend the combined palm/room-scale headset helper to aggregate `Grab=6`/`Move=2`, tool, crouch, nudge and haptic evidence without treating optional scene coverage as a false failure
- [ ] Complete Black Plague haptic parity: map safe light-toggle/melee/damage boundaries and headset-validate the exercised controller submissions
- [ ] Map a safe Black Plague audio boundary and consume/adapt the shared HRTF, occlusion and environmental-audio behavior
- [ ] Map compatible Black Plague renderer stages for the demonstrated Rework enhanced-visual behavior, or document evidence-backed target incompatibilities
- [ ] Implement/validate comfortable loading/map transitions and VR UI dimmer/overlay behavior at Black Plague-owned lifecycle boundaries
- [ ] Representative chapter-level validation

Exit criterion: a documented playable alpha for an exact Black Plague build with physical interaction, body/room-scale movement and representative headset validation **and** the Black Plague framework-readiness gate in `docs/TRILOGY_PARITY_PLAN.md` closed. Shared-but-unconsumed Rework policy is an open parity item, not completed porting.

## Phase 5 — Requiem backend

- [ ] Begin gameplay implementation only after the Black Plague framework-readiness gate is closed; non-invasive exact-build reconnaissance may happen earlier without displacing the active parity milestone
- [ ] Repeat exact-build research rather than assuming Black Plague RVAs
- [ ] Reuse validated HPL-level hooks where binary evidence permits
- [ ] Consume every applicable shared capability family from the trilogy parity ledger through narrow Requiem adapters/profiles
- [ ] Adapt Requiem-specific gameplay, interaction, presentation and audio behavior only at demonstrated target boundaries
- [ ] Consume the shared VR-native radial/quick-access policy through Requiem-specific action/inventory adapters after the common implementation is validated on the earlier backends
- [ ] Reuse the shared controller-profile parity matrix and validate Requiem-specific native intent routing without regressing any supported controller profile
- [ ] Complete stereo, input and representative-level validation

Exit criterion: a documented playable alpha for an exact Requiem build with its own completed parity ledger/evidence, while Overture and Black Plague regression gates remain green.

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
