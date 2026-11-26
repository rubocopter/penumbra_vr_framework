# Penumbra VR Framework — Black Plague + Requiem manual overlay (experimental)

This ZIP contains the Framework v1.0.15 **Windows x86** VR modules for
**Penumbra: Black Plague** and **Penumbra: Requiem**. It is intended for
experiments with Steam Proton or Wine on Linux, without the Windows installer.
Linux launch, OpenVR integration, controls and headset gameplay are **unverified**.
It does not include retail game files, and it does not convert the mod into a
native Linux build. The two games use the **same `redist/` directory**.

## Install with Steam Proton

1. Install the Windows Steam versions of Black Plague and (if owned) Requiem.
   In Steam **Properties → Compatibility**, force Proton for the game so Steam
   downloads the Windows files. Start and quit the original game once.
   Requiem requires the Black Plague installation and its expansion content.
2. Locate the directory containing `redist/Penumbra.exe` and, for Requiem,
   `redist/Requiem.exe`. Extract this ZIP into a **different** directory.
3. Use a terminal to run this command, replacing both directories with yours:

   ```sh
   bash '/path/to/extracted/install-linux.sh' install '/path/to/Penumbra Black Plague'
   ```

   The helper verifies the ZIP with `sha256sum`, checks recognized unmodified
   Windows game executable hashes and the original retail `alut.dll`, rejects
   existing mod installations, and backs up the whole original directory as
   `redist.before-penumbravr`. It then preserves the original `alut.dll` as
   `PenumbraVR_alut_original.dll` for the forwarding bootstrap and applies the
   VR files. Keep the backup until you have finished testing.
4. Start SteamVR and launch Black Plague or Requiem using Steam's usual
   **Play** button with Proton. Both game executables remain retail files.

The helper only accepts recognized executable builds listed in
`docs/SUPPORTED_BUILDS.md` in the repository. Black Plague is required.
Requiem is optional when installing the shared overlay, but only the recognized
Windows Requiem build can be started by the bootstrap. Use a clean game directory
if a previous Windows Setup installation or other manual VR mod exists.
The ZIP includes the OpenVR loader/bindings, game-specific probes, audio proxies,
OpenAL Soft, required hand texture and third-party license notices. Other
original game data and DLLs are supplied by the user's Steam installation.

## Restore

Close the game and run:

```sh
bash '/path/to/extracted/install-linux.sh' restore '/path/to/Penumbra Black Plague'
```

The helper moves the modified `redist` to `redist.penumbravr-test` and returns
the backup to `redist`. Verify that the original game works before deleting
`redist.penumbravr-test`. If a copy was interrupted after backup creation, this
restore command can recover the original directory. Avoid running Steam file
verification or updating the game until after restoration.

## Testing and feedback

Proton must expose a working SteamVR/OpenVR runtime to the Windows executable.
The original games' x86 dependencies must also function under the chosen
Wine/Proton version. Windows build checks and synthetic file installation tests
do not establish Linux compatibility. There is no verified Proton version,
DLL override profile or headset result for these experimental artifacts.

Report Linux distribution, Proton version, SteamVR runtime, headset/controllers,
which game launches, any audio or stereo rendering issues, and crash/VR logs.
Do not upload your retail game files or personal save data to issue reports.
