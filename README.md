# Penumbra VR Framework

**One VR framework for the Penumbra trilogy.**

Penumbra VR Framework is an open-source PCVR project for **Penumbra: Overture, Black Plague and Requiem**. The project keeps game-neutral VR behavior in a shared runtime while each game retains the narrow source or exact-build integration it actually requires.

> **Status: pre-alpha.** Overture is Framework-hosted and has completed an initial headset validation. Black Plague has a working VR research/gameplay backend and is in active comfort, interaction and lifecycle validation. Requiem is planned. There is no supported public release build yet.

## Current state

| Game | State | Integration |
| --- | --- | --- |
| **Penumbra: Overture** | Framework-hosted; initial functional headset pass completed | Source-level HPL1 product owned by this repository |
| **Penumbra: Black Plague** | Active development and validation | Exact-build x86 binary backend/probe |
| **Penumbra: Requiem** | Gameplay deferred; exact-build identity/reconnaissance exists | Future exact-build binary backend |

### Overture

The Framework builds and packages its own `Penumbra_vr.exe`. The earlier [Penumbra: Overture VR Rework](https://github.com/rubocopter/penumbra_vr_rework) remains the behavioral reference, but it is not a build dependency. The autonomous Overture product is also used as a regression gate for shared-runtime extraction.

### Black Plague

Black Plague already has native stereo, rotational and positional HMD tracking, OpenVR controller input, tracked menus, native body/collision integration, direct VR locomotion, tracked physical crouch and the Rework-derived palm/grab interaction stack on the exact research build. PID 25484 (2026-09-16) remains positive headset evidence for crouch/Y, direct locomotion and the then-current short-range room-scale comfort filter.

The **current** headset candidate is newer than PID 25484 and therefore remains host-tested. After PID 26940 exposed black/rainbow imported hands and physical-only wall-contact mini-jumps, the renderer now isolates the additional fixed-function GL state, the backend suppresses only native step climbing on physical-HMD-only ticks, and horizontal presentation now uses the latest reconciled body anchor without raw between-tick X/Z prediction. The same candidate adds Rework-derived direct hand nudge plus shared haptic policy for pickup/drop, tracked UI select and successful hand contact, with per-event telemetry. Release and SDK-less Release both pass 35/35 CTest, metadata validation passes, and the initialized Black Plague exact-image verifier passes.

The next evidence gate is one clean combined headset run covering stable textured/finger-articulated hands, palm contact and direct nudge, `Grab=6` plus opportunistic free `Move=2`, gentle physical wall pressure without vertical bounce, normal stick stair/ledge stepping, short room-scale/crouch checks and exercised haptic submissions. Separate open parity work then remains for Hybrid release-hold/blocked stand, constrained Push/Move locomotion, magnetic item acquisition, native jointed mechanisms, final tool/light geometry, remaining UI/transitions, audio/visual parity and representative chapter validation.

### Requiem

Requiem will reuse validated game-neutral policy while repeating exact-build research wherever Black Plague-specific RVAs, layouts or native state contracts cannot safely transfer.

## Documentation

### Start here

- [Roadmap](ROADMAP.md) — current feature and validation state plus remaining milestones.
- [Trilogy parity plan](docs/TRILOGY_PARITY_PLAN.md) — authoritative Overture → Black Plague → Requiem capability ledger and framework-readiness gate.
- [Architecture](ARCHITECTURE.md) — ownership boundaries and integration model.
- [Design decisions and invariants](docs/DESIGN_DECISIONS.md) — boundaries that should not be reopened without contradictory evidence.
- [Headset validation checklist](docs/VR_HEADSET_TEST_CHECKLIST.md) — current Black Plague live/headset gates.
- [Operational handoff](docs/internal/CODEX_HANDOFF.md) — detailed current engineering checkpoint.

### Project references

- [Supported builds](docs/SUPPORTED_BUILDS.md)
- [Rework porting plan](docs/REWORK_PORTING_PLAN.md)
- [Black Plague probe notes](docs/BLACK_PLAGUE_PROBE.md)
- [Black Plague spatial notes](docs/BLACK_PLAGUE_SPATIAL_NOTES.md)

## Next steps

1. Close the current combined Black Plague body/palm headset candidate and the remaining focused posture/comfort edges without reopening already proven owners.
2. Close interaction parity: magnetic inventory-item acquisition, representative native jointed mechanisms, long-body behavior and definitive tool/light geometry.
3. Close presentation/output parity: inventory/notes/HUD/subtitles, transition comfort, applicable haptics, spatial audio and compatible enhanced-visual behavior.
4. Run representative chapter-level Black Plague validation and close the framework-readiness gate in the [trilogy parity plan](docs/TRILOGY_PARITY_PLAN.md).
5. Make Requiem the third-backend reuse proof: repeat exact-build boundary research, consume the same shared capability families, and validate them independently.

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
