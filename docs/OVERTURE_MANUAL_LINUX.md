# Penumbra VR Framework — Overture manual overlay (experimental)

This ZIP is a **manual, installer-free overlay** for the Windows version of
**Penumbra: Overture**. It is intended for community experiments with Steam
Proton or Wine on Linux. Linux runtime, SteamVR integration, controller bindings,
audio and headset gameplay have **not** been validated. It contains the
Framework-owned Windows x86 Overture VR executable and the matching runtime,
OpenVR bindings, shaders, models and textures. It does not contain the retail game.

## Install with Steam Proton

1. Install the game through Steam. Open its **Properties → Compatibility** and
   force a Proton version so Steam provides the **Windows** game files. Launch
   the unmodified game once, then close it and SteamVR.
2. Locate the game directory containing `redist/Penumbra.exe` and
   `redist/config/English.lang`. On a default Steam installation it is commonly
   `steamapps/common/Penumbra Overture/`. Other libraries have different paths.
3. Before changing files, check `sha256sum redist/Penumbra.exe` from the game
   directory. The recognized retail Windows executable SHA-256 is
   `95acb863441a17e701af2cd1b1ef301c55c1ac620269a167275580eb6954a448`.
   **Stop** if the checksum differs, either required path is missing, or a
   previous Penumbra VR installation is present. Use a clean retail install.
4. Extract this ZIP into a **separate** temporary directory. In that extracted
   directory run `sha256sum -c SHA256SUMS.txt`. Every entry must pass. The
   extracted `redist/` directory is the overlay payload.
5. From the **game directory**, back up its original `redist` folder **before**
   copying anything. The following commands assume the extracted ZIP is in
   `/tmp/penumbravr-overlay`:

   ```sh
   cp -a redist redist.before-penumbravr
   cp -a /tmp/penumbravr-overlay/redist/. redist/
   ```

   Keep `redist.before-penumbravr` until you have finished testing. Steam game
   verification or updates can change the installation, so restore the backup
   before verifying or updating the game. Do not mix this manual method with
   an installation already managed by the Windows Setup tool.
6. Start SteamVR and launch Overture through Steam's normal **Play** button.
   The overlay supplies `redist/Penumbra.exe` for Steam's launcher and keeps
   `redist/Penumbra_vr.exe` for direct developer troubleshooting.

## Restore

Close the game. From the game directory, remove the modified `redist` folder
and move the saved copy back in its place:

```sh
mv redist redist.penumbravr-test
mv redist.before-penumbravr redist
```

After confirming the original game starts, you can delete
`redist.penumbravr-test`. Keep this displaced directory until that confirmation.
Game saves and per-user settings outside `redist` are outside the overlay.

## Known limitations and feedback

This tests only whether the Windows x86 Overture build starts under Proton/Wine.
It uses **OpenVR/SteamVR** and needs a working OpenVR runtime exposed inside
the compatibility environment. A successful file copy or game launch does not
demonstrate VR input, audio, stereo rendering or playable gameplay. Native Linux
builds remain outside this experiment. Black Plague and Requiem have a separate
[experimental manual ZIP](BLACK_PLAGUE_REQUIEM_MANUAL_LINUX.md).

When reporting results, include the Linux distribution, Steam/Proton version,
headset, SteamVR/OpenVR runtime, whether the menu appears in the headset, and
any log or crash message. Do not attach private files or save data.
