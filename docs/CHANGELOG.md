# Changelog

This file records project-level milestones rather than day-by-day debugging
chronology. Intermediate implementation history remains available in Git.
Product-specific Overture release history is retained under
`products/overture/docs/RELEASES.md`.

## 1.0.4 — 2026-10-04

- Remove Overture's v1.0.3 in-game SteamVR binding shortcut after an external
  Valve Index report of a crash immediately before the main menu. The inactive
  stick fallback and corrected controller defaults remain intact.
- Add a Black Plague adapter for native `Push=1` interactions. VR-origin Push
  now follows the tracked palm and applies the proven Rework 300 N horizontal
  force while preserving Black Plague's native state lifecycle.
- Keep Black Plague/Requiem general behavior unchanged. Both v1.0.4 fixes are
  host-tested and require the focused external headset retest before promotion.

## 1.0.3 — 2026-10-03

- Preserve Black Plague's calibrated eye height across turning before the next
  native body sample; suppress only prediction from the previous yaw basis.
- Recover each missing Overture stick action independently through its existing
  legacy analog path, preserving active custom actions and excluding raw clicks.
- Correct Index light/interaction overlap, grip/trackpad force-click semantics
  and pause placement; remove invalid/duplicate Touch/Pico recenter defaults and
  WMR's mirrored pause/recenter overlap. Use declared SteamVR tip pointing poses.
- Add shortcuts to the active SteamVR bindings in Overture controls and Black
  Plague VR settings, plus a checked reference for all eight controller defaults.
  These changes are host-tested; hardware acceptance remains separate.

## 1.0.2 — 2026-10-03

- Corrected Valve Index thumbstick components in all three games' bundled
  defaults, including both handedness layouts and gameplay/UI stick clicks.
- Added the generated/distributed binding regression and published a new
  installer, matching source, checksums and scoped host-validation record.
- Added saved-binding update guidance and a hardware procedure for the
  unresolved index/pinky finger mismatch. See [patch notes](releases/1.0.2.md).

## 1.0.1 — 2026-10-03

- Corrected Overture acquisition of multi-body mechanisms: a sibling body from
  the same entity no longer hides the selected wheel/handle. The maintainer
  installed the exact 1.0.1 installer and accepted an Overture headset session
  including the initial drawer and repeated crank/mechanism interaction.
- Corrected nearest-overlap ranking: an occluded target can no longer suppress
  another accessible drawer/prop or inventory item. Physical reach, collision
  dimensions and sight barriers retain their established limits.
- Oculus Touch pointing uses SteamVR's tip pose in both binding trees; Quest 3
  alignment remains a focused hardware retest.
- Setup shows a bilingual preparation window during payload extraction and
  checksum validation, retained until the installer window appears. Disabled
  redundant single-file compression so the already zipped payload does not
  delay entry into the bootstrap UI.
- Installer discovery, preflight, installation, maintenance and verification
  run in background PowerShell runspaces with an animated waiting dialog.
  Controls and outcomes stay on the UI thread; checksums, all-root preflight,
  transactions, rollback and partial-failure reporting remain enforced.
- Black Plague render-thread telemetry drops and counts samples when its
  diagnostic lock is busy, avoiding a VR-frame stall.
- Updated executable/installer/package identity to 1.0.1 and added
  [patch notes](releases/1.0.1.md). Published the accepted candidate unchanged,
  with matching source, hashes, build report and a separate acceptance record.
  Broader headset and independent clean-machine acceptance remain separate.

## 1.0.0 — 2026-09-29

First public Penumbra VR Framework release for Overture, Black Plague and
Requiem. The published installer is the exact v1.0.0 artifact promoted from the
recorded release candidate after automated package/topology acceptance and the
maintainer's repeated real install/uninstall and game-entry checks.

### Trilogy framework

- Overture was moved into a Framework-owned source/build/package host while
  retaining Rework `23c890f` as the proven behavioral reference.
- Shared runtime policy now covers tracking transforms, settings, OpenVR action
  input, locomotion/accepted-motion semantics, hand/contact and interaction
  policy, haptics, render-target policy and reusable audio/visual calibration.
- The project now treats better game-neutral behavior discovered in later
  backends as eligible for promotion back into shared runtime and the
  Framework-owned Overture product after cross-consumer validation.

### Black Plague

- Added an exact-build x86 backend for the allowlisted Steam executable with
  fail-closed signatures, singular hook ownership and resident-DLL lifecycle.
