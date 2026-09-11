# Architecture

## Product boundary

Penumbra VR is one user-facing product, not necessarily one executable or DLL. The eventual installer presents a single product and deploys the appropriate integration for every detected game/build.

The architecture separates three concerns:

1. **Runtime:** game-independent VR behavior and policy.
2. **Host adapter/backend:** access to one game's renderer, player, UI, physics and interactions.
3. **Deployment:** installation, executable validation, backup, launch and rollback.

## Integration matrix

| Game | Integration model | Current direction |
|---|---|---|
| Overture | Released source code plus HPL1 | Framework-owned rebuilt executable using shared runtime policy |
| Black Plague | Closed game layer on HPL1 | Bootstrap/probe DLL plus exact-build binary backend/adapters |
| Requiem | Closed game layer on HPL1 | Future exact-build binary backend |

The public experience is unified even though these implementations are intentionally asymmetric.

## Runtime boundary

The game-independent runtime owns, or is intended to own:

- OpenVR lifecycle and compositor access;
- tracked poses and coordinate conversion;
- action manifests, controller input and haptics;
- shared settings and structured logging;
- renderer-neutral eye/view data;
- tracking space, calibration and recenter policy;
- reusable locomotion/reconciliation policy demonstrated in real integrations;
- game-neutral accepted-body-motion observation;
- host-independent tests.

The runtime must not own:

- game RVAs or signatures;
- exact HPL object layouts;
- player-class assumptions;
- binary calling conventions;
- game-specific entity queries;
- native update ownership.

Those details belong to a backend/adapter and, for binary games, to exact-build evidence.

`runtime::VrAcceptedBodyMotion` is the first body contract consumed by both Overture and Black Plague. It contains only finite before/after body positions plus accepted displacement. It deliberately knows nothing about HPL layouts, movement speeds, solver internals, hook addresses or who owns the native update.

## Shared HPL1 adapter boundary

The games share HPL1 ancestry, but shared behavior is promoted only after evidence supports it. Reusable engine behavior therefore lives in a narrow adapter layer between shared policy and per-game implementation.

Examples:

- `src/adapters/hpl1/camera_matrix_override.*` owns byte-exact camera transaction behavior while the backend supplies validated layout data;
- `src/adapters/overture_source` exposes source-level `cPlayer` / `iCharacterBody` operations to the Overture backend;
- the Black Plague body adapter exposes only the measured exact-build native movement/body boundary required by shared policy.

This layer must not turn a coincidental RVA, field offset or one game's state-machine detail into a supposed engine contract.

## Reuse direction

Overture Rework `23c890f` remains the proven behavioral reference for Overture VR. Existing proven behavior should be adapted rather than reinvented.

Reuse is nevertheless bidirectional. When another backend demonstrates a better game-neutral implementation, the Framework should preserve that stronger behavior and adapt per-game details around it. The current finger-articulation direction is the main example: Black Plague's independent curls/per-joint curves/spread/thumb opposition are the better candidate shared semantics, while Overture's bind poses, bone axes, deadzone/smoothing and handle poses remain rig/profile concerns.

The goal is not three independent VR implementations; it is shared policy plus narrow mechanisms.

## Backend responsibilities

A backend translates runtime concepts into one game's implementation:

- stable frame/update boundaries;
- view and projection control;
- per-eye render entry and render-target ownership;
- player body and camera relationship;
- collision/body access;
- interaction, hands and held objects;
- game input and haptics;
- HUD, menus, inventory, notes and subtitles;
- positional audio listener when required.

The backend API should remain narrow and evidence-driven. A large speculative cross-game interface would encode guesses rather than reuse.

## Overture backend

`pvr_overture_backend` sequences the proven Rework tracking, calibration, room-scale and locomotion policy over a narrow `OvertureBodyAdapter`.

The Framework-owned product host under `products/overture` contains the minimum coherent Penumbra/HPL/OAL source, Win32 dependencies, required resources and package tooling. Rework `23c890f` is not part of the build/package graph.

The exact autonomous Release artifact has completed an initial functional SteamVR/headset/controller pass without an evident Rework regression. Exhaustive feature/hardware coverage remains separate from that completed integration milestone.

## Black Plague backend

The Black Plague backend is exact-build binary integration. Unknown hashes fail closed.

The render path already validates/intercepts the mapped world-render boundary, applies reversible per-eye camera overrides, renders into runtime-sized eye targets and submits continuous rotationally tracked stereo to OpenVR. HMD-aware render-list correction, keyboard/mouse preservation and rotational tracking have been exercised in headset. Frame-pacing, reliable mirror output and positional tracking remain open.

Native controller intents, tracked UI panels, provisional hands and free-body interaction are implemented. Palm collision, articulated mechanisms and definitive tool/light grip profiles remain pending.

### Player/body ownership

The player/body pipeline is now mapped and live-characterized:

