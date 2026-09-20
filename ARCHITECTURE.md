# Architecture

Penumbra VR Framework is one user-facing project with asymmetric integrations.
Shared runtime code owns behavior that is genuinely game-neutral; each game keeps
its renderer, native object access, physics/gameplay ownership and exact-build
binary data behind a backend or product adapter.

## Product boundary

| Game | Integration model | Current role |
| --- | --- | --- |
| Overture | Framework-owned source product built from the imported HPL1/game host | Proven behavioral reference and first shared-runtime consumer |
| Black Plague | Exact-build binary backend loaded through a managed bootstrap | Active second-backend integration and portability proof |
| Requiem | Exact-build metadata/research only | Planned third consumer after the Black Plague readiness gate |

Rework `23c890f` is the proven Overture baseline. It is reference evidence, not a
runtime or build dependency of the Framework-owned Overture product.

## Layers and ownership

### Shared runtime

Shared runtime owns reusable policy and data contracts:

- OpenVR session/pose data and renderer-neutral eye information;
- tracking-space transforms, world yaw/recenter and play-mode semantics;
- logical input state, action mapping and haptic policy;
- settings types, limits, persistence semantics and editor policy;
- metric locomotion and accepted-body-motion policy;
- hand pose conditioning, grab/contact mathematics and reusable interaction
  policy;
- stereo/render-target policy and visual calibration that does not depend on a
  specific HPL renderer layout;
- reusable spatial-audio policy such as HRTF configuration and environmental
  parameter data.

Shared runtime does not contain game RVAs, binary signatures, native player/body
layouts or model-specific grip/socket values.

### Backends/adapters

A backend owns the narrow native boundary required by its game:

- renderer/frame entry points and camera/native object access;
- character body, collision and native movement state;
- entity enumeration, object ownership and mechanism lifecycle;
- native input/action calls and UI state;
- exact-build hooks, signatures, calling conventions and structure fields;
- game-specific audio/render integration.

One exact callsite has one owner. Other consumers bind through explicit status or
fan-out instead of stacking independent hooks over the same instruction.

### Profiles/data

Profiles and source-controlled assets own values that are demonstrated but not
universal policy: controller bindings, rig/bone mappings, model/tool sockets,
localization, exact-build manifests, deployment payloads and recommended
configuration data.

## Evidence model

Implementation state is deliberately distinct from validation state:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A compile, unit test or exact-image verifier does not imply live or headset
validation. Evidence is scoped to the game/build/capability that produced it.
`docs/SUPPORTED_BUILDS.md` records executable identity and support level;
`docs/TRILOGY_PARITY_PLAN.md` records capability consumption.

Exact binary facts live primarily in `manifests/<game>/<sha256>.json` and backend
constants with provenance. Versioned prose summarizes conclusions instead of
retaining session-by-session reverse-engineering chronology.

## Overture architecture

`products/overture` is an autonomous source/build/package host. Its compile path
bridges the game/HPL source into Framework adapters and shared runtime:

```text
PenumbraOverture / HPL1Engine
        |
OvertureSourceIntegration
        |
OvertureBackend
        |
shared runtime policies
```

The product preserves its own game resources, shaders, audio wrapper, controller
assets, installer and release documentation. Framework changes must keep the
Overture product green while shared behavior evolves.

## Black Plague architecture

Black Plague uses the allowlisted Steam x86 build. The normal development
installation deploys an ALUT proxy bootstrap, the Framework probe, OpenVR assets,
hand texture, localization and generated HRTF configuration through the payload
contract in `assets/deployment/manifest.json`.

The bootstrap validates the exact executable and waits for the demonstrated
runtime-readiness boundary before deeper renderer hooks become active. The probe
uses process-lifetime residency: shutdown restores owned hooks/resources but the
DLL remains loaded until game exit because live evidence disproved safe unload.

The ordinary Steam **Play** bootstrap path has reached gameplay VR on the
allowlisted build. Subsequent presentation-pacing and packaged-hand-texture
corrections are host-tested and remain focused headset regression items.

### Presentation and tracking

The active path is conceptually:

