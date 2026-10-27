# Changelog

This file records project-level milestones rather than day-by-day debugging
chronology. Intermediate implementation history remains available in Git.
Product-specific Overture release history is retained under
`products/overture/docs/RELEASES.md`.

## 1.0.13 — 2026-10-27 — maintenance candidate

- Restore bounded native Normal Item pickup for Requiem level08's orb, keeping
  native reach/eligibility and independent head/controller solid visibility.
- Route active Requiem ScreenGui through the existing VR UI context and native
  cGui pointer/button boundaries; preserve native keypad validation and exit.
- Release UI select/back clicks on focus/tracking loss. Preserve all preceding
  throwing, lever, movement and native interaction corrections.
- Publish as a pre-release with both level08 changes pending headset validation.
  The developer has not tested this version in-game or in a VR headset; level08
  completion is not demonstrated. See [release notes](releases/1.0.13.md).

## 1.0.12 — 2026-10-27

- Correct Requiem's native Push/free-Move assisted throws to use the current
  holding-controller aim; preserve native launch profiles, reject stale aiming,
  and keep cancellation/ordinary release separate from B / Examine launch.
- Consume BP's proven hinge centre-of-mass velocity correction from shared
  runtime in Requiem and Framework Overture, retaining native constraints.
- Restore Requiem's dedicated native head-to-visible-palm area query and use
  common-origin area/contact priority. Keep physical reach and puzzle guards.
- Preserve all preceding maintenance fixes, including drawer endpoints, input,
  ladder and throwing lifecycles. Publish with automated host evidence only:
  the developer has not tested this version in-game or in a VR headset.
- Water recovery, the SDL startup crash and actual progression acceptance remain
  open. See [release notes](releases/1.0.12.md).

## 1.0.11 — 2026-10-26

- Recognize current interrupted-install journals and safely restore deleted
  preferences; reject directory/junction conflicts and verify ALUT before
  retiring its backup. Keep selected languages usable when uninstall removes
  an owned translation, preserving custom preferences and rollback.
- Keep Overture's acquiring interaction hand stable, reconstruct individual
  binding releases, align alternate UI pointer geometry/buttons and use the
  current head pose for body-independent menu/pause recenter.
- Preserve native occlusion independently for Requiem hand rays; drain native
  interaction ownership and destroy palm shapes on their owning game thread
  before removing hooks. Reset turn state only on the native update owner.
- Include native level05 ScriptArea arbitration and bounded suspension/restoration
  of held drawer endpoint stickiness. Add physical capsule-height diagnostics
  for the unresolved level04 water report; no buoyancy/gravity workaround.
- Fix Setup extraction with trailing destination separators and the Overture
  stale-package error. Add actual-consumer and filesystem regressions.
- Publish all current maintenance changes with host evidence. New headset,
  level03/05 progression, water recovery and SDL-crash acceptance remain open.

## 1.0.10 — 2026-10-25

- Correct Requiem's assisted Grab throw to follow the holding controller aim,
  preserving native CanBeThrown, mass profile, impulse and state/script order.
- Add independent tracked-head Examine/ladder selection and action-time
  Examine refresh while preserving physical hand selection, native range,
  description-once policy, solid occlusion and ladder activation guards.
- Guard gesture release against stale tracking references/epochs, focus and
  UI loss; retain Rework's velocity gains/caps. Remove unused empty history.
- Add production consumer regressions and exact-build evidence. Publish for
  feedback; level03 progression and headset aiming/cue usability remain pending.

## 1.0.9 — 2026-10-23

- Share the normal 3/4.5 m/s walk/sprint profile across the trilogy and keep
  metric stick travel independent of the physical HMD displacement bound.
- Correct Requiem's final held-jump query and inactive-player guard. Adapt
  Overture's normal jump envelope to headset cadence, merge native keyboard/VR
  Jump ownership and preserve native release/repress and script/solver handoff.
- Drive BP/Requiem positional audio from the reconciled central head pose with
  native listener/context fallback and restoration.
- Limit accepted native uphill advance to the ray-measured surface rise and
  preserve native collision selection and single physics-update ownership.
- Suspend BP slider endpoint stickiness only while VR owns the held drawer;
  preserve joints, limits, scripts and restoration on release.
- Add actual-consumer, native gateway and real OpenAL/Newton host regressions.
  Publish for user feedback with new live/headset acceptance explicitly pending.

## 1.0.8 — 2026-10-16

- Fix Black Plague physical target filtering, native Examine/SaveArea selection
  and physical-contact distance checks while retaining occlusion and native
  script/range ownership.