```text
cPlayer movement intent
      ↓
iCharacterBody::Move(D4F50)
      ↓
native acceleration / speed state
      ↓
D460A -> iCharacterBody::Update(D6E00) exactly once
      ↓
horizontal collision + native step/gravity/move-state work
      ↓
accepted body displacement
```

The first `BlackPlagueBodyAdapter` is live-tested and follows a strict single-owner model:

- `NativeInputBridge` owns the `MoveForward/MoveSideways` callsites;
- `BodyCollisionProbe` owns the `D460A -> D6E00` callsite;
- `BlackPlagueBodyAdapter` binds through those verified owners and receives fan-out rather than installing competing hooks.

The adapter dynamically re-resolves the current player body, forwards existing horizontal native intent and observes the result after the original native body update. It never invokes `D6E00` directly.

That boundary is live-tested for free movement, total blocking and slide/partial acceptance, including a live body replacement without stale cached identity. This does **not** validate active positional HMD translation, room-scale reconciliation, VR speed tuning, physical crouch, jump tuning or camera/bob comfort.

Native jump/vertical ownership remains separate from shared horizontal intent. Native crouch is also a real body/shape change, but Black Plague's exact stand-clearance mechanism remains insufficiently demonstrated for a shared physical-crouch policy.

## Current framework proof

The previous Black Plague proof was establishing the exact body/collision boundary and a safe adapter without double-updating physics. That proof is complete at `live-tested` level for the supported research build.

The tracking/body policy now has shared stateless planning, physical rejection
and locomotion-carry phases. Overture executes physical requests through its
source adapter; BP only plans them in a default-off shadow consumer:

```text
raw tracking at existing render boundary
      ↓
BP shadow input snapshot (pose + aligned yaw)
      ↓
existing adapter observes the single native tick
      ↓
shared plan/rebase + native accepted-motion anchor carry
      ↓
shadow telemetry only; no camera/body write
```

BP's accepted native movement is not a response to the physical plan: that plan
was never injected. A collision-aware request in metres, separated from native
acceleration and owned by the single tick, remains a missing adapter capability.
Positional translation stays zero; a successful shadow capture alone cannot
justify enabling it. Portable tests pass, but Windows host validation remains
pending. See [the milestone report](docs/internal/TRACKING_BODY_RECONCILIATION.md).

Camera/head-bob/footstep-bob ownership is a separate comfort track. It must not be “fixed” by speculative offsets while the body reconciliation milestone is in progress.

## Exact-build hook ownership

A binary callsite has one owner. If multiple subsystems need data from the same boundary, the owner exposes verified status/fan-out instead of allowing independent hook stacking.

This rule exists because initialized images legitimately differ from pristine executable bytes after the Framework installs earlier hooks. Validation must distinguish:

```text
pristine exact build
        ↓
verified owner installs replacement
        ↓
dependent consumer verifies owner state
```

from an unknown third-party rewrite, which must fail closed.

## Build identity

Filename detection is insufficient because multiple games use `Penumbra.exe`. Each binary backend selects a manifest using a cryptographic executable hash.

Evidence manifests contain, and future production manifests must preserve:

- game and release/channel identity;
- executable architecture and SHA-256;
- module-relative RVAs;
- independently verifiable signatures;
- calling conventions and structure layouts;
- provenance and validation notes.

Unknown hashes must never receive hooks intended for a known build. Unsupported builds should produce a diagnostic report suitable for adding support later.

## Bootstrap direction

The preferred production direction is one small, dependable bootstrap installed beside each supported binary game. It identifies the host, validates its build, loads exactly one backend and otherwise exits without modifying the process.

The current research launcher injects a version-gated probe into an already running Steam process. `--launch-vr` / `Start-Black-Plague-VR.cmd` also provide a one-step development path. This is not yet a production installer/bootstrap.

Whether the final bootstrap is loaded by a launcher, SDL proxy or another mechanism remains an open deployment decision. Multiple overlapping SDL/OpenAL/OpenGL proxy layers are not a design goal.

## Installer responsibilities

The eventual installer will:

- discover Steam and manually selected installations;
- distinguish games and exact executable builds;
- show supported, unknown and already-modded states;
- back up every replaced file transactionally;
- enable Large Address Aware only for allowlisted x86 PE32 executables after backup and restore original bytes during rollback;
- deploy only the files needed by each integration model;
- preserve saves and configuration;
- support verification, repair and rollback;
- never patch an unknown executable silently.

The transaction/state model is specified in [`docs/INSTALLER_DESIGN.md`](docs/INSTALLER_DESIGN.md). The repository currently contains the tested in-memory LAA transformation but does not yet implement the complete production installer.

## Validation model

Use these evidence states consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A compile or synthetic test never implies headset validation. A successful hook installation does not imply comfort or gameplay correctness. A body adapter can be live-tested while room-scale remains unvalidated.

The demonstrated Rework implementation remains the Overture behavioral reference, while detailed Black Plague binary evidence lives in `docs/BLACK_PLAGUE_PROBE.md` and `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`.
