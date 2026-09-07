# Third-party code and provenance

The repository is licensed under GPLv3 or later. Imported or adapted work is recorded below; build outputs and the optional OpenVR SDK are not source-controlled.

## Project lineage

[`veryjos/penumbra_vr`](https://github.com/veryjos/penumbra_vr) is the original
Overture-only VR mod. GitHub records
[`rubocopter/penumbra_vr_rework`](https://github.com/rubocopter/penumbra_vr_rework)
as its direct fork. This repository, `rubocopter/penumbra_vr_framework`, is a
separate trilogy framework and is not presented as an official continuation of
the original project. The distinct repository name is intentional.

Likely upstream inputs include:

| Component | Upstream | Known license | Current state |
|---|---|---|---|
| Original Penumbra: Overture VR mod lineage | `veryjos/penumbra_vr`; upstream of the Rework fork | No top-level license file or GitHub license metadata observed; embedded HPL1/Overture source retains its own notices | Historical attribution recorded here; this framework does not bulk-import the original repository |
| Penumbra Overture game code | Frictional Games | GPLv3 | Not imported |
| HPL1 Engine | Frictional Games | GPLv3, with separately documented asset/shader terms | Not imported |
| Penumbra VR Rework tracking-space behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/include/game/VRTracking.h`, `HPL1Engine/sources/scene/Camera3D.cpp`, `tests/VRTrackingTest/main.cpp` | GPLv3 or later | Adapted to runtime-neutral matrices in `src/runtime/vr_math.*` and `tests/vr_math/vr_math_test.cpp`; HPL types and game-specific placement were not copied |
| Penumbra VR Rework tracking space, locomotion and direct-reach policy | Frictional Games and `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/include/game/VRTracking.h`, `PenumbraOverture/Player.cpp`, `ButtonHandler.cpp`, `PlayerState_Misc_VR.cpp`, `VRHandCollisionPolicy.h` | GPLv3 or later, original Frictional Games 2006–2010 notice | Behavior adapted into `src/runtime/vr_tracking_space.*`, `vr_locomotion.*`, `vr_interaction_policy.*` and `src/backends/overture/*`; HPL body/collision calls are represented by an adapter interface. Constants, sequencing and seated calibration are preserved; validation adds invalid-transform/timing guards. |
| Penumbra VR Rework render-target policy | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/sources/graphics/Renderer3D.cpp`, `PenumbraOverture/VRSettings.cpp` | GPLv3 or later | Adapted to runtime-neutral sizing and proportional fallback in `src/runtime/render_target_policy.*`; the default scale and 0.5–2.0 limits are preserved |
| Penumbra VR Rework visual calibration v4 | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/assets/core/programs/VR_*.cg`, `Ambient_Hemisphere_*.cg`, `scripts/test-vr-visuals.ps1` | GPLv3 plus the HPL1 shader notice in the original files | CPU reference behavior adapted in `src/graphics/visual_calibration.*`; GPU shader integration is not yet imported |
| Penumbra VR Rework spatial-audio behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `PenumbraOverture/Init.cpp`, `HPL1Engine/sources/sound/SoundHandler.cpp`, `HPL1Engine/sources/impl/LowLevelSoundOpenAL.cpp` | GPLv3 or later | HRTF config, occlusion/distance filter and mine-gallery EFX parameters adapted in `src/audio/spatial_audio.*`; binary HPL/OpenAL hooks are not yet implemented |
| Penumbra VR Rework OpenVR actions and bindings | `rubocopter/penumbra_vr_rework` revision `23c890f`; `data/vr/actions.json`, `data/vr/bindings/*.json` | GPLv3 or later | Imported under `assets/openvr`, renamed for Penumbra VR and kept as runtime-neutral data; manifest registration and real action polling are now connected; installer deployment remains pending |
| Penumbra VR Rework logical input behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/include/input/SteamVRInput.h`, `HPL1Engine/sources/input/SteamVRInput.cpp`, `HPL1Engine/include/input/VRAnalog.hpp` | GPLv3 or later | Adapted into `src/runtime/vr_input_state.*` and its tests without HPL or OpenVR types; real polling is implemented in `vr_action_input.*` / `openvr_controller_input.cpp`, with native Black Plague intent consumption; live controller validation remains pending |
| Penumbra VR Rework settings behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `PenumbraOverture/VRSettings.h`, `PenumbraOverture/VRSettings.cpp`, `PenumbraOverture/Init.cpp`, `PenumbraOverture/MainMenu.cpp` | GPLv3 or later | Defaults, ranges, enum text values and the legacy smooth-turn migration are adapted into `src/runtime/vr_settings.*`; monitor-mirror state is included as a framework extension, while storage and UI remain backend concerns |
| Penumbra VR Rework monitor-mirror scheduling | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/sources/scene/Scene.cpp`, `HPL1Engine/include/game/Game.h`, `PenumbraOverture/Init.cpp`, `PenumbraOverture/MainMenu.cpp` | GPLv3 or later | Adapted into `src/runtime/stereo_render_policy.*` and the Black Plague render hook; the default is headset-only two-pass rendering and physical validation is pending |
| HPL1 / Penumbra VR Rework light-scissor behavior | Frictional Games and `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/sources/impl/LowLevelGraphicsSDL.cpp`, `sources/scene/Light3D.cpp`, `Light3DPoint.cpp`, `Light3DSpot.cpp`, `sources/math/Math.cpp` | GPLv3 or later; original source retains the Frictional Games copyright notice | Reimplemented as a binary-boundary coordinate adapter in `src/hooks/opengl_eye_scissor.*`, with eye-state isolation in `src/graphics/opengl_eye_targets.*`; no HPL objects or shaders copied; physical validation pending |
| OpenVR SDK 2.15.6 | Valve Software | BSD-style 3-clause license | Optional external build dependency; not stored in this repository |
| Penumbra VR Rework palm anchoring and throw limits | Frictional Games and Rework contributors, revision `23c890f`; `PenumbraOverture/PlayerState_Interact_VR.cpp` | GPLv3 or later, original Frictional Games 2006–2010 notice | Adapted into `src/runtime/vr_grab_pose.*` and the exact-build `spatial_interaction` backend; native Enter/Leave retain mass/gravity ownership, jointed and parented bodies excluded. No Rework meshes or textures copied; procedural glove geometry is framework code. |
| Other libraries | To be selected | To be reviewed | Not imported |

Before importing or deriving code, the change must record:

- upstream repository and revision
- original path and copyright notice
- applicable license
- local destination and modifications
- whether the code becomes part of the runtime, a backend, a tool or packaging

Project-wide GPLv3-or-later licensing is recorded in `COPYING`. This is compatible with the intended reuse of Overture/HPL1-derived implementation. Separately licensed dependencies and assets retain their own terms. This file records engineering provenance and is not legal advice.
