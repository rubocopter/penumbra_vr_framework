# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR is building a single user-facing PCVR mod for **Penumbra: Overture, Black Plague and Requiem**, with a shared VR runtime and game-specific backends where each title needs them.

> **Current status: pre-alpha framework development.** Overture has a proven playable reference implementation; Black Plague is under active headset development; Requiem is planned.

[Project status](#current-status) · [Architecture](ARCHITECTURE.md) · [Roadmap](ROADMAP.md) · [Supported builds](docs/SUPPORTED_BUILDS.md)

## The goal

The project started from the playable [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework). Instead of rebuilding the same VR systems separately for every game, this repository is turning that work into a reusable framework for the trilogy.

The guiding rule is simple: **port proven Rework behavior instead of reinventing it**, while keeping game-specific hooks and binary knowledge behind narrow backends. For Overture, the Framework now owns the complete source/build host under `products/overture`; Rework revision `23c890f` is behavioral/source evidence, not a build input.

The common framework targets:

- room-scale tracking and calibrated player height;
- standing and seated play;
- motion-controller input and physical interaction;
- locomotion, collision reconciliation and comfort settings;
- VR UI, audio and configuration;
- per-game integration without leaking addresses or object layouts into the shared runtime;
- one eventual installer that identifies the game and deploys the appropriate backend.

## Current status

| Game | State | Integration |
| --- | --- | --- |
| **Penumbra: Overture** | Framework-hosted; initial headset validation complete | Source-level HPL1 integration |
| **Penumbra: Black Plague** | Experimental; active headset development | Exact-build binary integration |
| **Penumbra: Requiem** | Planned | Exact-build binary integration |

### Overture

The Framework builds the real `Penumbra_vr.exe` and deployable Overture overlay from its own source host. The shared runtime/backend owns proven tracking, calibrated height, seated/standing, yaw/recenter, room-scale reconciliation and locomotion; the narrow adapter retains `cPlayer`/HPL body, collision and jump boundaries. [Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) revision `23c890f` remains the behavioral reference, but is not read by build or packaging. The initial headset pass found no evident regression, but it was not exhaustive and does not make Overture a supported release.

See [Overture backend migration](docs/OVERTURE_BACKEND_MIGRATION.md) and [Rework porting plan](docs/REWORK_PORTING_PLAN.md).

### Black Plague

Black Plague can already enter VR with native stereo and HMD-aware rendering. OpenVR action polling, an exact-build input bridge, tracked menus, provisional hands, controller picking and initial grab/throw paths are implemented, but this is **not yet a supported gameplay release**.

The next milestones are focused on porting the proven Overture gameplay behavior correctly: physical reach and interaction, body/capsule mapping, room-scale positional movement, held tools/lights, collision behavior and performance. The desktop mirror remains outside the immediate gameplay milestones because it is not yet reliable.

For development startup and controller state, see [VR startup and controllers](docs/VR_STARTUP_AND_CONTROLLERS.md) and [Black Plague probe](docs/BLACK_PLAGUE_PROBE.md).

### Requiem

Requiem is part of the framework target but is not yet an active gameplay backend. Work there should reuse the common runtime and lessons from Black Plague rather than start another independent VR implementation.

## Architecture

```text
Penumbra VR
├── runtime                 OpenVR, tracking, actions, configuration and logging
├── adapters/hpl1           Reusable HPL1 behavior
├── adapters/overture_source Source-level cPlayer/HPL boundary
├── backends
│   ├── overture            Overture sequencing over the shared runtime
│   ├── black_plague_binary Version-specific binary integration
│   └── requiem_binary      Version-specific binary integration
├── products/overture       Overture/HPL host, Win32 dependencies and package scripts
├── launcher/bootstrap      Game/build identification and backend loading
├── deployment/installer    Detection, backup, deployment and rollback
├── assets/openvr           Shared bindings plus Overture-specific overlay
├── manifests               Exact-build identity and research evidence
├── tools                   Build fingerprinting and research utilities
└── tests                   Host-independent verification
```

The shared runtime stays independent from game addresses and object layouts. Unknown executable builds must fail closed, and binary signatures, RVAs, offsets and calling conventions require evidence before they are treated as supported.

## Building

Native targets are Windows/x86. Development requires Visual Studio 2022 or Build Tools with the **Desktop development with C++** workload and CMake 3.25 or newer.

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

OpenVR is optional at configure time. Point `PENUMBRA_VR_OPENVR_SDK` to an unpacked SDK containing `headers/openvr.h` and `bin/win32/openvr_api.dll` when building OpenVR-enabled targets.

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=C:\path\to\openvr-sdk
```

Repository metadata can be validated independently with:

```powershell
.\tools\Test-PenumbraVrMetadata.ps1
```

Build and package Overture from this repository with:

```powershell
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
```

This produces `products/overture/build/bin/Release/Penumbra_vr.exe` and the deployable overlay in `products/overture/build/package/Release/PenumbraVR`. It needs no Rework checkout. The package targets a valid retail installation; use its `Install-PenumbraVR.bat` to deploy it rather than launching the executable from the staging directory.

The Overture host owns its pinned OpenVR 2.15.6 SDK, which can configure the root OpenVR-enabled targets:

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=.\products\overture\dependencies\openvr-2.15.6
```

## Documentation

- [Architecture](ARCHITECTURE.md)
- [Roadmap](ROADMAP.md)
- [Changelog](CHANGELOG.md)
- [VR configuration](docs/VR_CONFIGURATION.md)
- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Overture backend migration](docs/OVERTURE_BACKEND_MIGRATION.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)
- [Black Plague probe](docs/BLACK_PLAGUE_PROBE.md)
- [Binary research](docs/BINARY_RESEARCH.md)
- [Installer design](docs/INSTALLER_DESIGN.md)

## Identity and lineage

**Penumbra VR** is the user-facing project name. This repository is separate from [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the original Overture-only VR mod, and from the [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) derived from it.

The framework is an unofficial community project and does not claim to be an official continuation. Attribution for the original mod, Rework, Frictional Games, Valve and other components is maintained in [THIRD_PARTY.md](THIRD_PARTY.md).

## Support

If you enjoy the project and would like to support its continued development, you can [support me on Ko-fi](https://ko-fi.com/onitaku).

## License

Penumbra VR is licensed under GNU GPL v3 or later; see [COPYING](COPYING). Imported HPL1/Overture source retains its notices, assets and shaders retain their separate terms, and OpenVR remains under Valve's BSD-style license. See [THIRD_PARTY.md](THIRD_PARTY.md) and [`products/overture/SOURCE_PROVENANCE.md`](products/overture/SOURCE_PROVENANCE.md) for component-level provenance.
