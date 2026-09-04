# Changelog

This project is pre-alpha. Entries distinguish implemented infrastructure from
features validated in a headset.

## Unreleased

### Added

- Continuous Black Plague stereo-presentation start/stop lifecycle using the
  already validated yaw-aligned rotation tracking.
- Runtime-derived per-eye resolution with proportional allocation fallback.
- Shared HPL1 camera-matrix transaction adapter.
- Host-independent references and tests for Enhanced visuals v4 calibration and
  the accepted spatial-audio behavior from Overture VR Rework.
- Pure, idempotent x86 PE32 Large Address Aware inspection/transformation with
  unit tests. No installed executable is modified yet.
- Shared OpenVR action manifest and controller bindings, including PSVR2 Sense.
  Runtime action polling and hands are not connected yet.
- Exact-build LAA observations for the installed Black Plague and Requiem Steam
  executables.
- Unified installer transaction and rollback design.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with
  warnings treated as errors.
- Eleven host-independent tests pass in Release and Debug.
- Prior headset validation remains limited to bounded 512x512 stereo sessions:
  static presentation and yaw-aligned rotational tracking passed. Continuous
  runtime-resolution presentation still awaits an in-headset session.

### Changed

- Moved camera override behavior out of the Black Plague backend into the shared
  HPL1 adapter; exact camera offsets remain backend-owned.
- Documented Enhanced visuals, audio, action and tracking provenance against
  `rubocopter/penumbra_vr_rework` revision `23c890f`.
