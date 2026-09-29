# Release dependency audit

This owns the deployment dependency inventory for the existing candidate, not
a completed v1.0 distribution. Release gates are in
[CLOSURE_STATUS.md](CLOSURE_STATUS.md); provenance remains in
[THIRD_PARTY.md](THIRD_PARTY.md).

## Architecture and evidence

The audit inspected Release PE imports/headers with MSVC `dumpbin`, package and
deployment scripts, dynamic loads and vendored inputs. All inspected game
executables, Framework DLLs, OpenVR loaders and the LAA tool are **x86**.
An x64 runtime DLL cannot satisfy an x86 game import.

Root CMake uses static MSVC CRT (`/MT`); Overture uses `/MD`. Framework probes,
bootstrap and LAA tool import Windows libraries only; the probes dynamically
load OpenVR. Overture directly imports OpenVR and legacy HPL libraries.
The old dependency README's source/header versions do not always identify the
actual DLLs. The table distinguishes measured versions from snapshot labels.

## Runtime inventory

O = Overture; B/R = Black Plague and Requiem. Game-supplied dependencies are
not redistributed by Framework. `assets/deployment/prerequisites.json` owns
the x86 local-library and essential-content inventory consumed by discovery,
preflight and final verification. This does not validate every game asset or
prove that a codec/audio device works in gameplay.

| Dependency / version | Architecture | Purpose / consumer | Redistribution and current delivery | Verification |
| --- | --- | --- | --- | --- |
| OpenVR SDK 2.15.6; loader resource 1.1.1 | x86 loader | Tracking/compositor/actions, all games | BSD 3-clause, bundled with Valve notice in both products | Existing package hash/pinned-loader checks; installed hash and action graph |
| SteamVR | External runtime | OpenVR service/headset/controllers, all games | Install externally through Steam, not bundled | Resolve registered OpenVR runtime and files; missing-runtime guidance; manual headset readiness |
| OpenAL Soft 1.25.2 | x86 | Audio, EFX/HRTF | O: `OpenAL32.dll`; B/R: private `PenumbraVR_OpenALSoft.dll` behind Framework `OpenAL32.dll` device compatibility proxy. Both use the pinned official Win32 implementation; LGPL COPYING, PFFFT notice and full upstream source accompany the release | Hash/PE checks, actual x86 loader/null-device test and journaled original restore; audible B/R gameplay/headset acceptance pending |
| VC143 CRT 14.44.35211.0: `msvcp140.dll`, `vcruntime140.dll` | x86 | O `/MD` executable | Official app-local files bundled, SHA-256 pinned in `products/overture/runtime-dependencies.json`; subject to VS terms | Existing packaging hash pins; installed hashes and UCRT |
| Universal CRT and Win32 APIs | x86 interface | CRT API sets, kernel/user/shell/crypto/GDI+/OpenGL/audio | OS supplied; never copy system DLLs | Tested Windows 10/11 baseline and necessary APIs |
| .NET bootstrapper runtime / Windows Desktop | x64 | Single Setup EXE only | Self-contained publication; Microsoft license and third-party notices retained under `licenses/dotnet` inside the payload | Standalone EXE smoke with absent `DOTNET_ROOT`; separate-PC acceptance pending |
| VC++ 7.1: `msvcp71.dll` 7.10.3077.0, `msvcr71.dll` 7.10.3052.4 | x86 | B/R executable imports | Observed game-local files; not bundled by Framework | Local PE/import checks; Steam file verification when missing |
| SDL: DLL resource 1.2.11 | x86 | Window/input/audio, all games | Game supplied; vendored build input. Complete legacy binary notice set absent | Verify local dependency; do not substitute the README's 1.2.14 automatically |
| SDL_image 1.2.5, SDL_ttf 2.0.8 | x86 | Images/fonts, all games | Game supplied; vendored link inputs; normalized binary notices incomplete | Direct/dynamic codec closure and representative decoding |
| Cg/CgGL: DLL resources 1.5.0019 | x86 | Native shaders, all games | NVIDIA proprietary runtime; game supplied. Exact applicable redistribution terms not established here | Both local DLLs and shader loading; README's 2.2 is not binary identity |
| Newton: snapshot README 1.53, no DLL resource version | x86 | Native physics, all games | Game supplied; no new redistribution permission established | Local PE/import closure; do not replace game's physics DLL |
| ALUT: snapshot 1.1, no DLL resource version | x86 | Audio startup, all games | O game supplied; B/R proxy forwards to backed-up retail DLL | Existing B/R original SHA-256 allowlist; OpenAL dependency also needs checking |
| Vorbisfile/Vorbis/Ogg: snapshot README 1.1/1.0, versionless DLLs | x86 | Audio decoding, all games | Game supplied; vendored link inputs; exact notices need normalization if ever shipped | Recursive imports/local architecture; hashes identify versionless files |
| Theora: snapshot README 1.0 | x86 | Legacy media support | Game supplied; vendored input, not copied by candidate | Identify active consumer before requiring or shipping DLL |
| JPEG, PNG12 1.2.10 in observed B/R root, zlib 1.2.1 | x86 | Dynamic image decoding | Game supplied; not bundled as replacements | Actual codec load/decode; PNG recursively imports zlib |
| PNG13 1.2.7 | x86 | Present in vendored bundle and observed game root | Not copied by candidate; does not satisfy a PNG12 request | Identify actual consumer before requiring/bundling |

