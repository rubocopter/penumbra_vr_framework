# Troubleshooting

This guide covers the current combined Framework development candidate. Installation steps live in the root [README](../README.md#installation); recognized build identities live in [SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md). The public v1.0 installer is still being prepared.

## Game detection and dependencies

- **Game not listed:** use **Carpeta...** in the candidate GUI to choose the original installation. Requiem must be installed inside the Black Plague base. Incomplete targets show essential content/dependency failures; this checks launch prerequisites, not every purchased game asset.
- **Unknown executable:** obtain its identity with the read-only command below and compare it with the recognized builds. A renamed executable is not sufficient. Restore an original recognized build rather than attempting to bypass the check.
- **Missing DLL:** restore game-supplied files through Steam file verification. Both products supply pinned app-local OpenVR/OpenAL Soft; Overture also supplies its x86 Visual C++ files. Installer errors identify the dependency and remedy. See the [dependency inventory](RUNTIME_DEPENDENCIES.md).
- **Steam file verification:** it can replace managed Framework files. After verifying the base game, inspect the installer state and use the matching package to reinstall or repair. Keep the original backups.

## VR and controller input

Start SteamVR and confirm the headset and both controllers are ready before launching the game manually through Steam Play. Launch from the installed game, not the extracted package directory. No OpenXR runtime switch is needed.

If the game launches flat, check that the candidate installation completed for that root and inspect its installer/bootstrap/runtime logs. Unknown builds are deliberately rejected.

If controllers track but an action is missing, inspect the active SteamVR binding for that game/controller. Repair restores owned bundled defaults; saved SteamVR custom mappings remain separate. Vive/WMR layouts have documented omissions, and only Sense has physical-device evidence. See [controls](CONTROLS.md) and the [controller ledger](TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution).

For performance, begin with [recommended configuration](VR_CONFIGURATION.md) and reduce render scale where configurable. Current Black Plague/Requiem recommendations keep PostEffects/Refractions enabled; disabling them is not a general workaround for the corrected effects path.

Requiem's intermittent startup crash remains unresolved. If it occurs, capture the failed candidate/build identity and available logs/dump; a successful retry does not establish that the defect is fixed.

## Repair, recovery and removal

Close the affected games before every installer operation. Keep the extracted matching candidate and original backup/state files.

Use **Reparar** for missing or damaged recorded Framework payloads and **Desinstalar** for removal. Review the preview before applying. **Retirar Requiem** removes only that layer; removing Black Plague includes dependent Requiem. If an operation reports unexpected modifications, retain the reported files and logs; do not delete the backups or force an overwrite.

Use **Recuperar** for an interrupted operation, or run the packaged selector explicitly:

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

From the repository or the extracted combined package, inspect an executable without modifying it:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 'C:\path\to\game.exe' -AsJson
```

Use **Verificar** for a passive installation report and **Diagnóstico ZIP** to collect bounded, redacted technical information. The CLI equivalents are `-Verify` or `-DiagnosticOutputPath <new.zip>` with `-Game` and `-GamePath`. Headset/controllers remain unknown when no passive evidence is available. The collector currently includes B/R probe tails; Overture's HPL log is not collected. Include reproduction steps and hardware in the issue. Review the archive before sharing. Check [GitHub Issues](https://github.com/rubocopter/penumbra_vr_framework/issues); new reports depend on repository issue-creation availability.

For Overture-specific GPU/audio/crash details, see the imported [Overture troubleshooting guide](../products/overture/docs/TROUBLESHOOTING.md). Its standalone installer commands belong to that product baseline; use the combined candidate instructions above for Framework installation/removal.
