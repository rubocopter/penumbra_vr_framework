# Penumbra VR Framework

**One VR framework for the Penumbra trilogy.**

Penumbra VR Framework is an open-source PCVR project for **Penumbra: Overture, Black Plague and Requiem**. The objective is a reusable framework with shared VR systems and narrow per-game integrations instead of separate incompatible mods.

> **Status: pre-alpha.** Overture is Framework-hosted and validated through the initial headset pass. Black Plague is under active gameplay/body integration. Requiem is planned.

## Documentation

- [Roadmap](ROADMAP.md)
- [Architecture](ARCHITECTURE.md)
- [Audit status and implementation tracking](docs/AUDIT_STATUS.md)
- [Codex implementation prompt](docs/CODEX_IMPLEMENTATION_PROMPT.md)
- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)

## Project state

| Game | State | Integration |
| --- | --- | --- |
| Penumbra: Overture | Framework-hosted | Source-level HPL1 integration |
| Penumbra: Black Plague | Active development | Exact-build binary integration |
| Penumbra: Requiem | Planned | Exact-build binary integration |

## Design principles

- Preserve validated behavior before refactoring.
- Keep game-neutral VR behavior in shared runtime systems.
- Keep native engine details, RVAs and binary contracts inside adapters/backends.
- Reuse Overture Rework as behavioral reference, not as a permanent dependency.
- Track implementation, host testing and headset validation separately.

## Repository layout

```text
src/                    shared runtime, adapters and game backends
products/overture/      Framework-owned Overture source host
assets/openvr/          shared controller bindings and overlays
manifests/              exact-build identity data
tools/                  build and validation utilities
tests/                  host-independent tests
docs/                    technical documentation
```

Unknown executable builds fail closed. Game-specific binary knowledge is not treated as a generic HPL contract.

## Building

Windows/x86 development environment:

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

Standalone Overture package:

```powershell
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
```

## Support

If you want to support development, see [Ko-fi](https://ko-fi.com/onitaku).

## License

Penumbra VR Framework is licensed under GNU GPL v3 or later. See [COPYING](COPYING) and [THIRD_PARTY.md](THIRD_PARTY.md).
