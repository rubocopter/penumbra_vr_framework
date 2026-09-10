# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: common runtime, initial Overture backend and Black Plague research backend.**

The framework now owns the complete source/build host needed for Overture under `products/overture`. That host compiles the source-level HPL adapter, `pvr_overture_backend` and shared runtime into the real `Penumbra_vr.exe`; `rubocopter/penumbra_vr_rework@23c890f` is no longer in the build or packaging graph. The adapter owns `cPlayer`/`iCharacterBody`, feet/body queries, native collision updates and jump calls; tracking, calibrated height, seated/standing, yaw/recenter, room-scale reconciliation and fixed-displacement locomotion remain in the runtime/backend. The autonomous Release artifact with SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` has been deployed through the packaged installer and exercised with SteamVR, a real headset and controllers. The first functional pass felt equivalent to the proven Rework build and exposed no evident regression, but it was not an exhaustive feature/hardware test and does not make this a supported release. See [the migration audit](docs/OVERTURE_BACKEND_MIGRATION.md).

The Black Plague integration remains experimental. Native stereo, yaw-aligned rotation tracking, keyboard/mouse input and conservative HMD-aware visibility have been validated in the headset at `3400x3468` per eye. The continuous stereo lifecycle and two-eye schedule have also been exercised in the headset. The optional desktop mirror path exists in code, but it is **not currently a supported or validated feature** and is intentionally excluded from the next gameplay milestones because it has continued to produce problems. Acceptable frame pacing at the target refresh rate also remains open.

OpenVR action polling and an exact-build native input bridge are implemented, together with tracked menus, provisional procedural gloves, controller picking and free-body palm-relative grab/throw. These new gameplay paths are code-tested but are not yet a completed headset-validated Rework gameplay port. Palm collision, articulated mechanisms, tool/light attachment, positional body tracking and Enhanced visuals GPU integration remain pending.

For one-step Black Plague startup use `Start-Black-Plague-VR.cmd`, which launches through Steam and enables VR without manual PID/attach commands. Read [startup instructions and controller status](docs/VR_STARTUP_AND_CONTROLLERS.md); validation states in that document are kept separate from code/test status.

The current Black Plague interaction work also uses the Rework direct physical reach policy of `0.18 m` for the generic prop-pick fallback instead of the native camera-ray distance. The exact-build body/capsule mapping required for room-scale positional translation is not yet established, so positional HMD movement remains disabled rather than being approximated with an unverified collider.

The existing, playable Overture implementation remains in [rubocopter/penumbra_vr_rework](https://github.com/rubocopter/penumbra_vr_rework). Revision `23c890f` is the immutable behavioral/source reference, not a checkout required to build this repository. The Framework contains only the coherent Overture/HPL source host, dependencies, assets and tooling needed by the product; historical, generated and unrelated Rework material was deliberately excluded. The exact migration boundary is recorded in [`products/overture/SOURCE_PROVENANCE.md`](products/overture/SOURCE_PROVENANCE.md).

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
- **Prefer porting proven Rework behavior over reimplementing it from scratch.**
- Require evidence for every signature, RVA, structure offset, and calling convention.
- Do not mark a backend supported until it reaches an in-headset milestone.
- Preserve upstream attribution and licenses for imported or adapted code and assets.

## Intended components

```text
Penumbra VR
├── runtime                 OpenVR, tracking, actions, configuration and logging
├── adapters/hpl1           Reusable HPL1 behavior with no exact-build addresses
├── adapters/overture_source Source-level cPlayer/HPL boundary
├── backends
│   ├── overture            Overture sequencing over the shared runtime
│   ├── black_plague_binary Version-specific binary integration
│   └── requiem_binary      Version-specific binary integration
├── products/overture       Overture/HPL source host, Win32 deps and package scripts
├── launcher/bootstrap      Host identification and backend loading
├── deployment/installer    Detection, backup, deployment and rollback
├── assets/openvr           Shared bindings plus Overture-specific overlay
├── manifests               Exact-build identity and binary-research evidence
├── tools                   Build fingerprinting and research utilities
└── tests                   Host-independent verification
```

These are architectural boundaries; some later-game and installer components remain incomplete as described in the roadmap.

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

Build and package the complete Overture product from this repository with:

```powershell
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
```

This produces `products/overture/build/bin/Release/Penumbra_vr.exe` and the
deployable tree under `products/overture/build/package/Release/PenumbraVR`.
The command does not require a Rework checkout. It requires Visual Studio 2022
with the x86 C++ toolchain; all product source, Win32 link/runtime dependencies,
OpenVR assets and packaging inputs are owned by this repository.

The generated Overture package is an **overlay for a valid retail installation**,
not a standalone game directory. Do not test it by running `Penumbra_vr.exe`
inside the package folder. Run the packaged `Install-PenumbraVR.bat`, which
detects or accepts the retail installation, backs up replaced files and deploys
the overlay into its `redist` tree; then start SteamVR and launch Overture through
Steam. The current deployment path is reversible but is not the future unified
production installer.

OpenVR support remains optional for the root CMake targets. The Overture product
owns its pinned OpenVR 2.15.6 SDK snapshot, which can also configure those targets:

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=.\products\overture\dependencies\openvr-2.15.6
```

For Black Plague, use the developer launcher/probe described in
[docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md). Positional motion and
the desktop mirror should not be treated as working gameplay features merely
because their hooks or command paths exist.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), [docs/VR_CONFIGURATION.md](docs/VR_CONFIGURATION.md), [docs/INSTALLER_DESIGN.md](docs/INSTALLER_DESIGN.md), [docs/REWORK_PORTING_PLAN.md](docs/REWORK_PORTING_PLAN.md), [docs/OVERTURE_BACKEND_MIGRATION.md](docs/OVERTURE_BACKEND_MIGRATION.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime, deployment or hook code.

## Licensing

Penumbra VR is licensed under the GNU General Public License version 3 or any later version; see [COPYING](COPYING). Imported HPL1/Overture source retains its notices, assets and shaders retain their separate terms, and OpenVR remains under Valve's BSD-style license. See [THIRD_PARTY.md](THIRD_PARTY.md) and [`products/overture/SOURCE_PROVENANCE.md`](products/overture/SOURCE_PROVENANCE.md) for component-level provenance.
