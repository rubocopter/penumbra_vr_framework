# Overture source-host provenance

## Purpose and baseline

`products/overture` is the Framework-owned source/build host for the Overture
product. Its migration baseline is `rubocopter/penumbra_vr_rework` commit
`23c890f7dbd06b939be9951d282e6e948d9a6623` (`v0.1.0`). The six-file consumer
integration delta proven in the earlier Rework-hosted checkpoint was applied to
the imported `Player`, `ButtonHandler` and project boundary, then rebased to
Framework-local paths. The Rework checkout is reference evidence only and is
not read by compilation, validation or packaging.

This is a scoped source snapshot rather than Git-history transplantation. The
original copyright headers and license files remain with their components.
Behavioral derivation is additionally recorded in the root `THIRD_PARTY.md` and
`docs/OVERTURE_BACKEND_MIGRATION.md`.

## Coherent product boundary imported

| Local component | Why it is required | Ownership/classification |
|---|---|---|
| `PenumbraOverture` source and `Penumbra.vcxproj` | Builds the actual game executable and owns game state, UI, entities and the `cPlayer`/`cButtonHandler` call sites | Upstream Overture plus proven VR modifications; game-specific product host |
| `HPL1Engine/include`, `HPL1Engine/sources` and `HPL.vcxproj` | Supplies the modified HPL1 renderer, OpenVR/input, tracking, physics and resource interfaces used by Overture VR | Upstream HPL1 plus proven Rework VR modifications; engine boundary |
| Required HPL shaders and flashlight resource | Runtime inputs referenced by the executable/package and by the shader gate | Product resource overlay; shader and asset terms retained separately |
| `OALWrapper` source and project | Builds the audio wrapper linked by the game | Frictional Games component |
| `dependencies/include`, Win32 libraries/DLLs and OpenVR 2.15.6 Win32 SDK | Makes the three legacy Win32 projects link and makes the package runnable | Pinned third-party build/runtime snapshot |
| `data/config`, `data/maps`, `data/models`, `data/textures` | Product-owned configuration and modified/runtime resource overlay | Required Overture package resources |
| `assets/openvr/overture` (at repository root) | Preserves the proven Overture-specific action and controller mappings | Source-controlled product overlay, consumed by package scripts |
| `scripts`, `tests/VRTrackingTest`, product docs and `PenumbraVR.sln` | Validation, build, packaging, deployment input and legacy host reference tests | Framework-owned build/package infrastructure, derived from the proven pipeline |

## Framework bridge and de-duplication

The final compile path is:

```text
products/overture/PenumbraOverture (cButtonHandler / cPlayer)
    -> src/adapters/overture_source/OvertureSourceIntegration
    -> src/backends/overture/OvertureBackend
    -> src/runtime (tracking space, settings, input intents and locomotion)
```

`Player.cpp` and `ButtonHandler.cpp` retain only their game/HPL responsibilities
around that boundary: updater/lifecycle facts, player-state gates, crouch,
footsteps, entities, native body collision calls and jump calls. Their earlier
inline copies of turn/play-mode calibration, head-relative stick movement and
room-scale reconciliation were removed when the bridge was introduced. The
proven constants and sequencing therefore have one active owner in the
Framework runtime/backend; this source-host migration did not retune them.

`src/adapters/overture_source/PenumbraVR.Framework.Overture.props` imports the
adapter, backend and required runtime translation units using a root calculated
from the property-sheet location. `Penumbra.vcxproj` has no sibling-checkout
path. Debug disables legacy minimal rebuild only for this C++20 consumer because
MSVC rejects `/Gm` together with `/std:c++20`.

## Source-controlled versus generated

Source-controlled inputs are the source/projects, required resource overlays,
Win32 dependency snapshot, OpenVR JSON, validation/package scripts, tests and
the license/provenance documents described above.

Generated material lives below `products/overture/build` and is ignored:
object files, PDBs, static libraries, executables, shader compiler temporaries,
fingerprint stamps, test binaries, package staging and `SHA256SUMS.txt`. A clean
checkout regenerates these outputs with `tools/Build-OvertureProduct.ps1`.

## Whitespace policy for imported snapshots

The imported `PenumbraOverture`, `HPL1Engine`, `OALWrapper`, `dependencies` and
`data` trees deliberately preserve their inherited formatting.  Their scoped
entries in the repository-root `.gitattributes` disable only Git's
`blank-at-eol`, `blank-at-eof` and `space-before-tab` diagnostics; they do not
rewrite content or change line-ending policy.

The exception does not cover Framework-owned source, documentation, assets,
scripts, tests, projects or product documentation.  Those paths keep Git's
default whitespace checks, so `git diff --check` remains a reproducible gate
for all Framework-authored changes without requiring a mass normalization of
the imported snapshot.

## Deliberately not migrated

- Rework's `.git`, `.github`, release history and repository administration:
  historical context, not product inputs.
- Existing `build` outputs, logs, deployed/retail files and local caches:
  generated or machine-specific and reproducible from the Framework.
- HPL tools, editors, samples, standalone engine tests and unrelated assets:
  no project or package reference from the Overture product.
- macOS/Linux dependency trees and obsolete Visual Studio/CMake variants:
  the current product target is Win32 with the three selected v143 projects.
- Texture authoring/download/audit utilities not called by the build/package
  gate: useful historical maintenance tools, but not required to reproduce the
  selected source-controlled texture set.
- Rework's `data/vr` location: the exact product JSON was moved to
  `assets/openvr/overture`, so retaining another live copy would create drift.

## Licenses and release caveat

- Overture and HPL1 retain GPLv3 copies in `PenumbraOverture/COPYING` and
  `HPL1Engine/COPYING`.
- HPL assets and shaders retain `HPL1Engine/LICENSE-assets` (CC BY-SA 3.0) and
  `HPL1Engine/LICENSE-shaders` (zlib), plus `NOTICE`.
- OALWrapper retains its zlib `LICENSE`.
- OpenVR retains Valve's BSD 3-clause `LICENSE`.
- OpenAL Soft's COPYING/readme accompany its imported runtime DLL.
- External texture permissions and attribution remain in
  `docs/TEXTURE_CREDITS.md` and the adjacent policy/audit documents.

The package script carries the principal HPL/OAL/OpenVR/OpenAL notices into the
test package. The inherited legacy Win32 dependency bundle has mixed licenses
and does not contain a normalized standalone license file for every binary;
that completeness check remains required before a public production release.
It does not affect build independence and is not treated as an undocumented
external dependency. This provenance record is engineering documentation, not
legal advice.
