# Third-party code and provenance

The repository is licensed under GPLv3 or later. Imported or adapted work is recorded below; build outputs and the optional OpenVR SDK are not source-controlled.

Likely upstream inputs include:

| Component | Upstream | Known license | Current state |
|---|---|---|---|
| Penumbra Overture game code | Frictional Games | GPLv3 | Not imported |
| HPL1 Engine | Frictional Games | GPLv3, with separately documented asset/shader terms | Not imported |
| Penumbra VR Rework tracking-space behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/include/game/VRTracking.h`, `HPL1Engine/sources/scene/Camera3D.cpp`, `tests/VRTrackingTest/main.cpp` | GPLv3 or later | Adapted to runtime-neutral matrices in `src/runtime/vr_math.*` and `tests/vr_math/vr_math_test.cpp`; HPL types and game-specific placement were not copied |
| Penumbra VR Rework render-target policy | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/sources/graphics/Renderer3D.cpp`, `PenumbraOverture/VRSettings.cpp` | GPLv3 or later | Adapted to runtime-neutral sizing and proportional fallback in `src/runtime/render_target_policy.*`; the default scale and 0.5–2.0 limits are preserved |
| Penumbra VR Rework visual calibration v4 | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/assets/core/programs/VR_*.cg`, `Ambient_Hemisphere_*.cg`, `scripts/test-vr-visuals.ps1` | GPLv3 plus the HPL1 shader notice in the original files | CPU reference behavior adapted in `src/graphics/visual_calibration.*`; GPU shader integration is not yet imported |
| Penumbra VR Rework spatial-audio behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `PenumbraOverture/Init.cpp`, `HPL1Engine/sources/sound/SoundHandler.cpp`, `HPL1Engine/sources/impl/LowLevelSoundOpenAL.cpp` | GPLv3 or later | HRTF config, occlusion/distance filter and mine-gallery EFX parameters adapted in `src/audio/spatial_audio.*`; binary HPL/OpenAL hooks are not yet implemented |
| Penumbra VR Rework OpenVR actions and bindings | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/data/vr/actions.json`, `HPL1Engine/data/vr/bindings/*.json` | GPLv3 or later | Imported under `assets/openvr`, renamed for Penumbra VR and kept as runtime-neutral data; action polling, SteamVR registration and installer deployment are not yet implemented |
| OpenVR SDK 2.15.6 | Valve Software | BSD-style 3-clause license | Optional external build dependency; not stored in this repository |
| Other libraries | To be selected | To be reviewed | Not imported |

Before importing or deriving code, the change must record:

- upstream repository and revision
- original path and copyright notice
- applicable license
- local destination and modifications
- whether the code becomes part of the runtime, a backend, a tool or packaging

Project-wide GPLv3-or-later licensing is recorded in `COPYING`. This is compatible with the intended reuse of Overture/HPL1-derived implementation. Separately licensed dependencies and assets retain their own terms. This file records engineering provenance and is not legal advice.
