# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: common runtime, initial Overture backend and Black Plague research backend.**

The framework now builds a host-tested Overture backend core derived from
Rework revision `23c890f`. It owns the proven tracking-space, calibrated-height,
seated-mode, room-scale collision reconciliation and fixed-displacement
locomotion sequence while a narrow adapter owns HPL body collision and jump
calls. It is not yet linked into or deployed with the Overture executable, so
this is an architectural/behavioral integration milestone rather than a new
playable Overture package. See [the migration audit](docs/OVERTURE_BACKEND_MIGRATION.md).

This repository contains an experimental Black Plague binary integration, not a released playable mod or installer. Native stereo, yaw-aligned rotation tracking, keyboard/mouse input and conservative HMD-aware visibility have been validated in the headset at `3400x3468` per eye. Both the two-eye schedule and optional third monitor pass have executed with correct live telemetry. After disabling the legacy frame cap, the 2026-09-05 session produced sampled intervals of about 90 submissions per second; compositor reprojection and representative-scene frame pacing still need measurement. Deactivation restores hooks but deliberately keeps the research DLL resident until game exit.

The latest session exposed medium-distance lamp illumination disappearing during head movement while the bulb stayed lit. A Rework-informed eye-resolution scissor correction, GL-state isolation and diagnostics are implemented, but **not yet headset-validated**; see [the lighting checklist](docs/VR_LIGHTING_VALIDATION.md). OpenVR action polling and an exact-build native input bridge are implemented, together with tracked menus, provisional procedural gloves, controller picking and free-body palm-relative grab/throw. These new paths are code-tested, not headset-validated. Palm collision, articulated mechanisms, tool/light attachment, positional body tracking and Enhanced visuals GPU integration remain pending. This is not yet a completed Rework gameplay port.

For one-step startup use `Start-Black-Plague-VR.cmd`, which launches through Steam and enables VR without manual PID/attach commands. Read [startup instructions and precise controller status](docs/VR_STARTUP_AND_CONTROLLERS.md); only preflight, code and GL tests have been run for this new path, not a new live/headset session.

SteamVR still shows Black Plague on a virtual cinema screen during normal execution. That is desktop mirroring, not stereo VR. The bounded diagnostic commands return to that path after 300 frames. The new `--start-vr` command instead begins continuous native presentation and `--stop-vr` tears it down; both remain explicitly experimental. The HMD-aware conservative render-list update is now runtime- and headset-validated; frame-pacing work remains open.

The existing, playable Overture implementation remains in [rubocopter/penumbra_vr_rework](https://github.com/rubocopter/penumbra_vr_rework). It is the behavioral reference for this project. Selected behavior has begun moving into game-neutral modules with explicit provenance; the Overture game layer is not copied wholesale.

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

Black Plague must currently be launched through Steam. Once it is running, attach or remove the probe with:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --attach <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-openvr-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-world-duplication <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-matrices <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-submission <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-tracked-stereo-submission <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --start-vr <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --vr-mirror-on <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --vr-mirror-off <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --stop-vr <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <process-id>
```

The OpenVR-sized target, controlled world-duplication and stereo commands are experimental. The diagnostic submission commands remain fixed at 512×512 so their prior evidence stays reproducible. `--start-vr` uses SteamVR's recommended per-eye size at the Rework default scale of 1.0 and retries progressively smaller proportional targets if allocation fails; it preserves the validated yaw-aligned rotation and runs until `--stop-vr` or `--detach`. Two continuous PS VR2 sessions have validated the full `3400x3468` lifecycle, HMD-aware visibility, keyboard/mouse preservation and clean stop/teardown. Continuous mode advances game time on the first eye and omits the third world pass when the monitor mirror is off. `--vr-mirror-on` restores the separate desktop pass, while `--vr-mirror-off` returns to the two-pass path; both commands persist the choice in `%LOCALAPPDATA%\PenumbraVR\settings.ini`, and `--start-vr` reapplies it. The safe default is off. Those scheduling modes still require complete physical validation. Acceptable 90 Hz frame pacing remains open. Positional motion remains disabled. OpenVR is only available in builds configured with `PENUMBRA_VR_OPENVR_SDK`. Logs are written to `%LOCALAPPDATA%\PenumbraVR\logs`. This is a developer probe, not an end-user launcher or settings screen.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), [docs/VR_CONFIGURATION.md](docs/VR_CONFIGURATION.md), [docs/INSTALLER_DESIGN.md](docs/INSTALLER_DESIGN.md), [docs/REWORK_PORTING_PLAN.md](docs/REWORK_PORTING_PLAN.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime, deployment or hook code.

## Licensing

Penumbra VR is licensed under the GNU General Public License version 3 or any later version; see [COPYING](COPYING). This matches the HPL1/Overture codebase and permits the intended, attributed reuse of Penumbra VR Rework. OpenVR remains under Valve's BSD-style license and is consumed as an external SDK dependency. See [THIRD_PARTY.md](THIRD_PARTY.md) for component-level provenance.
