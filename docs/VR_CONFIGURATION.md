# VR configuration baseline

This document records the recommended starting configuration for the Penumbra
trilogy. It is a developer baseline, not yet an installer-managed preset. Edit
`settings.cfg` only while the corresponding game is closed: HPL1 writes its
live state on exit and can overwrite changes made while it is running.

The game-independent defaults, ranges, enum values and migration behavior live
in `src/runtime/vr_settings.*`. This document describes the user-facing policy;
configuration-file storage and game-specific application remain separate work.

## Common baseline

Use these values in the `Graphics` and `Screen` sections for all three games:

| Section | Key | Value | Reason |
| --- | --- | --- | --- |
| `Graphics` | `LimitFPS` | `false` | Avoid the legacy 60 FPS render cap; VR frame pacing is owned by the runtime. |
| `Screen` | `Vsync` | `false` | Avoid synchronizing the desktop swap in addition to the VR compositor. |
| `Graphics` | `FSAA` | `0` | The high-resolution eye targets already provide substantial spatial sampling; legacy window multisampling adds memory and GPU cost. |
| `Graphics` | `MotionBlur` | `false` | Preserve head-tracked clarity and reduce discomfort. |
| `Graphics` | `DepthOfField` | `false` | Avoid a focus effect that does not follow the player's real accommodation. |
| `Graphics` | `NoiseFilter` | `false` | Preserve clarity and avoid an unnecessary full-screen effect. |

Keep `Physics/UpdatesPerSec` at `60`. The physics rate is not the headset refresh
rate, and raising it without simulation validation can change gameplay.

The current balanced visual baseline keeps full-resolution textures,
`ShaderQuality=3`, anisotropic filtering at `16`, bloom, post effects and
refractions where the game already enables them. Shadows remain at `0` while
frame pacing is still being optimized. If performance remains below the
headset refresh rate, reduce VR render scale before reducing texture quality.

## Overture Rework

Overture Rework has additional settings in the `VR` section. The recommended
starting points are:

- `RenderScale=1.0`; use `0.75` as the first performance fallback.
- `EnhancedVisuals=true` to retain the Rework lighting, HDR/tonemapping and
  related visual path.
- `HRTF=On` for headphones when OpenAL Soft is the selected device.
- `Game/RenderToMonitor=false` for normal play. Set it to `true` only when a
  desktop mirror is needed for menu operation, capture or spectators.

Locomotion mode, turn mode, handedness, player height, UI distance and UI scale
are comfort or calibration choices and should not be overwritten by a generic
performance preset.

## Black Plague and Requiem

The binary backends do not yet read a persistent VR section from each game's
configuration. Black Plague's launcher stores the monitor-mirror preference in
`%LOCALAPPDATA%\PenumbraVR\settings.ini`; `--vr-mirror-on` and
`--vr-mirror-off` update it after a successful live change, and `--start-vr`
applies it automatically. A missing file or key defaults to `false`. Mirroring
retains a third desktop world pass, so it should be enabled only while needed.
Requiem is configuration-ready only; its VR backend is not playable yet.

The eventual installer should apply these settings through a reversible,
per-game preset, preserve unrelated user preferences, and create a backup
before modifying an existing file.
