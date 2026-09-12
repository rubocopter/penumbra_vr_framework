# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR is an open-source PCVR project for **Penumbra: Overture, Black Plague and Requiem**. The goal is one user-facing framework with shared VR systems and narrow per-game integrations instead of three independent mods.

> **Status: pre-alpha.** Overture is Framework-hosted and has completed an initial headset validation. Black Plague is under active gameplay/body integration. Requiem is planned. There is no public release build yet.

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Supported builds](docs/SUPPORTED_BUILDS.md)

## Project status

| Game | State | Integration |
| --- | --- | --- |
| **Penumbra: Overture** | Initial headset validation complete | Source-level HPL1 integration |
| **Penumbra: Black Plague** | Active development | Exact-build binary integration |
| **Penumbra: Requiem** | Planned | Exact-build binary integration |

### Overture

The Framework now builds and packages the real `Penumbra_vr.exe` from its own source host. The earlier [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) remains the behavioral reference, but it is no longer a build dependency.

The autonomous Framework build has completed an initial SteamVR/headset/controller pass without an evident regression versus the previously tested Rework behavior. This is not yet an exhaustive validation or a supported release.

### Black Plague

Black Plague already has native stereo, rotational HMD tracking and keyboard/mouse preservation working in-headset. Controller input, tracked menus, provisional hands and initial physical interaction are also implemented.

The native player-body/collision boundary, first narrow body adapter and default-off tracking/body reconciliation shadow are now live-tested. The separate **collision-aware physical X/Z displacement request** through the existing native tick is implemented and host-tested at the exact-build pre-collision boundary. Positional HMD translation remains disabled until that request path is live-validated through real queue/injection/collision/reconciliation evidence.

The complete Rework-derived VR settings schema is persisted by the Framework, and the shared 18-row editor policy is host-tested. Black Plague has an explicit capability map for the settings its backend currently applies and an offline editor via `PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`. A dedicated in-game VR settings page still requires a demonstrated safe native-menu insertion boundary.

### Requiem

Requiem will reuse the shared runtime and proven architecture, while repeating binary research wherever Black Plague-specific evidence cannot safely transfer.

## Why a framework?

The project began as an Overture-only VR mod. The current direction is to keep game-neutral behavior in the shared runtime and isolate HPL/game-specific details behind adapters and exact-build backends.

The rule is evidence-driven reuse: preserve proven behavior, but do not force every game to copy Overture when another backend demonstrates a better game-neutral implementation.

## Repository layout

```text
src/                    shared runtime, adapters and game backends
products/overture/      Framework-owned Overture source host and packaging
assets/openvr/           shared controller bindings and per-game overlays
manifests/               exact-build identity/research data
tools/                   build, validation and research utilities
tests/                   host-independent tests
docs/                    technical and development documentation
```

Unknown executable builds fail closed. Game RVAs, object layouts and calling conventions remain backend-owned and are not treated as generic HPL contracts.

## Building

Native targets are Windows/x86. Development requires Visual Studio 2022 or Build Tools with **Desktop development with C++** and CMake 3.25+.

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

For the standalone Overture product:

```powershell
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
```

See the [architecture](ARCHITECTURE.md), [roadmap](ROADMAP.md), [Rework porting plan](docs/REWORK_PORTING_PLAN.md) and [Black Plague probe notes](docs/BLACK_PLAGUE_PROBE.md) for implementation details.

## Lineage and license

**Penumbra VR** is an unofficial community project. It is separate from [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the original Overture-only mod, and from the Overture Rework derived from it.

The Framework is licensed under GNU GPL v3 or later; see [COPYING](COPYING). Component attribution and third-party licensing are documented in [THIRD_PARTY.md](THIRD_PARTY.md).

## Support

If you want to support continued development, see [Ko-fi](https://ko-fi.com/onitaku).
