# Third-party code and provenance

No third-party source or binary is currently stored in this repository.

Likely upstream inputs include:

| Component | Upstream | Known license | Current state |
|---|---|---|---|
| Penumbra Overture game code | Frictional Games | GPLv3 | Not imported |
| HPL1 Engine | Frictional Games | GPLv3, with separately documented asset/shader terms | Not imported |
| Penumbra VR Rework modifications | rubocopter/penumbra_vr_rework and its contributors | Requires provenance review; built on GPLv3 sources | Not imported |
| OpenVR SDK | Valve Software | BSD-style 3-clause license | Not imported |
| Other libraries | To be selected | To be reviewed | Not imported |

Before importing or deriving code, the change must record:

- upstream repository and revision
- original path and copyright notice
- applicable license
- local destination and modifications
- whether the code becomes part of the runtime, a backend, a tool or packaging

The project-wide license remains undecided. Because meaningful reuse of Overture/HPL1-derived implementation is expected, GPLv3 compatibility must be treated as a design constraint before distribution. This file records engineering intent and is not legal advice.
