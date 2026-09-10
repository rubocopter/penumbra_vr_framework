# Penumbra: Overture VR Rework

A community-made PCVR rework that brings **Penumbra: Overture** to full room-scale VR with motion-controlled hands, physical interactions, and a complete spatial-audio chain built for horror.

> First stable — current release: **v0.1.0** (2 September 2026). You must own **Penumbra: Overture** on Steam; the original game is not included.

**[Download v0.1.0](https://github.com/rubocopter/penumbra_vr_rework/releases/tag/v0.1.0)** · [Controls and VR settings](docs/INPUT.md)

## Why try it?

- Full room-scale PCVR — walk, crouch, lean, reach
- Motion-controlled hands with animated fingers, collision-constrained palms, and geometry-calibrated equipped objects
- Physical interaction — pick up, throw, nudge, and manipulate objects, doors, drawers, hatches, and mechanisms naturally
- Assisted magnetic acquisition for visible inventory items that are difficult to reach, with occlusion checks and per-hand feedback
- Low-ceiling crouch recovery that preserves the requested physical, button, or hybrid stance until standing is possible again
- **Hear the dark**: binaural HRTF audio, creatures that sound muffled through walls, distant sources that lose their treble, and a mine-gallery reverb that makes corridors breathe
- Standing or seated play, left- or right-handed controls, and height calibration
- Snap, smooth, or physical-only turning plus adjustable movement and crouch modes
- Performance, Balanced, and Quality presets with adjustable render scale
- Optional Enhanced visuals: MSAA 2×, refined ambient/surface lighting, restrained flashlight/glowstick response and sharper dark image treatment; Off by default, switchable in game
- 231 reviewed, bounded-resolution diffuse texture replacements with a texture-only rollback
- SteamVR profiles for PS VR2 Sense, Valve Index, Meta Quest/Touch, Pico, WMR, and HTC Vive
- Room-anchored menus and cinematics with scalable subtitles (no letterboxing)
- Reversible installer that backs up every original file it replaces
- Spanish localization for VR tutorial, menus, and controller notes

## Spatial audio

Penumbra is about what you hear coming. This rework rebuilds the entire acoustic chain on top of a bundled OpenAL Soft runtime, so it behaves identically on every machine:

1. **Positional tracking** — the listener follows your head pose in real time, not last frame's render matrix
2. **Binaural HRTF** — optional headphone spatialization with elevation cues, set to Auto, Headphones, or Off in VR Settings
3. **Distance absorption** — far sources dull progressively even in line of sight
4. **Occlusion** — walls and closed doors muffle treble with a smooth fade, so a creature behind a door sounds held back until you open it
5. **Ambient reverb** — a restrained stone-gallery tail that gives large spaces depth without washing out the mix

## Quick start

This download is a mod overlay for an existing retail installation; it is not a
standalone copy of Penumbra. Running `Penumbra_vr.exe` directly from the extracted
package directory is unsupported and can fail because the retail `redist`
resources and launch context are absent.

1. Download the release `.zip` and extract it completely.
2. Run `Install-PenumbraVR.bat`; it finds the Steam installation and backs up replaced files.
3. Start SteamVR, then launch **Penumbra: Overture** with Steam's normal **Play** button.

Requires Windows 10/11, SteamVR, and a PCVR headset — the Visual C++ runtime ships inside the package since alpha.6.1. To remove the mod, run `Install-PenumbraVR.bat -Restore`; only files owned by the mod are removed, and backed-up originals are restored.

The release includes 231 reviewed model/scenery diffuse
textures. They affect both visual modes and
the monitor. New entries use bounded resizing and high-quality JPEG; the nine
previously tested textures are retained unchanged. Run
`Install-PenumbraVR.bat -SkipTexturePack` to omit them or restore their backed-up
originals while retaining the mod. The pack is **Penumbra Collection 4x AI
upscale**, by **Bret2011** (Nexus uploader **c3pohumancyborgrelations**), crediting
**Frictional Games** for original assets. Redistribution is permitted with
creator credit; see [permissions](docs/TEXTURE_CREDITS.md) and [selection](docs/TEXTURES.md).

## Status

**v0.1.0 — first stable baseline.** The full game is playable in VR and the main tracking, interaction, collision, comfort, UI, audio, install, and shutdown paths have automated and PS VR2 hardware validation. Stable does not mean hardware-complete or an exhaustively tested campaign: PS VR2 Sense remains the only controller family tested end to end on a physical device, while Index, Touch, Pico, WMR, and Vive use bundled profiles that still need device-specific reports. Hand collision uses one approximate palm volume rather than per-finger rigid bodies, the current hand mesh has limited thumb and ring/middle movement, and the original game state still permits only one active grab. See the [release history](docs/RELEASES.md) for build-specific status.

**Framework integration checkpoint.** The autonomous Framework Release
executable with SHA-256
`D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` was
installed through `Install-PenumbraVR.bat` and completed a first functional
SteamVR/headset/controller test. The user perceived behavior equivalent to the
previously tested Rework build and reported no evident regression. This confirms
the source-host/build/deployment integration, not exhaustive coverage of every
feature or controller profile.

**Optional visual improvements.** Off-by-default VR-only
Enhanced visuals combines MSAA 2×, stronger surface relief/masked highlights,
dark image treatment, bounded ambient and restrained held-light/halo response.
The v4 calibration and expanded texture batch received positive PS VR2 feedback.
A 17-minute multi-map session at 90 Hz recorded 1.07% reprojection and a clean
shutdown. This is not a matched On/Off benchmark or a peak-memory measurement.
Off retains the original renderer,
but does not undo installed texture replacements. The release also
contains the two-column VR settings layout, flashlight alignment fix and the
grouped texture selection above. See the [validation record](docs/VALIDATION-v0.1.0.md)
and [remaining work](docs/ROADMAP.md#post-stable-follow-up).

## Documentation

- [Troubleshooting and compatibility](docs/TROUBLESHOOTING.md) — crash dumps and logs, laptop GPUs, audio devices, controller resets, non-Steam installs
- [Controls and VR settings](docs/INPUT.md) — bindings, comfort options, compatibility, and diagnostics
- [VR architecture](docs/VR_ARCHITECTURE.md) — tracking space, world placement, UI, and hands
- [Rendering and lighting research](docs/LIGHTING.md) — HPL1 light path, discarded SSAO measurements, baked-shadow diagnosis, and the current Enhanced visuals A/B
- [Texture selection](docs/TEXTURES.md) — release allowlist, exclusions, memory budget and rollback
- [Stable validation](docs/VALIDATION-v0.1.0.md) — tested installation, session timings, recoverable warnings and evidence limits
- [Input architecture](docs/INPUT_ARCHITECTURE.md) — input flow and the engine/game boundary
- [Roadmap](docs/ROADMAP.md) — current priorities and future work
- [Release history](docs/RELEASES.md) — changes and known limitations by version

## About

This is a community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr). It is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment.

## Development

- This source host is owned by `rubocopter/penumbra_vr_framework`; Rework `23c890f` is its behavioral/source reference and is not a build dependency.
- From the Framework root, `tools/Build-OvertureProduct.ps1 -Configuration Release -Full -Package` validates the project, builds Win32, runs the unit tests and produces the release layout.
- `scripts/generate-bindings.ps1` — checks/regenerates the Overture-specific SteamVR bindings at the Framework root under `assets/openvr/overture`; the build fails if the shipped JSONs drift from the spec.
- `tests/VRTrackingTest` — unit tests for tracking and VR hand-collision policy, executed by every build
- `SOURCE_PROVENANCE.md` — exact imported source-host boundary, exclusions, licenses and generated/source-controlled split

The package is generated at
`products/overture/build/package/Release/PenumbraVR`. The packaged release
places `Install-PenumbraVR.bat` in its root. Use that overlay deployment path for
testing; the package directory itself is not a standalone game installation.

## License

Licensed under GPL v3 or later. See `HPL1Engine/COPYING`,
`PenumbraOverture/COPYING`, `SOURCE_PROVENANCE.md` and the Framework root
`THIRD_PARTY.md` for details and third-party notices.

The externally supplied textures are not relicensed by this statement; their
separate [attribution and permissions](docs/TEXTURE_CREDITS.md) accompany the package.
