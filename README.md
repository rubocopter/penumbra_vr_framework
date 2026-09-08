# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: common runtime, initial Overture backend and Black Plague research backend.**

The framework now builds a host-tested Overture backend core derived from Rework revision `23c890f`. It ports the proven tracking-space, calibrated-height, seated/standing, world-yaw, room-scale collision-reconciliation and fixed-displacement locomotion behavior into game-neutral runtime modules, while a narrow Overture adapter owns HPL body collision and jump calls. This backend core is not yet linked into or deployed with the Overture executable and has not yet been headset-validated. It is therefore an architectural/behavioral integration milestone, not a new playable Overture package. See [the migration audit](docs/OVERTURE_BACKEND_MIGRATION.md).

The Black Plague integration remains experimental. Native stereo, yaw-aligned rotation tracking, keyboard/mouse input and conservative HMD-aware visibility have been validated in the headset at `3400x3468` per eye. The continuous stereo lifecycle and two-eye schedule have also been exercised in the headset. The optional desktop mirror path exists in code, but it is **not currently a supported or validated feature** and is intentionally excluded from the next gameplay milestones because it has continued to produce problems. Acceptable frame pacing at the target refresh rate also remains open.

OpenVR action polling and an exact-build native input bridge are implemented, together with tracked menus, provisional procedural gloves, controller picking and free-body palm-relative grab/throw. These new gameplay paths are code-tested but are not yet a completed headset-validated Rework gameplay port. Palm collision, articulated mechanisms, tool/light attachment, positional body tracking and Enhanced visuals GPU integration remain pending.

For one-step Black Plague startup use `Start-Black-Plague-VR.cmd`, which launches through Steam and enables VR without manual PID/attach commands. Read [startup instructions and controller status](docs/VR_STARTUP_AND_CONTROLLERS.md); validation states in that document are kept separate from code/test status.

The current Black Plague interaction work also uses the Rework direct physical reach policy of `0.18 m` for the generic prop-pick fallback instead of the native camera-ray distance. The exact-build body/capsule mapping required for room-scale positional translation is not yet established, so positional HMD movement remains disabled rather than being approximated with an unverified collider.

The existing, playable Overture implementation remains in [rubocopter/penumbra_vr_rework](https://github.com/rubocopter/penumbra_vr_rework). It is the behavioral reference for this project. The rule for the common runtime is to port **proven behavior** from Rework and adapt only the game-specific boundaries; the Overture game layer is not copied wholesale.

## Identity and lineage

**Penumbra VR** remains the user-facing name. The repository uses the distinct
slug [`rubocopter/penumbra_vr_framework`](https://github.com/rubocopter/penumbra_vr_framework)
to avoid being confused with
[`veryjos/penumbra_vr`](https://github.com/veryjos/penumbra_vr), the original
Overture-only VR mod. `rubocopter/penumbra_vr_rework` is a direct GitHub fork of
that original project.

This framework is a new, standalone repository for the trilogy. It is not a
GitHub fork of `veryjos/penumbra_vr`, does not claim to be its official
continuation, and preserves attribution for the original mod, the Rework fork,
Frictional Games and Valve in [THIRD_PARTY.md](THIRD_PARTY.md).

## Project principles

- One product and installer for users; game-specific backends internally.
- Detect exact executable builds and fail closed on unknown versions.
- Keep the VR runtime independent from game addresses and object layouts.
- Treat Overture source integration and binary hooks as different adapters.
- **Prefer porting proven Rework behavior over reimplementing it from scratch.**
- Require evidence for every signature, RVA, structure offset, and calling convention.
- Do not mark a backend supported until it reaches an in-headset milestone.
- Preserve upstream attribution and licenses when code is eventually imported.

## Intended components

```text
Penumbra VR
├── runtime                 OpenVR, tracking, actions, configuration and logging
├── adapters/hpl1           Reusable HPL1 behavior with no exact-build addresses
├── backends
│   ├── overture_source     Source-level HPL1/Overture integration
│   ├── black_plague_binary Version-specific binary integration
│   └── requiem_binary      Version-specific binary integration
├── launcher/bootstrap      Host identification and backend loading
├── deployment/installer    Detection, backup, deployment and rollback
├── assets/openvr           OpenVR action manifest and controller bindings
├── manifests               Exact-build identity and binary-research evidence
├── tools                   Build fingerprinting and research utilities
└── tests                   Host-independent verification
```

These are architectural boundaries, not claims that the components already exist. Directories and build targets will be added when their first working implementation is ready.

## Build and current probe

The native targets are Windows/x86 because the observed game executables are
32-bit. Development requires Visual Studio 2022 or Build Tools with the Desktop
development with C++ workload, including CMake 3.25 or newer. Run these commands
from a Visual Studio Developer PowerShell, or otherwise ensure `cmake` and
`ctest` are on `PATH`:

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

Repository metadata can be checked independently of compilation:

```powershell
.\tools\Test-PenumbraVrMetadata.ps1
```

This validates JSON parsing, agreement between exact-build manifests and the
compiled catalogue, and the semantic references in the OpenVR action/binding
data. CI runs this check and both Debug and Release tests without the external
OpenVR SDK.

OpenVR support is optional at configure time. Point `PENUMBRA_VR_OPENVR_SDK` at an unpacked SDK containing `headers/openvr.h` and `bin/win32/openvr_api.dll`; the SDK remains external to this repository:

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=C:\path\to\openvr-2.15.6
```

For Black Plague, use the developer launcher/probe described in
[docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md). Positional motion and
the desktop mirror should not be treated as working gameplay features merely
because their hooks or command paths exist.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), [docs/VR_CONFIGURATION.md](docs/VR_CONFIGURATION.md), [docs/INSTALLER_DESIGN.md](docs/INSTALLER_DESIGN.md), [docs/REWORK_PORTING_PLAN.md](docs/REWORK_PORTING_PLAN.md), [docs/OVERTURE_BACKEND_MIGRATION.md](docs/OVERTURE_BACKEND_MIGRATION.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime, deployment or hook code.

## Licensing

Penumbra VR is licensed under the GNU General Public License version 3 or any later version; see [COPYING](COPYING). This matches the HPL1/Overture codebase and permits the intended, attributed reuse of Penumbra VR Rework. OpenVR remains under Valve's BSD-style license and is consumed as an external SDK dependency. See [THIRD_PARTY.md](THIRD_PARTY.md) for component-level provenance.
