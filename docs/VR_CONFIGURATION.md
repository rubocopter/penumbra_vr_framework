# VR configuration baseline

This document records the recommended starting configuration for the Penumbra
trilogy. It is a developer baseline, not yet an installer-managed preset. Edit
`settings.cfg` only while the corresponding game is closed: HPL1 writes its
live state on exit and can overwrite changes made while it is running.

The game-independent defaults, ranges, enum values and migration behavior live
in `src/runtime/vr_settings.*`. Black Plague now consumes the input/comfort
subset from the framework INI; Overture and Requiem remain separate integration
work at this stage.

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

## Black Plague framework profile

Black Plague reads this profile from `%LOCALAPPDATA%\PenumbraVR\settings.ini`
when the probe is attached:

```ini
[VR]
MonitorMirror=true
Handedness=Right
MoveSpeed=0.85
MoveDeadZone=0.15
TurnMode=Snap
SnapTurnAngle=45
SmoothTurnSpeed=90
TurnDeadZone=0.20
UiDistance=1.75
UiScale=1.0
RenderScale=1.0
```

`Handedness` accepts `Right` or `Left`; it selects the matching action/UI set,
aim pointer and interaction hand, while flashlight/glowstick use the opposite
hand. `TurnMode` accepts `Disabled`, `Snap` or `Smooth`. Angles and smooth speed
are degrees and degrees/second. `MoveSpeed` scales analog input only and remains
clamped by the native full-stick range; it does not alter physics or game time.
The current local profile uses `0.85` to retest the reported excessive speed.
`UiDistance` is the menu distance in metres, `UiScale` changes the physical
panel size while keeping its aspect ratio, and `RenderScale` scales the OpenVR
recommended per-eye dimensions before the existing allocation fallback. The
same menu geometry is used for drawing and controller-ray hit testing.

Missing keys use normalized Rework defaults. Malformed recognized values make
preflight fail instead of silently starting with a mixed profile. Values outside
the documented runtime ranges are clamped. Changes take effect on the next probe
attachment. `--vr-mirror-on` and `--vr-mirror-off` still update only the mirror
key after a successful live change.

## Requiem

Requiem is configuration-ready only. The shared settings model and schema can be
used as the backend develops, but there is no playable Requiem VR backend yet.
Do not treat the Black Plague profile above as evidence of Requiem support.

The eventual installer should apply these settings through a reversible,
per-game preset, preserve unrelated user preferences, and create a backup
before modifying an existing file.