```text
OpenVR pose
   |
tracking/world-yaw policy
   |
visibility preparation -----> per-eye camera transaction
                                |              |
                           left eye        right eye
                                \              /
                                 compositor submit
```

A compositor pose sequence is consumed once. When the game reaches an eye render
without a fresh visibility-owned sequence, the renderer can acquire a fresh pose
for that outer loop instead of alternating a native-only frame. Camera/GL state
is restored transactionally around eye work.

Author-authored map-start yaw remains game-owned. The backend compensates only the
mapped native spawn-yaw delta out of VR world yaw so door/map transitions do not
silently redefine the user's tracked world. Camera-facing particles have a
separate per-eye refresh boundary because geometry generated for one eye cannot
be reused blindly for the other.

### Body and locomotion

The native physics loop remains the single owner of `iCharacterBody::Update`.
The adapter never calls it a second time. VR contributes horizontal intent and
observes/filters the displacement accepted by the native collision solver.

Shared policy provides:

- HMD-relative metric walk/sprint (`1.5/2.25 m/s` reference policy);
- physical room-scale translation;
- accepted-motion reconciliation and tracking-anchor carry;
- physical/button/hybrid crouch intent;
- accepted-travel footstep cadence.

Black Plague owns the exact body/collision boundary, native step behavior,
movement-state restrictions and player lifecycle checks. Physical HMD motion
must never credit lateral/backward collision correction as accepted head motion;
the current candidate projects actual body motion onto the requested physical
direction and clamps it to the request length.

### Hands and interaction

OpenVR actions publish grip/aim/skeletal inputs. Shared hand policy conditions
finger channels and provides reusable contact/grab math. Black Plague maps those
outputs onto its exact native entities and physics.

Free bodies, native `Move` interactions and constrained mechanisms are distinct
ownership families. Free-body placement can be Framework-owned after native
selection, while doors/sliders/hinges preserve native lifecycle and joints and
consume only demonstrated shared servo/contact behavior. Unknown mechanism
families fail closed.

Palm collision controls visible/resolved hand placement; raw controller aim
remains available for pointing and bounded acquisition assistance. Tool sockets
and rig-specific axes stay in per-game/profile data.

### UI and settings

Black Plague consumes the shared settings schema through
`%LOCALAPPDATA%\PenumbraVR\settings.ini` and a native `VR Settings` page inserted
through the verified Options UI boundary. Input/presentation settings that are
safe to refresh apply live; render-scale/audio startup settings keep their
restart semantics.

Inventory/notebook activation reuses mapped native actions. The gameplay HUD and
subtitle queue is captured from the existing native draw owner and composited
into both eyes; `SubtitleScale` remains below functional-support status until the
surface is headset-validated.

### Audio and visuals

HRTF configuration is written before OpenAL device creation. Black Plague also
has an exact-build OpenAL/EFX consumer for the shared mine-gallery reverb/bus
trim. The separate Rework distance/occlusion low-pass behavior still lacks a
safe target boundary.

The transferable Enhanced Visuals eye stage is available through a renderer-
neutral intermediate/MSAA/final-treatment path with direct-eye fallback. HPL
material/ambient behavior that is specific to Overture stays product-owned until
another backend demonstrates a real reusable boundary.

## Deployment direction

The final product should expose one safe installer while retaining the different
integration models. Installer responsibilities are:

1. discover Steam/manual installations;
2. fingerprint exact builds and fail closed on unknown hashes;
3. plan every file/executable change before writing;
4. back up and verify originals;
5. apply known executable transforms and payloads transactionally;
6. verify installed hashes/state;
7. support repair and exact rollback.

`assets/deployment/manifest.json` is the payload ownership contract and
`assets/settings/recommended.json` is the optional recommended-profile input.
User calibration must not be silently overwritten.

## Current implementation boundary

Overture is integrated, Black Plague has a substantial playable development
backend and Requiem gameplay remains gated. The immediate architectural work is
therefore validation and completion of the existing Black Plague consumers, not
creation of another abstraction layer. A shared subsystem is considered proven
only when a real second consumer uses it successfully.
