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

Black Plague already has native stereo, rotational and positional HMD tracking, OpenVR controller input, tracked menus, provisional hands, native body/collision integration, direct VR locomotion and Rework-derived physical crouch through the exact supported build.

The current body path uses a **single same-tick tracking/body transaction**. PID 25484 (2026-09-16) added positive headset evidence for the current crouch/Y and short-range room-scale path: 12,858 presentation frames with zero stereo failures, three physical crouch entry/exit cycles, final native standing state, `0.961 m` tracked-Y range and 650 accepted direct-locomotion samples. The user reported that the session felt good.

The remaining gates are narrower: capture the Hybrid release-hold interval that PID 25484 missed, exercise blocked-stand/low-ceiling behavior, complete deliberate wall/slide edge checks, validate constrained `0.5 m/s` Push/Move locomotion, cleanly validate the already-integrated palm/interaction path, then continue with tracking-world-yaw/bob and mirror/focus work. PID 28412 live-tested the no-write native palm query, and PID 8644 live-tested the corrected backend-owned palm-shape lifecycle plus the isolated Rework-derived resolver. Exact-image evidence pins Black Plague's independent character and `skip_body` filters. The gameplay path now publishes the held body per hand and feeds resolved palms to visible hands, physical interaction and tools while aim remains raw. `Grab=6` keeps the existing rigid palm-relative free-body path; free bodies entering `Move=2` now preserve the picked contact point and follow the palm through Rework-derived physical force, while jointed/mechanism bodies remain native. These gameplay-interaction changes are host-tested only. PID 23000 exercised them in the headset, but severe frame-rate loss plus a right-controller dropout made that session inconclusive for both performance and interaction feel.

### Requiem

Requiem will reuse validated game-neutral policy while repeating exact-build research wherever Black Plague-specific RVAs, layouts or native state contracts cannot safely transfer.

## Documentation

### Start here

- [Roadmap](ROADMAP.md) — current feature and validation state plus remaining milestones.
- [Architecture](ARCHITECTURE.md) — ownership boundaries and integration model.
- [Design decisions and invariants](docs/DESIGN_DECISIONS.md) — boundaries that should not be reopened without contradictory evidence.
- [Headset validation checklist](docs/VR_HEADSET_TEST_CHECKLIST.md) — current Black Plague live/headset gates.
- [Operational handoff](docs/internal/CODEX_HANDOFF.md) — detailed current engineering checkpoint.

### Project references

- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)
- [Black Plague probe notes](docs/BLACK_PLAGUE_PROBE.md)
- [Black Plague spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md)
- [Historical Astra High audit](docs/audits/ASTRA_HIGH_AUDIT.md)

## Next steps

1. Close the remaining focused Black Plague posture edges: Hybrid release-hold capture and blocked-stand/low-ceiling recovery.
2. Headset-validate the host-tested gameplay palm path and the distinct `Grab=6` / free-body `Move=2` routes after a clean reboot, including representative small props, long wooden boards/bars, both hands, wall/slide contact and one native jointed mechanism; compare palm-collision on/off performance.
3. Validate the remaining constrained locomotion, deliberate wall/slide, recenter/tracking-loss and yaw/bob comfort gates without reopening the body owner that already has headset evidence.
4. Continue interaction work through native mechanism state, tool/light geometry, UI/HUD coverage and representative chapter testing.
5. Start Requiem exact-build research only after the Black Plague backend has a stable playable-alpha boundary.

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