- Added native tracked stereo rendering, OpenVR compositor/input integration,
  HMD-relative locomotion, room-scale/body reconciliation, crouch/tracked Y,
  hands, finger articulation, palm collision, physical interaction and haptics.
- Added target-specific adapters for free bodies and verified native
  slider/hinge/mechanism ownership while keeping unknown mechanisms fail-closed.
- Added tracked game UI paths for inventory/notebook and a captured native
  gameplay HUD/subtitle surface pending final headset validation.
- Added the native `VR Settings` page plus shared settings storage.
- Added HRTF startup policy and exact-build OpenAL/EFX environmental reverb/bus
  trim integration; distance/occlusion low-pass remains open.
- Added the transferable Enhanced Visuals eye stage with graceful GL fallback;
  headset validation and Overture-specific material behavior remain separate.
- Added a managed `alut.dll` bootstrap for normal Steam **Play**. The installed
  path has reached gameplay VR on the allowlisted build. Current corrections to
  presentation pacing and packaged hand texture still require focused headset
  regression coverage.
- Added map-start yaw compensation, per-eye particle refresh and stronger
  body-lifecycle guards from current exact-build evidence; these remain at their
  documented validation levels until headset retest.
- Ported fresh surface-contact Grab from Requiem while preserving native/stale
  fallback, and independently mapped the native full-eye refraction-copy
  boundary. A Black Plague headset run confirms chair grips from different
  points and correct special effects in the observed scene. Ventilation-wall
  mini-hops remain an open comfort defect.

### Requiem

- Added an exact-build development backend using the shared OpenVR session,
  tracked stereo, Sense input, physical locomotion/crouch, hands and spatial UI.
- Reused the common hand/contact behavior through Requiem-owned native Grab,
  Move and Push boundaries. Headset tests validate representative free-body
  Grab with carry/snap/release and jointed Move through the monolith puzzle.
- Corrected a Push acquisition check that ran before Requiem published its new
  player state. In the subsequent headset test, the user moved and tipped a
  cube to solve a puzzle without problems; the log confirmed VR acquisition
  and hand force.
- Validated the first level transition and exact-build full-eye portal
  refraction correction. Tools now share the resolved palm with the visible
  hand; final native-size flashlight direction and beam alignment have headset
  acceptance. Broader progression, intermittent startup failure and known
  contact imperfections remain open.

### Deployment and data

- Added exact canonical/LAA fingerprints for Black Plague and Requiem.
- Added shared OpenVR actions and eight controller binding graphs consumed by
  all three games, with metadata guards and source-hash checks through package,
  installation and repair. Hardware validation and compatibility layout limits
  remain separate.
- Added Framework-owned Black Plague/Requiem Spanish localization payloads with
  hashes and attribution.
- Added `assets/deployment/manifest.json` as the shared deploy/installer payload
  contract. The shared Black Plague/Requiem deploy covers bootstrap/probe DLLs,
  OpenVR assets, hand texture, both Spanish localizations and generated HRTF config.
- Added the unified bilingual `PenumbraVR-Setup-1.0.0.exe` with Steam/manual
  discovery, exact-build validation, optional components/settings, transactional
  install/update/repair/recovery/removal and exact restore for Overture and the
  shared Black Plague/Requiem root.
- Published matching source, checksums and build report. Automated topology A–F
  coverage validates isolated and combined game layouts; independent clean-PC
  and non-Sense hardware coverage remain intentionally separate evidence.
- Added `assets/settings/recommended.json` with maintainer-tested per-game
  configuration data for future reversible installer presets.

### Repository/documentation maintenance

- Removed the session-oriented closure status document; current work order, capability state and build evidence now live only in their owning documents.
- Reduced installer documentation to its durable transaction contract and current prototype baseline.

- Consolidated durable state into the architecture, parity, supported-build,
  roadmap, configuration, installer and current headset-validation documents.
- Removed versioned debugging chronologies whose conclusions are already encoded
  in code, tests, manifests or the durable state documents.
- Temporary video frames, screenshots, disassemblies, research checkouts and
  other consumed evidence are local/ignored and may be deleted once their result
  has been recorded durably.

## Overture v0.1.0 reference

The original Overture VR Rework `v0.1.0` remains the proven public baseline and
the behavioral reference used by the Framework. Its release notes, validation
record and product-specific history remain in `products/overture/docs` and in the
original `rubocopter/penumbra_vr_rework` repository.
