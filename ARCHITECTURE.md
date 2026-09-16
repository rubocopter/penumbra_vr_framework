# Architecture

## Product boundary

Penumbra VR Framework is one user-facing project, not necessarily one executable or one injection mechanism. The eventual product should detect a supported Penumbra game/build and deploy the appropriate integration while exposing a coherent VR experience.

The architecture separates four concerns:

1. **Shared runtime** — game-neutral VR units, transforms, policies and reusable algorithms.
2. **Host adapter/backend** — one game's renderer, player, physics, input, interaction and native state boundaries.
3. **Profiles/data** — controller bindings, tool/model sockets, bone mappings and demonstrated build capabilities.
4. **Deployment** — build identification, installation, launch, backup, verification and rollback.

The three games intentionally use asymmetric integrations where the evidence requires it.

## Integration matrix

| Game | Integration model | Current state |
| --- | --- | --- |
| Overture | Released source + HPL1 | Framework-owned source product; initial headset validation completed |
| Black Plague | Closed game layer on HPL1 | Exact-build x86 launcher/probe/backend; active gameplay and comfort validation |
| Requiem | Closed game layer on HPL1 | Catalog/deployment preparation only; functional backend not yet demonstrated |

The goal is shared behavior plus narrow mechanisms, not three independent VR implementations and not a speculative universal engine API.

## Behavioral reference and evidence model

Overture Rework revision `23c890f7dbd06b939be9951d282e6e948d9a6623` remains the behavioral reference for already-demonstrated Overture VR behavior. It is not a build dependency.

Validation states are architectural metadata and must be kept distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A build or synthetic test does not imply in-game behavior. A live binary boundary does not imply headset comfort. A successful headset run for one contract does not promote unrelated contracts.

The historical Astra audit and the current architectural constraints are documented in:

- [`docs/audits/ASTRA_HIGH_AUDIT.md`](docs/audits/ASTRA_HIGH_AUDIT.md)
- [`docs/DESIGN_DECISIONS.md`](docs/DESIGN_DECISIONS.md)
- [`ROADMAP.md`](ROADMAP.md)
- [`docs/internal/CODEX_HANDOFF.md`](docs/internal/CODEX_HANDOFF.md)

## Shared runtime boundary

The runtime owns or is intended to own behavior that has demonstrated game-neutral semantics, including:

- OpenVR session/runtime abstractions;
- tracked-pose types and coordinate conversion;
- tracking space, calibration, recenter and identified pose/yaw epochs;
- action state, controller input and haptic policy;
- locomotion speeds and neutral displacement policy;
- tracking/body reconciliation mathematics;
- accepted-body-motion representation;
- crouch desired-state policy and shared play-mode policy;
- reusable grab-pose and hand-contact mathematics;
- shared settings schemas and validation-friendly telemetry types;
- renderer-neutral eye/view and render-target policy;
- host-independent tests.

The runtime does **not** own:

- game RVAs or byte signatures;
- exact HPL object layouts;
- binary calling conventions;
- one game's player/state enum values;
- native physics/update ownership;
- game-specific entity classification;
- per-model tool sockets or bone axes.

Promote behavior to the runtime only when the semantics are demonstrated. Duplicated but validated game/source code is preferable to a false abstraction.

## Adapter/backend boundary

A backend translates shared policy into one game's actual engine phases and objects. Typical backend responsibilities include:

- exact build identification;
- renderer and visibility boundaries;
- player/body/camera acquisition;
- native collision and shape queries;
- move-state application and native state classification;
- hook ownership and teardown;
- interaction entity classification;
- HUD/menu/inventory integration;
- per-game audio or presentation details;
- exact-build ABI, layouts and RVAs.

The shared HPL ancestry of the trilogy is useful evidence, but a source declaration or one game's RVA is never automatically another game's ABI.

## Overture architecture

Overture is a source-integrated product:

```text
Framework-owned Penumbra/HPL1 source product
    ↓
Game.cpp acquisition/update
    ↓
ButtonHandler / Player
    ↓
OvertureSourceIntegration
    ↓
OvertureBackend
    ↓
shared runtime policy
    ↓
HPL/source adapters
    ↓
source renderer, player and physics
```

The autonomous product lives under `products/overture`. The Framework owns the coherent source host and packaging path; the separate Rework repository is used for behavioral comparison, not compilation.

The Overture product is also a regression consumer for shared extraction. Existing proven `Game.cpp` sequencing and `PlayerState_Interact_VR.cpp` behavior should not be rewritten without a demonstrated contract gap.

## Black Plague architecture

Black Plague is an exact-build binary integration:

