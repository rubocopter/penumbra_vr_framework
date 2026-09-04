# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: Black Plague bootstrap and render-path research.**

This repository does not currently contain a playable mod or installer. It contains the first verified Black Plague binary integration plus shared OpenVR, HPL1-camera, render-target, visual-calibration, spatial-audio and deployment components. Three runs submitted 900 static stereo frames to the headset; native full-view stereo was observed at the deliberately low diagnostic resolution. Six more runs submitted 1,800 frames with Overture-derived yaw alignment and live rotation-only head tracking; the user confirmed correct world orientation and horizontal/vertical response. A continuous presentation lifecycle now keeps OpenVR and runtime-sized eye targets active until explicitly stopped, with proportional allocation fallback copied from the proven Rework policy; this new path compiles with the passing test suite but has not yet received an in-headset run. Deactivation restores every hook but deliberately keeps the research DLL resident until the game exits. Shared OpenVR action/binding data has been imported, including PSVR2 Sense, but action polling, positional tracking, hands and gameplay interaction are not implemented yet.

SteamVR still shows Black Plague on a virtual cinema screen during normal execution. That is desktop mirroring, not stereo VR. The bounded diagnostic commands return to that path after 300 frames. The new `--start-vr` command instead begins continuous native presentation and `--stop-vr` tears it down; both remain explicitly experimental pending a longer headset test.

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
├── bootstrap               Host identification and backend loading
├── backends
│   ├── overture_source     Source-level HPL1/Overture integration
│   ├── black_plague_binary Version-specific binary integration
│   └── requiem_binary      Version-specific binary integration
├── installer               Detection, deployment, backup and rollback
├── manifests               OpenVR action manifests
├── bindings                Controller bindings
├── tools                   Build fingerprinting and research utilities
└── tests                   Host-independent verification
```

These are architectural boundaries, not claims that the components already exist. Directories and build targets will be added when their first working implementation is ready.

## Build and current probe

The native targets are Windows/x86 because the observed game executables are 32-bit:

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

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
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --stop-vr <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <process-id>
```

The OpenVR-sized target, controlled world-duplication and stereo commands are experimental. The diagnostic submission commands remain fixed at 512×512 so their prior evidence stays reproducible. `--start-vr` uses SteamVR's recommended per-eye size at the Rework default scale of 1.0 and retries progressively smaller proportional targets if allocation fails; it preserves the validated yaw-aligned rotation and runs until `--stop-vr` or `--detach`. This continuous mode has not yet been validated in the headset. Positional motion remains disabled. OpenVR is only available in builds configured with `PENUMBRA_VR_OPENVR_SDK`. Logs are written to `%LOCALAPPDATA%\PenumbraVR\logs`. This is a developer probe, not an end-user launcher.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), [docs/INSTALLER_DESIGN.md](docs/INSTALLER_DESIGN.md), [docs/REWORK_PORTING_PLAN.md](docs/REWORK_PORTING_PLAN.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime, deployment or hook code.

## Licensing

Penumbra VR is licensed under the GNU General Public License version 3 or any later version; see [COPYING](COPYING). This matches the HPL1/Overture codebase and permits the intended, attributed reuse of Penumbra VR Rework. OpenVR remains under Valve's BSD-style license and is consumed as an external SDK dependency. See [THIRD_PARTY.md](THIRD_PARTY.md) for component-level provenance.
