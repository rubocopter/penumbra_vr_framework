# Penumbra VR Framework

<p align="center">
  <a href="COPYING"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**One PCVR framework for Penumbra: Overture, Black Plague and Requiem.**

The project takes the proven Overture VR work and turns it into a shared framework that can support the rest of the trilogy without losing the game-specific behavior each title needs.

> **Pre-alpha — no public Framework release yet.** Overture is integrated and serves as the proven reference. Black Plague is the active development target and already has substantial headset-tested VR functionality. Requiem comes after the Black Plague framework-readiness gate.

## Games

| Game | State |
| --- | --- |
| Penumbra: Overture | Framework backend with Rework behavior as reference |
| Penumbra: Black Plague | Active VR backend development and headset validation |
| Penumbra: Requiem | Planned after Black Plague framework validation |

For a finished, playable Overture package today, use [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework).

## What the Framework is building toward

- Shared room-scale tracking, locomotion, input, interaction and comfort systems.
- Game-specific backends for renderer, body/collision, UI and exact engine boundaries.
- Motion-controlled hands and interactions instead of simple controller remapping.
- Independent validation for each game before shared behavior is treated as portable.

## Project documentation

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Trilogy parity](docs/TRILOGY_PARITY_PLAN.md) · [Headset validation](docs/VR_HEADSET_TEST_CHECKLIST.md) · [Supported builds](docs/SUPPORTED_BUILDS.md)

More detailed Black Plague research lives in [probe notes](docs/BLACK_PLAGUE_PROBE.md) and [spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md). Internal engineering checkpoints stay under `docs/internal/`.

<details>
<summary><strong>For developers</strong></summary>

Native targets are Windows/x86. Development requires Visual Studio 2022 with C++ support and CMake 3.25+.

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release --output-on-failure
```

The project preserves proven Rework behavior first, keeps exact-build assumptions inside game backends and treats implementation, host testing and headset validation as separate states. See [Rework porting](docs/REWORK_PORTING_PLAN.md) and [design decisions](docs/DESIGN_DECISIONS.md) for the engineering rules.

</details>

## Lineage and license

Penumbra VR Framework is an unofficial community project. It is separate from [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr) and the original Overture-only mod.

Licensed under **GNU GPL v3 or later**. See [COPYING](COPYING) and [THIRD_PARTY.md](THIRD_PARTY.md).
