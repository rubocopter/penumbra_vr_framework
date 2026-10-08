# Penumbra VR Framework

![Penumbra VR Framework — Overture, Black Plague and Requiem](assets/banner/penumbra-vr-framework.png)

<p align="center">
  <a href="https://github.com/rubocopter/penumbra_vr_framework/releases/tag/v1.0.6"><img alt="Get the latest release: v1.0.6" src="https://img.shields.io/badge/latest%20release-v1.0.6-brightgreen?style=flat-square"></a>
  <a href="COPYING"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
  <a href="https://ko-fi.com/onitaku"><img alt="Support development on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**Step into the complete Penumbra trilogy in PC VR.** Revisit three atmospheric adventures from inside the world: look around naturally, explore with your motion controllers and reach out to handle the objects around you.

[**Get the VR installer**](https://github.com/rubocopter/penumbra_vr_framework/releases/download/v1.0.6/PenumbraVR-Setup-1.0.6.exe) · [What you need](#before-you-start) · [Help](#need-help)

You’ll need the original PC games and SteamVR. Install only the games you own; **Requiem requires Penumbra: Black Plague**.

## Watch the trilogy in VR

Take a look at each game in motion. These short clips show the Framework in real gameplay; choose a player to watch right here on GitHub.

### Penumbra: Overture

Descend into the mine and explore its dark spaces from inside the game. Turn to look around, move through the environment and reach for the objects and puzzles in front of you.

https://github.com/user-attachments/assets/f842a62c-ed82-4423-b9b9-43c14caf2265

**Want the full opening?** [Watch the first 18 minutes, uncut, on YouTube →](https://www.youtube.com/watch?v=dc90jlH-tp0)

### Penumbra: Black Plague

Search the shelter for a way forward. Inspect your surroundings up close and use tracked hands to pick up, move and use objects as you work through its puzzles.

https://github.com/user-attachments/assets/9e4cc11f-7785-42e6-be19-6fa0ced367fd

### Penumbra: Requiem

Return to the Penumbra world for the puzzle-focused expansion. Requiem brings its rooms and challenges into VR as an add-on to Black Plague.

https://github.com/user-attachments/assets/325b79e0-aca3-4111-93d2-8a73f9530a07

## Make the world your own in VR

- **Look around naturally.** Head tracking and stereoscopic 3D put the game world around you.
- **Move through each space.** Use room-scale movement and controller locomotion, with turning and comfort options to suit your setup.
- **Reach into the puzzles.** Tracked hands bring physical object handling and game-specific interactions into play.
- **Keep the essentials close.** Inventory, notebook and menus are presented for VR.

The details vary between games and controller profiles. The guides below explain current compatibility and controls.

## Get started

1. Install the original game or games and SteamVR.
2. Download and run the [latest installer](https://github.com/rubocopter/penumbra_vr_framework/releases/tag/v1.0.6). Choose the games and optional extras you want.
3. Start SteamVR, put on your headset, then launch the game from Steam as usual.

The installer detects supported games and can install, update, repair or remove VR components. You don’t need to own the whole trilogy. Optional extras include Spanish translations, Overture texture improvements and recommended graphics settings.

## Before you start

- A Windows PC, a PCVR headset, two tracked controllers and SteamVR.
- Original game files matching a [recognized game build](docs/SUPPORTED_BUILDS.md). Other editions or modified executables may not work.
- Requiem installed as an expansion alongside Black Plague.

The project’s end-to-end headset testing has primarily used PS VR2 and Sense controllers. Other controller profiles are included, but their bindings and gameplay still need device-specific testing. Check the [controller guide](docs/CONTROLS.md) for details.

## Current notes

The latest release, **v1.0.6**, adds first-level VR Settings in the same place in all three games, in English and Spanish. It includes the maintainer-accepted Black Plague vice/locker and Requiem intro-height fixes, plus a correction for excessive Black Plague blocking near movable obstacles. The new menu routes and blocking correction have host tests; their headset confirmation remains separate. Requiem may occasionally fail during startup, and full progression and other controller families still need more testing. See [known issues and validation status](docs/CLOSURE_STATUS.md) before installing.

## Need help?

For installation steps, controller mappings, graphics settings and fixes, see [Troubleshooting](docs/TROUBLESHOOTING.md), [Controls](docs/CONTROLS.md) and [VR configuration](docs/VR_CONFIGURATION.md). To report a problem, [open or check a GitHub issue](https://github.com/rubocopter/penumbra_vr_framework/issues) and include the game, headset/controllers and what happened.

[Release notes](docs/releases/1.0.6.md) · [All releases](https://github.com/rubocopter/penumbra_vr_framework/releases) · [Support development on Ko-fi](https://ko-fi.com/onitaku)

<details>
<summary>Controller compatibility and default controls</summary>

Bindings are included for PS VR2 Sense, Valve Index, Meta/Oculus Touch, Pico and Windows Mixed Reality controllers. A profile being included does not mean that every controller or action has been validated on physical hardware. Vive wand and WMR layouts have known missing actions; see the [controller capability ledger](docs/TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution).

With the default right-handed Sense layout, the left stick moves and the right stick turns. R2 interacts or uses an item, R1 opens inventory, Square opens the notebook, Cross jumps and R3 toggles crouch in Button/Hybrid mode. Mappings can be changed in SteamVR.

</details>

<details>
<summary>Installer options, repair and removal</summary>

The installer lets you choose individual games and optional components, locate games that were not detected, apply recommended settings, verify or repair an installation, create a diagnostics ZIP and uninstall VR components. Overture is managed separately; Black Plague and Requiem share an installation folder, and Requiem depends on Black Plague. The same installer can be reopened to change or repair your selection.

See the [installer guide](docs/INSTALLER_DESIGN.md) and [recovery instructions](docs/TROUBLESHOOTING.md#repair-recovery-and-removal) for details.

</details>

<details>
<summary>Compatibility, settings and troubleshooting</summary>

Use recognized, clean game files and start SteamVR before launching a game. A GPU with working OpenGL drivers is required. The project has no published minimum CPU or GPU specification; performance depends on the PC, headset and selected render scale.

For a starting point, use VR render scale 1.0 where available and lower it if performance is insufficient. Disable desktop VSync/FPS limiting, legacy FSAA, motion blur, depth of field and noise filtering; keep physics at 60 updates per second. Keep PostEffects and Refractions enabled for Black Plague/Requiem. Settings vary between games; the [full configuration guide](docs/VR_CONFIGURATION.md) has the current recommendations.

Requiem can occasionally fail during startup. Black Plague may push the player back on contact with some objects. Other translations, texture packs, executable replacements and graphics injectors are not established as compatible. Unknown game builds are rejected by the installer.

If installation or launch fails, use the installer’s verify/repair tools and follow [Troubleshooting](docs/TROUBLESHOOTING.md). Include the game version, Framework release, headset/controllers, steps to reproduce and relevant logs in a [bug report](https://github.com/rubocopter/penumbra_vr_framework/issues).

</details>

<details>
<summary>More project information</summary>

For contributors and technical readers: [Architecture](docs/ARCHITECTURE.md) · [Trilogy capabilities](docs/TRILOGY_PARITY_PLAN.md) · [Recognized builds](docs/SUPPORTED_BUILDS.md) · [Release status](docs/CLOSURE_STATUS.md) · [Roadmap](docs/ROADMAP.md) · [Build instructions](docs/SOURCE-AND-NOTICES.md#building-the-versioned-release).

Penumbra VR Framework is an unofficial fan project and is not affiliated with or endorsed by Frictional Games, Valve or Sony Interactive Entertainment. It is licensed under **GNU GPL v3 or later**; see [COPYING](COPYING). Dependencies and optional assets retain their own terms; see [third-party notices](docs/THIRD_PARTY.md). The original games are required and are not included.

</details>
