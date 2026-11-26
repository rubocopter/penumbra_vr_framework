# Penumbra VR Framework — Black Plague + Requiem on Linux (experimental)

This ZIP installs the **Windows x86** VR backends under Steam Proton/Wine. Both
games share Black Plague's `redist/` directory. It contains the VR backends,
ALUT/OpenAL proxies, OpenVR bindings, hand texture and optional translations and
Neural companion, with their notices. No retail game or NVIDIA model is included.
Linux launch, SteamVR, audio, controllers and headset gameplay remain unvalidated.

## Install

1. Install Black Plague and, optionally, Requiem in Steam. Force a Proton version
   in **Properties → Compatibility** to obtain Windows game files. Launch the
   original game once, then close it and SteamVR.
2. Extract this ZIP **outside** the game directory. Install Python **3.9 or newer**
   on Linux; only its standard library is needed. Windows Setup is not used.
3. In the extracted folder run `sha256sum -c SHA256SUMS.txt`; all entries must pass.
4. Preview, then install using the directory containing `redist/`:

   ```sh
   python3 install-linux.py install '/path/to/Penumbra Black Plague' --recommended-settings --laa --plan
   python3 install-linux.py install '/path/to/Penumbra Black Plague' --recommended-settings --laa
   ```

`bash install-linux.sh` accepts the same arguments as `python3 install-linux.py`.
The helper checks supported canonical or pinned LAA Windows executables, original
ALUT/OpenAL, Requiem content when present, and all package hashes. It rejects
unknown builds and existing mods. Original ALUT forwarding is preserved and a
complete `redist.before-penumbravr` backup is created **before** changing files.
Start SteamVR and launch Black Plague or Requiem through Steam **Play**.
Do not drag the overlay onto an existing mod or copy `optional/` into the game.

## Options and existing settings

- `--spanish`: install community Spanish translations for Black Plague and
  detected Requiem. Existing valid language selections are preserved; select
  Spanish in-game. Translation credits are in the ZIP.
- `--recommended-settings`: apply the same Windows graphics profile, including
  native BP/Requiem post-effects/refractions. Unrelated settings are preserved.
- `--documents '/actual/Windows/Documents'`: also update existing personal
  `Penumbra/Black Plague/settings.cfg` and `Penumbra/Requiem/settings.cfg` in that
  game's Wine prefix. Locate the actual Documents directory, including any prefix
  redirection. No prefix guessing or native Linux save edits occur. Without this
  option only game defaults are updated; user preferences may override them.
- `--laa`: apply the exact pinned Large Address Aware transforms to detected
  games. Output hashes must match the known variants. Already transformed builds
  remain valid. This does not establish headset compatibility.
- `--plan`: validate and preview without writes.

OpenAL's `[general] hrtf = auto` is installed only if `alsoft.ini` is absent.
Existing audio configuration is preserved. These games do not have an additional
Framework enhancement texture pack; the VR hand texture is always installed.
In-game VR settings and controller profiles remain part of the existing runtime.

### Optional experimental Neural

Append the following to the install command to import your own local x64 model:

```sh
--neural-model '/path/to/nvngx_dlssnr.dll' --wine-redist 'Z:\path\to\game\redist'
```

Obtain the Windows redist path using `winepath -w` **inside this game's prefix**.
The bundled x64 helper/forwarder uses the same UTF-16 INI layout as Windows. No
model is downloaded or redistributed. Neural needs a 64-bit-capable prefix and
compatible NVIDIA runtime, and has **not** been validated under Wine/Proton.
Leave it disabled unless testing it; runtime menu controls are unchanged.

## Restore, update or repair

Close both games and SteamVR, then run:

```sh
python3 install-linux.py restore '/path/to/Penumbra Black Plague'
```

Pass the same `--documents` directory if used during installation. The helper
conditionally restores installer-owned personal attributes, preserves later user
edits and avoids leaving a language selection pointing at a removed translation.
The modified tree moves to `redist.penumbravr-test`; the completed original backup
returns to `redist`. Savegames and calibration are not deleted. Verify the original
games before deleting the test tree yourself.

For update/repair, restore, inspect/remove the test tree, then reinstall the
chosen options. Interrupted installs with a completed backup can be restored;
incomplete backups require manual inspection. Do not mix this helper with Windows
Setup receipts or update/verify Steam files before restoration. If an older
copy-only/script overlay is present, first restore with its original helper.

## Report results

Include distribution, Proton version, SteamVR runtime, headset/controllers,
package SHA-256, selected options, failing game and VR/crash logs. No Proton
version or DLL override profile is verified. Host tests do not establish Linux
compatibility. Do not attach retail files or personal saves.
