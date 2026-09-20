# Changelog

This file records project-level milestones rather than day-by-day debugging
chronology. Intermediate implementation history remains available in Git.
Product-specific Overture release history is retained under
`products/overture/docs/RELEASES.md`.

## Unreleased — Framework integration line

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

### Deployment and data

- Added exact canonical/LAA fingerprints for Black Plague and Requiem.
- Added shared OpenVR actions and eight controller binding graphs.
- Added Framework-owned Black Plague/Requiem Spanish localization payloads with
  hashes and attribution.
- Added `assets/deployment/manifest.json` as the shared deploy/installer payload
  contract. The Black Plague development deploy now covers bootstrap/probe DLLs,
  OpenVR assets, hand texture, Spanish localization and generated HRTF config.
- Added `assets/settings/recommended.json` with maintainer-tested per-game
  configuration data for future reversible installer presets.

### Repository/documentation maintenance

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
