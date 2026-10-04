# Troubleshooting

This guide covers the public **Penumbra VR Framework v1.0.5** installer and the current main-branch support documentation. Installation steps live in the root [README](../README.md#installation); recognized build identities live in [SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md).

The 1.0.5 installer shows **Preparing installation / Preparando la
instalación** while files are extracted and verified, then an animated waiting
dialog during checks and changes. The window keeps responding while these run;
wait for the final result before launching a game. Published 1.0.0 retains its
older startup/Apply interface.

## Game detection and dependencies

- **Game not listed:** use **Locate game… / Buscar juego…** in the release installer to choose the original game folder or its `redist` folder. Requiem must be installed inside the Black Plague base. Incomplete targets show essential content/dependency failures; this checks launch prerequisites, not every purchased game asset.
- **Unknown executable:** obtain its identity with the read-only command below and compare it with the recognized builds. A renamed executable is not sufficient. Restore an original recognized build rather than attempting to bypass the check.
- **Missing DLL:** restore game-supplied files through Steam file verification. Both products supply pinned app-local OpenVR/OpenAL Soft; Overture also supplies its x86 Visual C++ files. Installer errors identify the dependency and remedy. See the [dependency inventory](RUNTIME_DEPENDENCIES.md).
- **Steam file verification:** it can replace managed Framework files. After verifying the base game, reopen the matching v1.0 installer and use **Repair installation / Reparar instalación** if the Framework state needs to be restored. Keep the installer-created backups.

## VR and controller input

Start SteamVR and confirm the headset and both controllers are ready before launching the game manually through Steam **Play**. Launch from the installed game, not from a development or extracted package directory. No OpenXR runtime switch is needed.

If the game launches flat, check that the release installation completed for that root and inspect its installer/bootstrap/runtime logs. Unknown builds are deliberately rejected.

If controllers track but an action is missing, inspect the active SteamVR binding for that game/controller. Repair restores owned bundled defaults; saved SteamVR custom mappings remain separate. Vive/WMR layouts have documented omissions, and only Sense has physical-device evidence. See [controls](CONTROLS.md) and the [controller ledger](TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution).

For Valve Index, v1.0.5 retains the corrected thumbstick paths and removes
overlapping defaults. After
updating, reselect the bundled default in SteamVR for each game if an older
binding is still active. To retain a custom layout, bind **Move** and **Turn**
to the stick's **Position** output in **Joystick** mode; `Vector2` is their
expected action type. The reported index/pinky finger mismatch remains
unresolved. See [the patch instructions](releases/1.0.5.md)
and the [default controls reference](CONTROLLER_BINDINGS.md). Black Plague VR
Settings can open the active SteamVR bindings directly; use SteamVR's dashboard
binding editor for Overture and Requiem.

Black Plague v1.0.3 corrects a reproduced switch to native camera height while
turning. If the resting viewpoint is low when playing seated, select Seated
play mode and check personal calibration. This update does not establish the
cause of the reported low resting height or change native crouch/collision rules.

For performance, begin with [recommended configuration](VR_CONFIGURATION.md) and reduce render scale where configurable. Current Black Plague/Requiem recommendations keep PostEffects/Refractions enabled; disabling them is not a general workaround for the corrected effects path.

Requiem's intermittent startup crash remains unresolved. If it occurs, capture the affected build identity and available logs/dump. A successful retry does not establish that the defect is fixed.

## Repair, recovery and removal

Close the affected games before every installer operation. Reopen the same **PenumbraVR-Setup-1.0.5.exe** used for the release installation.

Use the **Maintenance / Mantenimiento** tab for installed roots:

- **Verify VR installation / Comprobar instalación VR** performs a passive check of the supported executable, Framework-owned files/backups, controller bindings, configuration and required dependencies.
- **Repair installation / Reparar instalación** restores missing or damaged recorded Framework payloads while respecting ownership/conflict checks.
- **Uninstall… / Desinstalar…** opens component selection. Overture can be removed independently. Selecting only the Requiem layer removes Requiem VR support while keeping Black Plague; removing Black Plague also removes its dependent Requiem layer.
- **Restore interrupted installation / Restaurar instalación interrumpida** restores the verified snapshot from before an interrupted transaction. It is enabled only when such an interrupted installation is detected; it is not a general backup browser.

If an operation reports unexpected modifications, retain the reported files and logs. Do not delete installer backups or force an overwrite.

Maintainers can invoke the packaged selector from a source checkout when explicit CLI recovery or inspection is required:

```powershell
.\tools\Install-PenumbraFrameworkCandidate.ps1 -Recover -Game Overture -GamePath 'C:\path\to\Penumbra Overture'
```

Use `-Game BlackPlague` or `-Game Requiem` with that game's folder/executable for the shared root. Recovery works even if the executable is missing. Complete recovery before another install, repair or removal. The [installer contract](INSTALLER_DESIGN.md) describes verified backups and conflict handling.

## Logs and build information

| Information | Location |
| --- | --- |
| Combined installer events | `%LOCALAPPDATA%\PenumbraVR\installer.jsonl` |
| Black Plague bootstrap | `%LOCALAPPDATA%\PenumbraVR\black_plague_bootstrap.log` |
| Requiem bootstrap | `%LOCALAPPDATA%\PenumbraVR\requiem_bootstrap.log` |
| Black Plague/Requiem runtime logs | `%LOCALAPPDATA%\PenumbraVR\logs\` |
| Overture HPL log | `Documents\Penumbra Overture\Episode1\hpl.log` (including redirected Documents folders) |
| Overture crash dump, if produced | `penumbravr_crash.dmp` beside the installed game executable |

From a repository checkout, inspect an executable without modifying it:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 'C:\path\to\game.exe' -AsJson
```

Use **Generate diagnostics ZIP / Generar ZIP de diagnóstico** in Maintenance to collect bounded, redacted technical information suitable for a bug report. The CLI equivalents are `-Verify` or `-DiagnosticOutputPath <new.zip>` with `-Game` and `-GamePath`. Headset/controllers remain unknown when no passive evidence is available. The collector currently includes B/R probe tails; Overture's HPL log is not collected. Include reproduction steps and hardware in the issue, and review the archive before sharing.

Use **View log / Ver registro** to open the installer log directly. Check [GitHub Issues](https://github.com/rubocopter/penumbra_vr_framework/issues) for existing reports before opening a new one.

For Overture-specific GPU/audio/crash details, see the imported [Overture troubleshooting guide](../products/overture/docs/TROUBLESHOOTING.md). Its standalone installer commands belong to that legacy product baseline; use the unified Framework installer for v1.0 installation, repair and removal.
