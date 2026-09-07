# Architecture

## Product boundary

Penumbra VR is one distribution, not necessarily one executable or DLL. The installer presents a single product and deploys the appropriate integration for every detected game.

The architecture deliberately separates three concerns:

1. **Runtime:** game-independent VR behavior.
2. **Host adapter/backend:** access to a particular game's renderer, player, UI and interactions.
3. **Deployment:** installation, executable validation, backup, launch and rollback.

## Integration matrix

| Game | Integration model | Initial deployment direction |
|---|---|---|
| Overture | Released source code plus HPL1 | Rebuilt executable using the shared runtime API |
| Black Plague | Closed game layer on HPL1 | Bootstrap DLL plus exact-build binary backend |
| Requiem | Closed game layer on HPL1 | Bootstrap DLL plus exact-build binary backend |

The public experience is unified even though these implementations are intentionally asymmetric.

## Runtime boundary

The game-independent runtime owns, or is intended to own:

- OpenVR lifecycle and compositor access
- tracked poses and coordinate conversion
- action manifests, controller input and haptics
- shared settings and structured logging
- renderer-neutral eye/view data
- host-independent tests

The current implementation already provides the OpenVR session, render-target
and stereo-pass policies, tracking mathematics and host-independent reference
functions for the accepted visual and spatial-audio tuning. The imported OpenVR
action manifest and controller bindings are joined by a device-independent
logical input router covering dead zones, context/handedness edge latching,
pose-loss releases and transient action-idle grace. Real OpenVR polling and
exact-build native intent consumption are connected, along with tracked menus,
procedural gloves and a free-body grab/throw adapter. These paths are code-tested,
not yet headset-validated. A runtime-owned settings model now
centralizes the Rework defaults, ranges, enum values and migration behavior;
the launcher persists the Black Plague monitor-mirror preference, while full
configuration storage and application remain adapter/backend responsibilities.
The runtime now also owns Rework's complete tracking-space boundary and its
room-scale/locomotion constants: world scale 1.0, calibrated height composition,
world yaw, 0.05 m physical body steps, collision rejection and the 1.5/2.25 m/s
walk/run policy.

The runtime must not own game RVAs, binary signatures, HPL object layouts or
assumptions about a specific player class. Those belong to a backend and, where
appropriate, to an exact build manifest.

## Shared HPL1 adapter boundary

Black Plague and Requiem use closely related HPL1 layouts, while Overture is
available as source. Reusable engine behavior therefore lives in a narrow HPL1
adapter layer between the runtime and the exact-build backends. For example,
`src/adapters/hpl1/camera_matrix_override.*` implements the byte-exact camera
transaction, while the Black Plague backend supplies the validated offsets.

This layer may share renderer, light, material, physics and sound behavior only
after it has been demonstrated in at least one real integration. It must not
turn a coincidental RVA or game-specific player state into a supposed engine
contract.

## Backend responsibilities

A backend translates runtime concepts into one game's implementation:

- stable frame boundary
- view and projection control
- per-eye render entry and render-target ownership
- player body and camera relationship
- interaction, hands and held objects
- game input and haptics
- HUD, menus, inventory, notes and subtitles
- positional audio listener when required

The backend API will be frozen only after the Black Plague proof of concept reveals the real data and lifecycle requirements. Defining a large speculative interface first would merely encode guesses.

Backend-owned code now lives under both `src/backends/overture` and
`src/backends/black_plague`. `pvr_overture_backend` is the first source-game
backend core: it consumes the common tracking, settings and input types, runs
the Rework-equivalent player/body sequence once per player update, and exposes
only collision/body/jump operations through a narrow HPL adapter. Its behavior
is host-tested, but the adapter is not yet linked into the Rework executable or
validated in the headset.