- Support connected articulated Grab bodies and world-anchored puzzle arms
  through their native joints. Restore native enabled/active lifecycle, preserve
  authored masses/limits and keep bucket handles coupled to their containers.
- Remove BP's camera-directed extra Push-release impulse from VR-owned release;
  correct native hinge velocity for offset centers of mass so wall levers can
  cross their authored puzzle activation threshold.
- Restore fixed computer-terminal activation at native range, tracked HPL GUI
  cursor movement and popup button press/release. Add focused route diagnostics.
- Center GameMessage/Examine text as a block and double its font/spacing while
  preserving native wrapping metrics, fades and shadows. Mask equipped light
  models during menu rendering without changing equip/light state.
- Include the host-tested physical-only motion-attribution and current step-ray
  normal boundary corrections; full low-obstacle/leaning parity remains open.
- Record scoped maintainer acceptance of puzzle/prop, PC and text corrections.
  Menu light visibility, flashlight attachment and broader contact still require
  focused validation. Overture/Requiem gameplay is unchanged in this release.
- Publish an updated unified installer with matching committed source,
  dependency sources, checksums and build provenance.

## 1.0.7 — 2026-10-10

- Recover missing digital release edges in the BP/Requiem shared input router
  when an individual binding becomes inactive or omits its change flag. Preserve
  context/hand entry suppression so menu changes cannot create unintended input.
- Retain the configured movement dead zone across temporary focus/input loss;
  explicit resets retain their default-setting behavior.
- Reject non-finite analog axes, normalize non-finite dead-zone settings and
  bound finite axes before radial rescaling. Reuse validated grip-pose flags in
  controller freshness checks.
- Add regressions for lost release edges, context/hand transitions, transient
  dead-zone preservation and invalid/overflowing analog data.
- Run isolated installer configuration, prerequisite, release-identity,
  bilingual UI-state and diagnostics/privacy fixtures in Windows CI.
- Record the maintainer's acceptance of the installed trilogy checklist with
  exact local runtime hashes; update current capability/configuration/validation
  documentation without transferring that evidence to unrelated devices.
- Publish the unified installer, matching committed source, dependency sources,
  SHA-256 checksums and build report. Requiem's intermittent startup cause remains
  unresolved despite the successful reported startup checks.

## 1.0.6 — 2026-10-08

- Place English/Spanish VR Settings at the same first-level position in the
  main and pause menus of all three games; add Requiem's native page with live
  settings, persistence and height calibration.
- Enable BP physical vice progression and easier locker acquisition; preserve
  physical-contact step guards while admitting eligible dynamic obstacles when
  bounded joystick intent is combined with HMD motion.
- Correct Requiem intro height without the inventory workaround, refresh
  post-physics slope height, track fullscreen UI and consume persisted controls.
- Match Overture logic cadence to the HMD display frequency with a 90 Hz
  fallback, and include both asymmetric eyes in shared visibility.
- Publish matching installer/source/checksums and scoped validation records.

## 1.0.5 — 2026-10-04

Consolidated update replacing the v1.0.1–v1.0.4 release downloads.

- Correct Valve Index thumbstick paths and button/force-click conflicts in both
  handedness layouts; align pointing poses and generate an eight-profile controls
  reference. BP VR settings can open the active SteamVR binding editor.
- Improve Overture drawer/prop/mechanism visibility ranking, independently recover
  inactive stick actions, release input on focus loss, and discard stale aim poses.
  Remove the new Controls-menu binding shortcut after the pre-menu crash report.
- Preserve BP calibrated eye height while turning and add tracked-palm native Push
  with HMD-relative stick projection, retaining native lifecycle/permission gates.
- Remove per-line disk flushes and excessive presentation logs, and avoid waiting
  on busy render telemetry. Preserve held BP objects through long ticks/stale
  presentation references while retaining real release/focus/tracking guards.
- Pass bounded BP hand-contact delta velocity directly to Newton without the
  extra mass multiplier; real-DLL regressions cover 1, 3, 20 and 100 kg bodies.
- Show installer preparation/progress and run long operations in background
  runspaces while retaining transactions, settings preservation and error reporting.
- Maintainer feedback accepts preceding local Overture gameplay and BP tutorial
  pacing. Native collision recoil remains unresolved; the shelf launch when using
  the mixed tutorial chemical is the original map script's intended behavior.
- Publish an updated installer, matching source, checksums and build provenance.
  Prior v1.0.1–v1.0.4 acceptance/validation records retain their scoped hashes;
  publication does not transfer them to the new artifact.

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
