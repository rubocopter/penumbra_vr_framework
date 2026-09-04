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

The future runtime may own:

- OpenVR lifecycle and compositor access
- tracked poses and coordinate conversion
- action manifests, controller input and haptics
- shared settings and structured logging
- renderer-neutral eye/view data
- host-independent tests

It must not own game RVAs, binary signatures, HPL object layouts or assumptions about a specific player class. Those belong to a backend and, where appropriate, to an exact build manifest.

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

The first backend-owned code now lives under `src/backends/black_plague`. It contains only the exact-build `RenderWorld` call-site validator; it is not yet a general backend interface or a claim of playable support.

## Build identity

Filename detection is insufficient because both Overture and Black Plague commonly use `Penumbra.exe`. Each binary backend must select a manifest using a cryptographic executable hash. A manifest will eventually contain:

- game and release/channel identity
- executable architecture and SHA-256
- preferred module-relative RVAs
- independently verifiable fallback signatures
- calling conventions and structure layouts
- provenance and validation notes

Unknown hashes must never receive hooks intended for a known build. Unsupported builds should produce a diagnostic report suitable for adding support later.

## Bootstrap direction

The preferred direction is one small, dependable bootstrap installed beside each supported binary game. It identifies the host, validates its build, loads exactly one backend, and otherwise exits without modifying the process.

Whether the bootstrap is loaded by a launcher, an SDL proxy, or another mechanism remains an open decision until a minimal injection experiment has been tested. Multiple simultaneous SDL/OpenAL/OpenGL proxy layers are not a design goal.

## Rendering milestone

The first architectural proof is not a set of compiling classes. It is Black Plague rendering a stable stereo scene to OpenVR with head tracking while retaining keyboard and mouse controls. Required research begins with:

1. process entry and module layout
2. main frame/swap boundary
3. active camera and projection construction
4. scene render entry
5. HUD/UI draw ordering
6. safe per-eye render-target handling

Only after that milestone should interaction and installer APIs be generalized.

The demonstrated Overture implementation is used as a behavioral and testing
reference rather than copied as a monolithic game layer. The component-by-
component boundary and extraction order are tracked in
[`docs/REWORK_PORTING_PLAN.md`](docs/REWORK_PORTING_PLAN.md).

## Installer responsibilities

The eventual installer will:

- discover Steam and manually selected installations
- distinguish games and exact executable builds
- show supported, unknown and already-modded states
- back up every replaced file transactionally
- deploy only the files needed by each integration model
- preserve saves and configuration
- support verification, repair and rollback
- never patch an unknown executable silently
