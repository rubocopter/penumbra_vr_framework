# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR is building a single user-facing PCVR mod for **Penumbra: Overture, Black Plague and Requiem**, with a shared VR runtime and narrow game-specific backends where each title needs them.

> **Current status: pre-alpha framework development.** Overture is Framework-hosted and has completed an initial headset validation. Black Plague has working stereo/head tracking plus a live-tested exact-build player-body boundary and is now moving into tracking/body reconciliation. Requiem is planned.

[Project status](#current-status) · [Architecture](ARCHITECTURE.md) · [Roadmap](ROADMAP.md) · [Supported builds](docs/SUPPORTED_BUILDS.md)

## The goal

The project started from the playable [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework). Instead of rebuilding the same VR systems separately for every game, this repository turns proven behavior into a reusable trilogy framework.

The core rule is to **port demonstrated behavior instead of reinventing it**, while keeping game-specific hooks, object layouts and binary knowledge behind narrow adapters. Rework revision `23c890f` remains the Overture behavioral reference, but it is no longer a build/package dependency.

Reuse is bidirectional: if another backend demonstrates a better game-neutral implementation, the Framework should preserve the better behavior and move it toward shared policy rather than forcing every game to copy Overture. Black Plague's richer finger-articulation semantics are the current example.

The common framework targets:

- room-scale tracking and calibrated player height;
- standing and seated play;
- motion-controller input and physical interaction;
- locomotion, collision reconciliation and comfort settings;
- VR UI, audio and configuration;
- per-game integration without leaking addresses or object layouts into shared runtime code;
- one eventual installer that detects the game/build and deploys the appropriate backend.

## Current status

| Game | State | Integration |
| --- | --- | --- |
| **Penumbra: Overture** | Framework-hosted; initial headset validation complete | Source-level HPL1 integration |
| **Penumbra: Black Plague** | Experimental; active gameplay/body integration | Exact-build binary integration |
| **Penumbra: Requiem** | Planned | Exact-build binary integration |

### Overture

The Framework builds the real `Penumbra_vr.exe` and deployable Overture overlay from its own source host under `products/overture`. Shared runtime/backend code owns the proven tracking, calibrated height, seated/standing, yaw/recenter, room-scale reconciliation and locomotion behavior; a narrow HPL adapter retains source-game body/collision/jump details.

The autonomous Release executable has completed an initial SteamVR/headset/controller pass with no evident regression versus the previously tested Rework behavior. That is initial headset validation, not exhaustive equivalence and not a supported-release claim.

See [Overture backend migration](docs/OVERTURE_BACKEND_MIGRATION.md) and [Rework porting plan](docs/REWORK_PORTING_PLAN.md).

### Black Plague

Black Plague already has native stereo, yaw-aligned HMD rotation and keyboard/mouse preservation validated in-headset. OpenVR action polling, an exact-build input bridge, tracked menus, provisional hands, controller picking and initial grab/throw paths are implemented, but the backend is still experimental.

The exact-build player/body pipeline is now substantially characterized and no longer the main unknown. Live evidence covers the native character body, `1/60` body tick, free movement, blocking, sliding/partial acceptance, physical crouch shape swap and native jump ownership. The first narrow `BlackPlagueBodyAdapter` is also live-tested: it binds to the existing movement/body-update owners, never calls the native body update twice, dynamically re-resolves the current body and exposes accepted displacement through the shared `VrAcceptedBodyMotion` contract.

**Current gameplay milestone:** connect shared tracking/body spatial reconciliation through that live-tested boundary while keeping positional HMD translation disabled until the policy is host-tested and deliberately headset-validated. Speed tuning, physical crouch, jump changes and camera/bob comfort remain separate gates.

The desktop mirror remains outside the immediate gameplay milestones because it is not yet reliable.

For implementation state and evidence, see [Black Plague probe](docs/BLACK_PLAGUE_PROBE.md), [Black Plague spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md) and [Roadmap](ROADMAP.md).

### Requiem

Requiem is part of the framework target but is not yet an active gameplay backend. It should reuse the common runtime and validated lessons from the other two games, while repeating exact-build research wherever Black Plague binary evidence cannot safely transfer.

## Architecture

```text
Penumbra VR
├── runtime                  OpenVR, tracking, actions, configuration and shared policy
├── adapters/hpl1            Reusable HPL1 behavior
├── adapters/overture_source Source-level cPlayer/HPL boundary
├── backends
│   ├── overture             Overture sequencing over shared runtime policy
│   ├── black_plague         Exact-build binary integration and adapters
│   └── requiem              Future exact-build binary integration
├── products/overture        Overture/HPL host, Win32 dependencies and package scripts
├── launcher/bootstrap       Game/build identification and backend loading
├── deployment/installer     Detection, backup, deployment and rollback
├── assets/openvr            Shared bindings plus per-game overlays
├── manifests                Exact-build identity and research evidence
├── tools                    Build fingerprinting and research utilities
└── tests                    Host-independent verification
```

The shared runtime stays independent from game addresses and object layouts. Unknown executable builds must fail closed, and binary signatures, RVAs, offsets and calling conventions require evidence before they are treated as supported.

For exact-build hooks, a callsite has one owner. Other systems bind through explicit fan-out/status boundaries rather than stacking competing hooks on the same instruction.

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

The Overture host owns its pinned OpenVR 2.15.6 SDK, which can also configure the root OpenVR-enabled targets:

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=.\products\overture\dependencies\openvr-2.15.6
```

## Documentation

- [Architecture](ARCHITECTURE.md)
- [Roadmap](ROADMAP.md)
- [Changelog](CHANGELOG.md)
- [Codex handoff](CODEX_HANDOFF.md)
- [Debug handoff](DEBUG_HANDOFF.md)
- [VR configuration](docs/VR_CONFIGURATION.md)
- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Overture backend migration](docs/OVERTURE_BACKEND_MIGRATION.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)
- [Black Plague probe](docs/BLACK_PLAGUE_PROBE.md)
- [Black Plague spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md)
- [Binary research](docs/BINARY_RESEARCH.md)
- [Installer design](docs/INSTALLER_DESIGN.md)

## Identity and lineage

**Penumbra VR** is the user-facing project name. This repository is separate from [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the original Overture-only VR mod, and from the [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) derived from it.

The framework is an unofficial community project and does not claim to be an official continuation. Attribution for the original mod, Rework, Frictional Games, Valve and other components is maintained in [THIRD_PARTY.md](THIRD_PARTY.md).

## Support

If you enjoy the project and would like to support its continued development, you can [support me on Ko-fi](https://ko-fi.com/onitaku).

## License

Penumbra VR is licensed under GNU GPL v3 or later; see [COPYING](COPYING). Imported HPL1/Overture source retains its notices, assets and shaders retain their separate terms, and OpenVR remains under Valve's BSD-style license. See [THIRD_PARTY.md](THIRD_PARTY.md) and [`products/overture/SOURCE_PROVENANCE.md`](products/overture/SOURCE_PROVENANCE.md) for component-level provenance.