```text
ProbeLauncher
    ↓
Steam launch / process discovery
    ↓
exact executable validation
    ↓
remote DLL load
    ↓
PenumbraVR_Initialize
    ↓
ordered component owners
    ↓
NativeInputBridge + BodyCollisionProbe
    ↓
BlackPlagueBodyAdapter / interaction / renderer
    ↓
shared runtime policy
```

Unknown builds fail closed. Exact-build evidence lives in manifests, verifiers and backend constants with provenance; it is not promoted into generic HPL contracts.

### Singular hook ownership

A native callsite has one owner. Multiple consumers receive verified fan-out/status rather than stacking independent hooks.

Important Black Plague owners include:

- `NativeInputBridge` — native input queries and movement callsites;
- `D460A -> D6E00` — the single native character-body update;
- `D7281` — bounded X/Z request injection before native horizontal resolution;
- the mapped visibility/render callsites — presentation/culling ownership;
- the SDL frame boundary — outer frame/presentation integration.

`BlackPlagueBodyAdapter` consumes those owners and does not install a second native body-update hook.

## Black Plague body/tracking transaction

The Astra audit identified a temporal defect in the former post-tick plan-for-next-tick path. The current host-tested contract is a same-tick transaction around the existing native body owner:

```text
before D6E00
    resolve player/body identity and B0
    consume latest identified tracking sample
    consume logical locomotion intent
    integrate using the physics tick
    bind request to tick/body/generation/epoch

inside existing D7281 owner
    consume the bounded horizontal request once
    preserve native Y/solver ownership

D6E00
    run exactly once

after D6E00
    observe B1
    verify tick/body/generation correspondence
    reconcile once
    carry accepted locomotion once
    publish body/anchor result
```

The previous plan against the `body_before` of an already-finished tick is not the current architecture.

The combined Black Plague boundary resolves physical and direct-stick horizontal requests through one native update. The total accepted body displacement is observed; any attribution between physical/stick components remains an adaptation and must not be described as two independent native solves.

Native jump, gravity and vertical gameplay remain outside this horizontal ownership contract.

### Locomotion policy

Shared runtime policy currently carries:

- normal walk: `1.5 m/s`;
- sprint: `2.25 m/s`;
- demonstrated constrained movement: `0.5 m/s`;
- bounded per-tick horizontal request policy, distinct from the game's native step-height semantics.

The normal direct route has headset evidence from the existing validation history. Black Plague's exact Push/Move state mapping to `0.5 m/s` is a later host-tested mapping and retains its own headset gate.

## Crouch and posture ownership

The current contract follows Rework's persistent desired-state model:

- shared policy owns desired crouch/latch semantics;
- Black Plague preserves native crouch query results when VR is not the owner;
- VR stance ownership is tied to session/player generation;
- Black Plague applies native stance through `cPlayer::ChangeMoveState(4/0)` on the existing game-thread owner;
- desired stance, effective move state and blocked stand are represented separately;
- standing remains pending/retryable when native clearance prevents the transition.

Ordinary physical/button/Hybrid crouch has headset evidence in the later Black Plague runs. Low-ceiling blocked-stand recovery and explicit tracked-Y/collider correlation remain separate headset gates.

Shared seated/play-mode policy has been extracted; per-game native application remains adapter-owned.

## Presentation and tracking epochs

The audit identified that visibility, eye rendering, controllers and body placement could consume different pose/yaw epochs. The post-audit architecture therefore carries identified presentation/tracking samples and explicit yaw epochs.

Real headset candidates then exposed two compositor-ownership bugs:

1. nested visibility callbacks during eye rendering could reacquire compositor poses;
2. a valid presentation snapshot could be reused after its eye pair had already been submitted.

The current presentation contract is **single-consumption**:

- the owning outer presentation phase acquires/publishes the sample;
- nested eye/visibility consumers reuse that owned sample rather than calling for another compositor frame;
- sequence `0`, already-submitted sequences and older sequences are rejected before another eye submit;
- a skipped world submit is preferable to submitting an eye pair twice for one compositor sequence.

PID 22096 subsequently sustained the current presentation path through menu/gameplay with no stereo/compositor failures. That closes the specific error-108 regression but does not by itself validate tracking-world-yaw ownership, mirror/focus or final body/footstep bob.

## Positional comfort handoff

Short physical X/Z pullback remained after the earlier body-boundary work. Later telemetry narrowed one remaining path to render-rate prediction continuing briefly into a horizontal direction rejected by the previous native solve.

The current host-tested render handoff carries the last physical reconciliation and removes only the prediction component that continues into the rejected direction. Tangential slide and movement away from the obstacle remain available.

This filter is not considered headset-validated until a fresh short-motion/wall/slide run closes the comfort gate.

## Interaction architecture

