# Penumbra VR Framework — Overture on Linux (experimental)

This package runs the **Windows x86** VR product under Steam Proton/Wine. It
includes the same VR executable, runtime libraries, OpenVR controller bindings,
shaders, models and optional resources as Windows. It contains no retail game.
Linux launch, SteamVR, audio, controllers and headset gameplay remain unvalidated.

## Install

1. Install Overture in Steam. In **Properties → Compatibility**, force a Proton
   version to obtain the Windows build. Launch the unmodified game once, then
   close the game and SteamVR.
2. Extract this ZIP **outside** the game folder. Install Python **3.9 or newer**
   on Linux; only its standard library is used. No Windows Setup or PowerShell
   is needed.
3. From the extracted folder verify `sha256sum -c SHA256SUMS.txt`. All entries
   must report OK. The helper repeats verification before installation.
4. Preview, then install, substituting your actual Steam game directory:

   ```sh
   python3 install-linux.py install '/path/to/Penumbra Overture' --recommended-settings --textures --plan
   python3 install-linux.py install '/path/to/Penumbra Overture' --recommended-settings --textures
   ```

The supported original `redist/Penumbra.exe` SHA-256 is
`95acb863441a17e701af2cd1b1ef301c55c1ac620269a167275580eb6954a448`.
Unknown builds and existing mods are rejected. The helper backs up the complete
`redist/` before any modification, provides Steam's `Penumbra.exe` entry point,
and retains `Penumbra_vr.exe` for troubleshooting. Start SteamVR and use Steam
**Play**. Do not copy `optional/` directly into the game.

## Options and existing settings

- `--textures`: install the selected Overture enhancement textures. Core VR
  models/shaders remain installed without this option. Credits accompany the ZIP.
- `--spanish`: install the Spanish translation. A valid current language selection
  is preserved; choose Spanish in the game to activate it.
- `--recommended-settings`: use the same per-game graphics profile as Windows.
  Changes apply only to existing sections and preserve unrelated settings.
- `--documents '/actual/Windows/Documents'`: additionally update existing personal
  settings in the selected Wine/Proton prefix. Locate the **actual Documents
  directory used by this game**, which may be redirected. The helper never guesses
  the prefix or edits native Linux saves. Without this option only game default
  files are updated; existing personal settings may override them.
- `--plan`: preflight and preview without writing files.

OpenAL's `[general] hrtf = auto` configuration is installed only when `alsoft.ini`
is absent; an existing audio configuration is preserved. Overture already has
Large Address Aware enabled in the shipped executable. In-game VR settings and
controller profiles remain available, subject to the unvalidated Wine runtime.

### Optional experimental Neural

The ZIP includes the Windows x64 helper/forwarder and their notices. NVIDIA
model bytes are **not** included or downloaded. To import your own model, append:

```sh
--neural-model '/path/to/nvngx_dlssnr.dll' --wine-redist 'Z:\path\to\game\redist'
```

Use the Windows path returned by `winepath -w` **inside this game's prefix**,
not another prefix's mapping. The helper writes the same UTF-16 Neural INI layout
as Windows. This requires a 64-bit-capable prefix and compatible NVIDIA runtime;
Neural under Wine/Proton has not been validated. Keep it disabled unless testing
that feature. Its runtime menu controls are unchanged.

## Restore, update or repair

Close the game and SteamVR. Use the same extracted helper:

```sh
python3 install-linux.py restore '/path/to/Penumbra Overture'
```

If installed with `--documents`, pass the **same** directory on restore. Personal
attributes still equal to installer-applied values are restored; later edits are
preserved. Language selection falls back to bundled English if the selected
translation is removed. Saves and calibration are not deleted.

The modified tree moves to `redist.penumbravr-test`; the verified completed
backup returns to `redist`. Confirm the original game works before deleting the
test tree yourself. For update/repair, restore, inspect/remove that test tree,
then install with the desired options. An interrupted install with a completed
backup can be restored; an incomplete backup is rejected for manual inspection.
Do not mix Windows Setup receipts with this helper or run Steam verification or
updates before restoration. Older copy-only overlays require the manual restore
procedure in their original guide before this helper can install.

## Report results

Include distribution, Proton version, SteamVR runtime, headset/controllers,
package SHA-256, selected options and game/VR error logs. No Proton version or
DLL override profile is verified. Host tests do not establish Linux compatibility.
Do not attach retail files or personal save data.
