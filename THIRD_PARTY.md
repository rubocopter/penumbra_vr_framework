# Third-party code and provenance

The repository is licensed under GPLv3 or later. Imported or adapted work is recorded below; build outputs and the optional OpenVR SDK are not source-controlled.

Likely upstream inputs include:

| Component | Upstream | Known license | Current state |
|---|---|---|---|
| Penumbra Overture game code | Frictional Games | GPLv3 | Not imported |
| HPL1 Engine | Frictional Games | GPLv3, with separately documented asset/shader terms | Not imported |
| Penumbra VR Rework tracking-space behavior | `rubocopter/penumbra_vr_rework` revision `23c890f`; `HPL1Engine/include/game/VRTracking.h`, `HPL1Engine/sources/scene/Camera3D.cpp`, `tests/VRTrackingTest/main.cpp` | GPLv3 or later | Adapted to runtime-neutral matrices in `src/runtime/vr_math.*` and `tests/vr_math/vr_math_test.cpp`; HPL types and game-specific placement were not copied |
| OpenVR SDK 2.15.6 | Valve Software | BSD-style 3-clause license | Optional external build dependency; not stored in this repository |
| Other libraries | To be selected | To be reviewed | Not imported |

Before importing or deriving code, the change must record:

- upstream repository and revision
- original path and copyright notice
- applicable license
- local destination and modifications
- whether the code becomes part of the runtime, a backend, a tool or packaging

Project-wide GPLv3-or-later licensing is recorded in `COPYING`. This is compatible with the intended reuse of Overture/HPL1-derived implementation. Separately licensed dependencies and assets retain their own terms. This file records engineering provenance and is not legal advice.