The inspected `SDL_image.dll` dynamically names `jpeg.dll`, `libpng12.dll`
and `libtiff.dll`. TIFF is conditional: determine whether supported assets need
it rather than shipping an arbitrary DLL. The vendored folder has PNG13 but
no PNG12. Static import inspection alone cannot prove codec deployment.

AngelScript and GLee are Overture static build/link inputs rather than new
installer DLLs. Windows HPL uses native MessageBox and compiles its FLTK calls
only outside WIN32; unused Windows `fltk.lib` linker entries were removed.
The executable does not import `fltkdll.dll`; that unused vendored DLL imports
`MSVCR80.dll` and is excluded from the installer.
HPL and OALWrapper are built as static libraries. Their source/notices remain
required for distribution.

## Confirmed development-machine dependency

B/R executables and original ALUT import `OpenAL32.dll`; the audited original shared
root has no app-local copy. The development machine has a system OpenAL router
with resource version 6.14.0357.24. The original Framework manifest supplied `alsoft.ini`
without an implementation. This established a dependency; it
does not prove which device implementation each prior headset run selected.

The clean-user run exposed a second boundary: B/R request the legacy router's
`Generic Software` / `Generic Hardware` playback names, which the pinned OpenAL
Soft implementation does not recognize. The Framework proxy adapts those names
to the default device using the fallback already demonstrated in Overture's
`LowLevelSoundOpenAL.cpp`; it forwards the remaining API to the private pinned
implementation. This removes system-router dependence while preserving game
audio policy. Empty device names also use the default; explicit modern names
are passed through.

The candidate supplies both app-local files to B/R, with host-tested
install/repair/upgrade/backup/rollback and an actual x86 loader test using null
audio. Independent audible gameplay validation remains pending. Unknown existing
audio DLLs must be preserved or cause an actionable conflict, never silently
replaced. O audio evidence does not validate B/R audio.

## Runtime, build and license boundaries

This is an **OpenVR / SteamVR** product. No OpenXR loader is used in the audited
build. No global OpenXR runtime change is needed. Both input consumers call
`SetActionManifestPath` on adjacent `vr/actions.json`; all eight defaults can
coexist there. Persistent SteamVR application registration is not implemented
and is not required simply to set the action manifest.

The single EXE bundles its .NET bootstrapper runtime. Its hidden GUI engine uses
Windows PowerShell 5.1 and .NET Framework Windows Forms supplied by Windows;
Windows Script Host is no longer needed for the public EXE. Visual Studio,
the .NET SDK, CMake, Python and shader compilation
are developer tools. The audited paths demonstrate no DirectX legacy runtime
requirement; rendering uses OpenGL and the GPU vendor driver.

Microsoft permits app-local deployment subject to applicable Visual Studio
license/distributable-code terms, which the publisher must record. See
[redistribution guidance](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170)
and [deployment methods](https://learn.microsoft.com/en-us/cpp/windows/choosing-a-deployment-method?view=msvc-170).
OpenAL delivery must satisfy the pinned release's
[COPYING](https://github.com/kcat/openal-soft/blob/1.25.2/COPYING), corresponding
source and applicable secondary notices. A COPYING file alone does not close
every binary redistribution obligation.

Framework's GPL does not relicense legacy DLLs, game assets, translations or
enhancement textures. Optional permissions remain separately attributed;
the core release must not require community packs.