The Black Plague backend validates and intercepts the exact-build `RenderWorld`
call site, performs reversible
per-eye camera overrides, renders to shared eye targets and can submit a
continuous rotation-tracked stereo stream. This remains a research backend: the
continuous path has completed a two-minute full-resolution runtime session and
preserved keyboard/mouse input, but the user observed HMD-relative geometry
popping consistent with a render list prepared before the per-eye camera
overrides. The new conservative HMD-aware update has removed those observed
artifacts in a follow-up headset run. Frame-pacing work remains open; positional
tracking and a production installer are still absent. Native controller intents,
tracked UI panels, provisional hands and free-body interaction are implemented;
palm collision and articulated mechanisms remain pending. The controller pick
fallback is now limited to Rework's 0.18 m collision-to-raw-palm reach; the
separate magnetic inventory-item path still requires a safe entity classifier.
Continuous stereo now defaults to two world passes, with game time advanced on
the first eye; an optional mirror retains a third desktop pass. This Rework-derived
schedule has host coverage and live pass-count telemetry; comprehensive visual
and frame-pacing validation remains open.

## Build identity

Filename detection is insufficient because both Overture and Black Plague commonly use `Penumbra.exe`. Each binary backend must select a manifest using a cryptographic executable hash. The current evidence manifests contain, and any future production manifest must preserve:

- game and release/channel identity
- executable architecture and SHA-256
- preferred module-relative RVAs
- independently verifiable fallback signatures
- calling conventions and structure layouts
- provenance and validation notes

Unknown hashes must never receive hooks intended for a known build. Unsupported builds should produce a diagnostic report suitable for adding support later.

## Bootstrap direction

The preferred direction is one small, dependable bootstrap installed beside each supported binary game. It identifies the host, validates its build, loads exactly one backend, and otherwise exits without modifying the process.

The current research launcher injects a version-gated probe into an already
running Steam process. `--launch-vr` also starts the exact game through Steam,
waits for initialization and enables continuous VR with the saved mirror setting.
The full startup sequence still needs live validation. This is not a production
installer/bootstrap. Whether the product bootstrap is loaded by a launcher, an
SDL proxy, or another mechanism remains an open decision. Multiple simultaneous
SDL/OpenAL/OpenGL proxy layers are not a design goal.

## Validation milestone

The original architectural proof was Black Plague rendering a stable stereo
scene to OpenVR with head tracking while retaining keyboard and mouse controls.
The next framework proof is behavioral equivalence: the source Overture adapter
must run the common backend in-game, after which the same tracking and movement
policies can be validated against both Overture and Black Plague.

The completed Black Plague render-path research established:

1. process entry and module layout
2. main frame/swap boundary
3. active camera and projection construction
4. scene render entry
5. HUD/UI draw ordering
6. safe per-eye render-target handling

Those boundaries remain backend-specific; interaction and installer APIs are
generalized only after equivalent behavior is demonstrated in a real host.

The demonstrated Overture implementation remains the behavioral reference and
is not copied as a monolithic game layer. The component-by-component boundary
is tracked in [`docs/REWORK_PORTING_PLAN.md`](docs/REWORK_PORTING_PLAN.md), and
the exact comparison behind the first backend is recorded in
[`docs/OVERTURE_BACKEND_MIGRATION.md`](docs/OVERTURE_BACKEND_MIGRATION.md).

## Installer responsibilities

The eventual installer will:

- discover Steam and manually selected installations
- distinguish games and exact executable builds
- show supported, unknown and already-modded states
- back up every replaced file transactionally
- enable Large Address Aware only for allowlisted x86 PE32 executables, after a
  backup, and restore the original bytes during rollback
- deploy only the files needed by each integration model
- preserve saves and configuration
- support verification, repair and rollback
- never patch an unknown executable silently

The transaction and state model are specified in
[`docs/INSTALLER_DESIGN.md`](docs/INSTALLER_DESIGN.md). The repository currently
contains only the tested in-memory LAA byte transformation; it does not yet
modify installed game files.
