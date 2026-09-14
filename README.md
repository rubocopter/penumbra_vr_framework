# Penumbra VR Framework

**One VR framework for the Penumbra trilogy.**

Penumbra VR Framework is an open-source PCVR project for **Penumbra: Overture, Black Plague and Requiem**. The project keeps game-neutral VR behavior in a shared runtime while each game retains the narrow source or exact-build integration it actually requires.

> **Status: pre-alpha.** Overture is Framework-hosted and has completed an initial headset validation. Black Plague has a working VR research/gameplay backend and is in active comfort, interaction and lifecycle validation. Requiem is planned. There is no supported public release build yet.

## Current state

| Game | State | Integration |
| --- | --- | --- |
| **Penumbra: Overture** | Framework-hosted; initial functional headset pass completed | Source-level HPL1 product owned by this repository |
| **Penumbra: Black Plague** | Active development and validation | Exact-build x86 binary backend/probe |
| **Penumbra: Requiem** | Planned | Future exact-build binary backend |

### Overture

The Framework builds and packages its own `Penumbra_vr.exe`. The earlier [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) remains the behavioral reference, but it is not a build dependency. The autonomous Overture product is also used as a regression gate for shared-runtime extraction.

### Black Plague

Black Plague already has native stereo, rotational HMD tracking, OpenVR controller input, tracked menus, provisional hands, native body/collision integration and direct VR locomotion through the exact supported build.

The post-audit work replaced the old cross-tick body plan with a **single same-tick tracking/body transaction**, hardened crouch and lifecycle ownership, introduced identified presentation/tracking epochs and pinned the native palm-query ABI. The current single-consumption presentation path completed a sustained headset run without the earlier compositor error-108 regression.

Important remaining gates are deliberately separate: short-range physical X/Z wall/slide comfort with the current rejected-direction prediction filter; low-ceiling stand recovery and tracked-Y correlation; constrained `0.5 m/s` Push/Move validation; tracking-world-yaw and final bob behavior; mirror/focus; interaction lifecycle; and the real-process no-write palm query before collision-resolved gameplay palms are connected.

See [Audit status](docs/AUDIT_STATUS.md) for the finding-by-finding reconciliation and [Current implementation plan](docs/IMPLEMENTATION_PLAN.md) for the active priority order.

### Requiem

Requiem will reuse validated game-neutral policy while repeating exact-build research wherever Black Plague-specific RVAs, layouts or native state contracts cannot safely transfer.

## Documentation

### Start here

- [Current audit reconciliation](docs/AUDIT_STATUS.md) — what changed after the Astra audit and what is actually still open.
- [Current implementation plan](docs/IMPLEMENTATION_PLAN.md) — ordered remaining work and validation gates.
- [Design decisions and invariants](docs/DESIGN_DECISIONS.md) — boundaries that should not be reopened without contradictory evidence.
- [Codex / Sol implementation prompt](docs/CODEX_IMPLEMENTATION_PROMPT.md) — continuation objective that begins from the actual current HEAD.
- [Complete Astra High audit](docs/audits/ASTRA_HIGH_AUDIT.md) — preserved historical diagnosis, six-phase plan, validation protocol and original Sol prompt.

### Project references

- [Roadmap](ROADMAP.md)
- [Architecture](ARCHITECTURE.md)
- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)
- [Black Plague probe notes](docs/BLACK_PLAGUE_PROBE.md)
- [Black Plague spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md)
- [Headset validation checklist](docs/VR_HEADSET_TEST_CHECKLIST.md)

## Design principles

- Preserve demonstrated behavior before refactoring.
- Reuse Overture Rework where it already proves VR behavior; do not reinvent it.
- Keep game-neutral transforms, policies and algorithms in the shared runtime.
- Keep RVAs, layouts, calling conventions, native state classification and engine-phase ownership inside game adapters/backends.
- A binary callsite has one owner; consumers use fan-out/status instead of stacked hooks.
- Unknown executable builds fail closed.
- Track `implemented`, `host-tested`, `live-tested`, `headset-validated` and `supported` separately.
- Do not generalize Black Plague or Overture evidence to Requiem without exact proof.

## Repository layout

```text
src/                    shared runtime, adapters and game backends
products/overture/      Framework-owned Overture source host and packaging
assets/openvr/          shared action manifest and controller bindings
assets/localization/    optional localized game payloads and provenance
manifests/              exact-build identity and research data
tools/                  build, validation and research utilities
tests/                  host-independent and exact-boundary test harnesses
docs/                   architecture, research, validation and audit documentation
```

## Building

Native targets are Windows/x86. Development requires Visual Studio 2022 or Build Tools with **Desktop development with C++** and CMake 3.25+.

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release --output-on-failure
```

For the autonomous Overture product:

```powershell
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
```

Build success is not a headset-validation claim. Follow the project validation documents before promoting a feature state.

## Lineage and license

Penumbra VR Framework is an unofficial community project. It is separate from [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the original Overture-only mod, and from the Overture Rework derived from it.

The Framework is licensed under **GNU GPL v3 or later**. See [COPYING](COPYING) and [THIRD_PARTY.md](THIRD_PARTY.md) for licensing, attribution and component provenance.

## Support

If you want to support continued development, see [Ko-fi](https://ko-fi.com/onitaku).
