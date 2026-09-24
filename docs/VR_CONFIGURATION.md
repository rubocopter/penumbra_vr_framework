# VR configuration

`assets/settings/recommended.json` is the machine-readable source for the
maintainer-tested baseline. This document explains those values and separates a
recommended game profile from personal VR calibration.

Edit a game's `settings.cfg` only while the game is closed; HPL1 can overwrite
live changes on exit.

## Recommended game settings

These are the current clean-profile recommendations for all three games. Display
resolution, key bindings, current map and save-state values are deliberately not
part of the preset.

| Setting | Overture | Black Plague | Requiem |
| --- | --- | --- | --- |
| Language | `Espanol.lang` | `Espanol.lang` | `Espanol_exp.lang` |
| `Graphics/LimitFPS` | `false` | `false` | `false` |
| `Screen/Vsync` | `false` | `false` | `false` |
| `Graphics/FSAA` | `0` | `0` | `0` |
| `Graphics/MotionBlur` | `false` | `false` | `false` |
| `Graphics/DepthOfField` | `false` | `false` | `false` |
| `Graphics/NoiseFilter` | `false` | `false` | `false` |
| `Graphics/TextureSizeLevel` | `0` | `0` | `0` |
| `Graphics/TextureAnisotropy` | `16` | `16` | `16` |
| `Graphics/ShaderQuality` | `3` | `3` | `3` |
| `Graphics/Shadows` | `0` | `0` | `0` |
| `Graphics/Bloom` | `true` | `true` | `true` |
| `Graphics/PostEffects` | `false` | `true` | `true` |
| `Graphics/Refractions` | `false` | `true` | `true` |
| `Physics/UpdatesPerSec` | `60` | `60` | `60` |
| Environmental audio | `true` | `true` | `true` |

Overture uses the app-local `OpenAL Soft` device. The observed Black Plague and
Requiem profiles keep `UseSoundHardware=false` and `UseThreading=true`.

### Why these values

- `LimitFPS=false` avoids the legacy 60 FPS render cap competing with VR pacing.
- `Vsync=false` avoids synchronizing the desktop swap in addition to the VR
  compositor.
- Legacy window `FSAA=0` avoids extra memory/GPU cost on top of high-resolution
  eye targets.
- Motion blur, depth of field and noise are disabled for head-tracked clarity.
- Physics remains at 60 updates/s; headset refresh rate is not a reason to alter
  gameplay simulation rate.
- Full-resolution textures, shader quality 3, anisotropy 16 and bloom remain the
  visual baseline. Reduce VR render scale before reducing texture quality when
  performance is insufficient.

## Overture VR reference profile

The maintainer's current tested Overture VR values are:

```ini
[VR]
SettingsVersion=1
MoveSpeed=1.0
MoveDeadZone=0.15
HeightOffset=0.0
TurnMode=Smooth
SnapTurnAngle=90
SmoothTurnSpeed=90
TurnDeadZone=0.20
UIDistance=2.0
UIScale=2.0
CrouchMode=Hybrid
PhysicalCrouchDepth=0.25
SubtitleScale=1.55
Handedness=Right
PlayMode=Standing
PlayerHeight=1.730447
RenderScale=1.0
EnhancedVisuals=true
HRTF=On
```

`Game/RenderToMonitor=false` is the normal-play recommendation. `RenderScale=1.0`
is the quality baseline; `0.75` is the first suggested performance fallback.

Height, handedness, turn mode, UI geometry and subtitle size are a tested
maintainer reference rather than universal values. An installer should offer
those values on a new profile but preserve an existing user's calibration unless
explicitly asked to replace it.

## Black Plague Framework profile

Black Plague uses `%LOCALAPPDATA%\PenumbraVR\settings.ini`. The maintainer's
current effective profile is:

```ini
[VR]
SettingsVersion=1
MonitorMirror=true
Handedness=Right
PlayMode=Standing
PlayerHeight=1.70
MoveSpeed=0.85
MoveDeadZone=0.15
HeightOffset=0.0
TurnMode=Snap
SnapTurnAngle=45
SmoothTurnSpeed=90
TurnDeadZone=0.20
UiDistance=1.75
UiScale=1.0
RenderScale=1.0
EnhancedVisuals=false
CrouchMode=Hybrid
PhysicalCrouchDepth=0.25
SubtitleScale=1.35
HRTF=Auto
```

`MoveSpeed` scales the shared `1.5/2.25 m/s` walk/sprint policy. `UiDistance` is
in metres and `UiScale` changes panel size. `RenderScale` multiplies the OpenVR
recommended per-eye dimensions before allocation fallback.

The Black Plague backend currently consumes handedness, play mode/height, turn
settings, movement/dead zones, height offset, crouch settings, UI distance/scale,
subtitle scale, render scale and HRTF. `MonitorMirror` remains a separate
presentation setting. `EnhancedVisuals` stays in saved profiles but its eye
stage and menu row are disabled until Black Plague's pre-tone lighting response
is calibrated. `SubtitleScale` enlarges the captured native HUD/message surface
and is awaiting headset validation at the Rework-centered UI distance.

The native Black Plague `VR Settings` page and offline editor use this shared
schema. Edit the profile offline with:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --configure-vr black-plague
```

Missing keys use normalized shared defaults; malformed recognized values fail
preflight and out-of-range numeric values are clamped.

## Requiem

Requiem's recommended game-level profile is versioned in
`assets/settings/recommended.json`, but there is no Requiem VR backend yet. The
profile therefore contains no active Requiem VR settings block. Shared VR values
will be adopted only when the backend actually consumes them.

## Installer policy

The future unified installer should treat recommended settings as an optional,
reversible preset. It must back up existing files, preserve unrelated preferences
and avoid silently replacing user-specific calibration.