Interaction is split into three distinct layers:

1. **selection/acquisition lifecycle** — input, contact candidate, generation and ownership;
2. **free-body grab** — palm-relative transform/held-body behavior;
3. **collision-resolved palm contact** — world collision, recovery and a resolved palm pose.

Free-body grab is not a substitute for collision-resolved palm contact, and neither is a substitute for jointed mechanism support.

### Generation/lifecycle

Post-audit interaction work makes selection/hold state generation-aware and revalidates acquisition so stale player/world state is not silently reused. Release/restoration must occur at most once and must never dereference a demonstrated-destroyed native body.

### Shared hand-contact policy

Rework's demonstrated palm dimensions, contact tolerances, sweep/refinement and recovery mathematics are extracted into shared runtime policy. Native shape creation, collision queries and body exclusions remain adapter/backend responsibilities.

### Black Plague palm boundary

The exact supported-image Black Plague contact boundary has advanced beyond the original audit:

- `CheckShapeWorldCollision` ABI/callback/contact layout is pinned;
- body shape/matrix accessors are pinned;
- `CreateBoxShape`, user count and destruction route are pinned;
- a default-off `--validate-palm-query` diagnostic reuses the current body shape and guards against native-state mutation;
- its synthetic harness is host-tested.

The diagnostic has not yet established the required successful real-process live gate, no owned palm shape is created by it, and the full Rework sweep/refinement/recovery resolver is not yet connected to Black Plague gameplay.

Therefore the architecture deliberately gates gameplay palm implementation on the no-write live query first.

## Tools, hands and mechanisms

Measured flashlight/glowstick grip points and model orientations are profile data, not global calibration knobs. Black Plague's richer finger articulation is retained and should inform shared semantics without forcing identical rigs.

Doors, levers, sliders and other joints remain native mechanisms until their state/constraint boundaries are mapped. Generic free-body grab must not be presented as mechanism support.

## Probe lifecycle

Initialization and shutdown are transactional at the component level. The probe records installed/active components and can represent a partial state if rollback or teardown fails.

Architectural requirements:

- publish `ready` only for the required installed set;
- preserve real capability/owner state after partial failure;
- keep teardown retryable;
- keep observability/memory valid while owned callbacks may still execute;
- treat remote-call timeout as indeterminate rather than proof of non-execution;
- prepare thread handles/storage before suspending peers and avoid allocations/error formatting inside the suspended region.

Do not collapse a partial state back to “clean” or “ready” for convenience.

## Historical SDL crash

The original SDL APPCRASH remains without a proven causal explanation. Later OpenVR compositor error-108 regressions had separately demonstrated presentation-ownership causes and must not be conflated with that historical crash.

If an SDL crash recurs, the required evidence is a dump with stack, registers and module identity. Temporal proximity to a shadow/mutex/feature is not causal proof.

## Build identity and binary evidence

Filename detection is insufficient because multiple games use similar executable names. Binary backends select supported builds by cryptographic identity and exact evidence.

Evidence records should preserve:

- game/channel/build identity;
- executable architecture and SHA-256;
- RVAs and independently verifiable byte/signature evidence;
- calling conventions and object layouts;
- whether the inspected image was pristine or already contained known Framework hooks;
- provenance and validation status.

Unknown or contradictory images fail closed.

## Deployment direction

The final product should provide one safe user-facing installer/bootstrap while retaining the asymmetric game integrations.

Deployment responsibilities include:

- Steam/manual installation discovery;
- build compatibility reporting;
- transactional backup/install/verify/rollback;
- allowlisted Large Address Aware transformation where required;
- action manifest/binding deployment;
- optional localization payload deployment with provenance;
- clean-machine and upgrade testing.

The current research launcher/probe path is not yet the final production installer.

## Current implementation boundary

Do not interpret this architecture document as a request to redesign already-tested contracts. The active order is maintained in [`ROADMAP.md`](ROADMAP.md) and the current engineering checkpoint in [`docs/internal/CODEX_HANDOFF.md`](docs/internal/CODEX_HANDOFF.md).

At the current checkpoint, the principal remaining evidence/implementation boundaries are:

1. close the remaining focused Black Plague posture edges after PID 25484: Hybrid release-hold capture and blocked-stand/low-ceiling recovery;
2. run the real-process no-write Black Plague palm query, then add owned palm shapes and the proven Rework resolver if that gate passes;
3. finish deliberate wall/slide, constrained locomotion, recenter/tracking-loss and yaw/bob comfort gates;
4. interaction lifecycle/tool geometry validation;
5. mirror/focus as a separate presentation gate;
6. mechanisms, broader UI/gameplay coverage and production deployment;
7. Requiem exact-build backend research.
