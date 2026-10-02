# Penumbra VR Framework

![Penumbra VR Framework — Overture, Black Plague and Requiem](assets/banner/penumbra-vr-framework.png)

<p align="center">
  <a href="COPYING"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
  <a href="https://github.com/rubocopter/penumbra_vr_framework/releases/tag/v1.0.1"><img alt="Release: v1.0.1" src="https://img.shields.io/badge/release-v1.0.1-brightgreen?style=flat-square"></a>
  <a href="#requirements"><img alt="Platform: Windows" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square"></a>
  <a href="#vr-hardware"><img alt="Runtime: SteamVR / OpenVR" src="https://img.shields.io/badge/runtime-SteamVR%20%2F%20OpenVR-1b2838?style=flat-square"></a>
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support development on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**VR rendering, head tracking and motion-controller interaction for Penumbra: Overture, Black Plague and Requiem.**

One PCVR project for the trilogy. You must own and install the original games; Requiem uses the Black Plague base installation.

[**Download v1.0.1**](https://github.com/rubocopter/penumbra_vr_framework/releases/tag/v1.0.1) · [Installation](#installation) · [Troubleshooting](docs/TROUBLESHOOTING.md) · [Report an issue](https://github.com/rubocopter/penumbra_vr_framework/issues)

## Status

**v1.0.1 is the current public Framework release.** This patch improves Overture object/mechanism acquisition, Oculus Touch aim and installer responsiveness. The maintainer installed the exact released EXE and accepted an Overture headset play session including the initial drawer and repeated crank/mechanism interaction. See the [patch notes](docs/releases/1.0.1.md).

The v1.0.0 foundation retains its install/uninstall and game-entry evidence for all three games. The new release also passed automated installation topologies, repair, removal and exact restoration; current Black Plague changes and Quest 3 pointer alignment retain their separate validation limits.

Validation breadth still differs by game and hardware. PS VR2 + Sense is the controller setup validated end to end by the maintainer; other bundled controller profiles still need device-specific feedback. Independent clean-PC coverage also remains limited. See the [release status](docs/CLOSURE_STATUS.md), [recognized builds](docs/SUPPORTED_BUILDS.md) and known issues below for the evidence boundaries.

## Supported Games

| Game | Current validation | Installation |
| --- | --- | --- |
| **Penumbra: Overture** | The v1.0.1 installer and a gameplay session including drawer/crank interactions have maintainer acceptance. | Separate Overture installation. |
| **Penumbra: Black Plague** | Gameplay VR from Steam Play and representative interactions/effects tested in headset; the v1.0 installer has maintainer game-entry acceptance. | Black Plague installation. |
| **Penumbra: Requiem** | Representative puzzles, first transition, portal effects and held tools tested in headset; broader progression and launch reliability remain open. | Expansion in the Black Plague installation; requires its base game. |

The release recognizes specific builds rather than every retail or modified executable. Check [recognized builds and validation](docs/SUPPORTED_BUILDS.md) before installing; unknown executables are rejected.

## Features

- Stereoscopic rendering with head tracking and room-scale movement.
- Tracked controller hands, object interaction and game-specific puzzle integration.
- Controller locomotion, turning, jumping and physical/button crouching.
- VR menus, inventory and notebook presentation.
- SteamVR Input bindings for eight controller profiles, with coverage limits listed below.
- Configurable comfort and display options in Overture and Black Plague; Requiem currently uses development defaults.
- One installer for Overture and the shared Black Plague/Requiem root, with discovery, backups, repair, recovery and restore.

Feature coverage and validation differ by game; see the [trilogy capability ledger](docs/TRILOGY_PARITY_PLAN.md) for details.

## Installation

Download **PenumbraVR-Setup-1.0.1.exe** from the [v1.0.1 release](https://github.com/rubocopter/penumbra_vr_framework/releases/tag/v1.0.1). The matching source archive, checksums and build report are published alongside it.

1. Install the original games and close them. Use recognized, clean game files.
2. Double-click the Setup EXE and accept Windows elevation. It contains the installer and mod payload for your installed games.
3. Choose **English** or **Español**. English is the default; Español automatically selects the optional translations, which remain editable.
4. Review the detected games. Use **Locate game… / Buscar juego…** if an installation was not found automatically.
5. Select the games and optional components you want. The primary action changes automatically between **Install**, **Update**, **Apply changes** and **Repair installation** according to the selected installation state.
6. Start SteamVR and confirm the headset/controllers are ready, then launch the selected game manually through its normal Steam **Play** button.

You do not need the whole trilogy. The installer handles Overture only, Black Plague only, Black Plague + Requiem, Overture + Black Plague, or all three. Requiem appears as a layer of its Black Plague installation rather than as a separate standalone target.

Spanish localization, Overture texture enhancements and targeted recommended graphics settings are optional. English starts with translations off; selecting Español checks the translation options. Settings changes preserve language, resolution and personal calibration; removal preserves subsequent user edits. Reopen the same EXE to modify components, verify or repair the installation, generate diagnostics, or uninstall. See [installer behavior](docs/INSTALLER_DESIGN.md) for recovery and ownership rules.

## Requirements

- **Windows 10/11** is the release target. Independent clean-PC coverage remains limited.
- Licensed original game installations matching the [recognized builds](docs/SUPPORTED_BUILDS.md). Black Plague/Requiem target the recorded Steam builds; other editions are not established as compatible.
- **SteamVR**, a PCVR headset and two tracked controllers. The project uses OpenVR; no OpenXR runtime switch is required.
- A GPU with working vendor OpenGL drivers. No measured minimum CPU/GPU specification is published.
- Windows PowerShell 5.1 and .NET Framework Windows Forms are supplied by Windows. The EXE bundles its own bootstrapper runtime; no .NET SDK/runtime installation is required.

Both products bundle pinned app-local OpenVR and OpenAL Soft; Overture also bundles its pinned x86 Visual C++ runtime. Preflight checks game-owned x86 libraries, essential configuration/content and SteamVR before installation. Missing game files must be restored through Steam. The [dependency audit](docs/RUNTIME_DEPENDENCIES.md) owns the exact inventory and validation limits.

## VR Hardware

Bindings are included for the following families. A bundled profile does not establish that a physical device has been tested or that every action is usable.

| Headset / controller family | Evidence and limitations |
| --- | --- |
| PS VR2 + Sense on PC | Physical headset/controller evidence in all three games; v1.0 maintainer install/game-entry acceptance. |
| Valve Index controllers | Bindings included; physical-device validation pending. |
| Meta Quest / Oculus Touch through SteamVR | Touch bindings included; physical-device validation pending. |
| Pico 4 / Pico Neo 3 through SteamVR | Two binding profiles included; physical-device validation pending. |
| HTC Vive wands | Untested compatibility layout; turn, button crouch, holster and pause are unbound. |
| Windows Mixed Reality / Holographic controllers | Two untested layouts; holster and skeletal outputs are missing. |

The [controller ledger](docs/TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution) is authoritative for action coverage. Bindings can be customized through SteamVR. WMR runtime availability must be checked for the user's setup.

## Recommended Settings

**Required:** use SteamVR and a recognized game build. **Recommended:** disable desktop VSync/FPS limiting, legacy FSAA, motion blur, depth of field and noise filtering; keep physics at 60 updates/s. Begin at VR render scale 1.0 where configurable, lowering it if performance is insufficient.

Keep Black Plague/Requiem **PostEffects and Refractions enabled** in the current recommended profile: the observed special-effects and portal rendering corrections are included. Overture has a different profile. Height, handedness, turning, language and subtitle size are personal choices.

[VR configuration](docs/VR_CONFIGURATION.md) is the authoritative settings guide, backed by the versioned preset. Edit game configuration only while the game is closed. Requiem has no persisted VR settings page/preset yet.

## Controls

With the default right-handed Sense layout: **left stick** moves, **right stick** turns, **R2** interacts/uses, **R1** opens inventory and **Square** opens the notebook. **Cross** jumps; **R3** toggles crouch in Button/Hybrid mode. Physical crouching is also available.

See [controls](docs/CONTROLS.md) for the Sense quick reference, menus and other controller profiles. SteamVR custom bindings can change these mappings.

## Gameplay

*Current v1.0 gameplay video coming soon.*

### Steam Artwork

Optional Steam library artwork for Overture, Black Plague and Requiem is included in the repository for users who want the trilogy to match the VR installation.

[Overture](assets/steam/overture-vr-600x900.png) · [Black Plague](assets/steam/black-plague-vr-600x900.png) · [Requiem](assets/steam/requiem-vr-600x900.png) — 600 × 900 PNGs. Save the image and assign it as custom artwork in Steam's library. The artwork is promotional illustration, not gameplay footage.

## Known Issues

- Requiem can fail intermittently during startup; the cause remains unresolved.
- Black Plague can produce small vertical hops when touching ventilation walls.
- Requiem has slight finger/contact intersections, resistant monolith rings and possible displacement of stacked blocks during climbing.
- Requiem throwing and broader progression, and several current Black Plague interaction/UI paths, need further headset regression.
- Non-Sense controller hardware lacks personal validation. Vive/WMR defaults have the omissions listed above.

## Compatibility

| Installation or modification | Current position |
| --- | --- |
| Recognized vanilla builds | v1.0 allowlisted inputs; automated package/install/repair/restore coverage and maintainer release acceptance exist, but independent clean-PC breadth is limited. |
| Bundled Spanish translations | Optional; installation/repair/restore host-tested. |
| Bundled Overture texture selection | Optional inherited product baseline; does not establish compatibility with arbitrary texture packs. |
| Other translations, enhancement mods or texture packs | Compatibility unknown; report the exact combination. |
| Executable replacements | Unknown hashes are rejected; only recorded builds/managed variants are recognized. |
| Other graphics injectors or proxy DLLs | Compatibility not established; test a clean installation first. |

## Troubleshooting

If a game is missing, use **Locate game… / Buscar juego…** and check its build. If it launches flat, confirm the installation completed and SteamVR is running, then collect the logs. Missing bindings can be checked in SteamVR and repaired by reopening the release installer.

For unsupported versions, dependency failures, modified-file conflicts and interrupted installs, follow [Troubleshooting](docs/TROUBLESHOOTING.md). The **Maintenance / Mantenimiento** tab provides **Verify VR installation / Comprobar instalación VR**, **Repair installation / Reparar instalación**, **Generate diagnostics ZIP / Generar ZIP de diagnóstico**, the installation log and interrupted-install recovery.

## Bug Reports

Use [GitHub Issues](https://github.com/rubocopter/penumbra_vr_framework/issues) to check existing reports and submit a report if issue creation is available. Include the affected game/build, Framework version or commit, headset/controllers, SteamVR version, reproduction steps and relevant [logs](docs/TROUBLESHOOTING.md#logs-and-build-information). For a crash, include the crash dump if one was produced.

## Uninstallation

Close the games and reopen the same **PenumbraVR-Setup-1.0.1.exe**. Use the **Maintenance / Mantenimiento** tab and choose **Uninstall… / Desinstalar…**. The removal dialog lets you select which installed VR components to remove while the installer verifies backups and restores owned original files.

Overture can be removed independently. Black Plague and Requiem share one installation root: selecting only the Requiem layer removes its VR support while keeping Black Plague, while removing Black Plague also removes its dependent Requiem layer. Preserve the installer backups and follow [recovery guidance](docs/TROUBLESHOOTING.md#repair-recovery-and-removal) if an operation was interrupted or reports modified files.

## Documentation

[VR configuration](docs/VR_CONFIGURATION.md) · [Controls](docs/CONTROLS.md) · [Troubleshooting](docs/TROUBLESHOOTING.md) · [Recognized builds](docs/SUPPORTED_BUILDS.md)

For maintainers: [Architecture](docs/ARCHITECTURE.md) · [Trilogy capabilities](docs/TRILOGY_PARITY_PLAN.md) · [Installer contract](docs/INSTALLER_DESIGN.md) · [Release status](docs/CLOSURE_STATUS.md) · [Roadmap](docs/ROADMAP.md)

## Building

Use Visual Studio 2022 with C++ support, the Win32 toolchain and CMake 3.25+. From the repository root:

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release --output-on-failure
.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full -Package
.\tools\Package-FrameworkCandidate.ps1 -OutputPath .\artifacts\PenumbraVR-Framework-candidate.zip
```

Root binaries appear in `build/bin/Release`; Overture builds under `products/overture/build`. The last command produces an internal development ZIP. For a versioned Setup EXE, matching Source ZIP, notices, checksums and build report from a clean committed checkout, use [the release procedure](docs/SOURCE-AND-NOTICES.md#building-the-versioned-release). It requires explicit pinned official CRT and upstream source inputs. Internal candidate packages do not imply public support.

## Credits

- **Frictional Games** — Penumbra and the HPL1 engine.
- [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr) and [rubocopter/penumbra_vr_rework](https://github.com/rubocopter/penumbra_vr_rework) — the original VR work and proven Overture baseline.
- **Valve**, **OpenAL Soft** and the other dependencies/contributors listed in [third-party notices](docs/THIRD_PARTY.md).
- Translation and texture contributors retain their separate [attribution](docs/THIRD_PARTY.md).

Penumbra VR Framework is an unofficial fan project and is not affiliated with or endorsed by Frictional Games, Valve or Sony Interactive Entertainment.

## License

Licensed under **GNU GPL v3 or later**; see [COPYING](COPYING). Dependencies and optional assets retain their own terms; see [third-party notices](docs/THIRD_PARTY.md). The original games are required and are not included in Framework packages.
