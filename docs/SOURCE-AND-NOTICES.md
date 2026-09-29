# Candidate source and notices

Framework v1.0.0 is a release candidate. Public release acceptance remains in
[CLOSURE_STATUS.md](CLOSURE_STATUS.md); a successful package build does not close it.

The versioned release builder supplies `PenumbraVR-Source-1.0.0.zip` alongside
the installer. It contains the exact committed Framework/Overture/HPL source,
build scripts, dependency headers/link inputs and retained notices, plus the
upstream source archives pinned by `assets/deployment/redistribution.json`.
The setup archive also carries those upstream archives under `sources/`.
Never publish the setup alone without its corresponding source and checksums.

OpenAL Soft is dynamically loaded as an app-local `OpenAL32.dll`. The carried
x86 DLL is byte-identical to `bin/Win32/soft_oal.dll` in the official
[1.25.2 binary release](https://github.com/kcat/openal-soft/releases/tag/1.25.2).
Its LGPL COPYING and PFFFT notice are supplied; the unmodified upstream
[1.25.2 source](https://openal-soft.org/openal-releases/openal-soft-1.25.2.tar.bz2)
is pinned and supplied in full. Users can replace this library; installer hash
verification reports the change and does not silently adopt it.

AngelScript 2.7.1b's official SDK header matches the vendored header after
newline normalization. The complete
[official SDK](https://www.angelcode.com/angelscript/downloads.html) and retained
header notice accompany the release source. Legacy library compiler flags
have not been reconstructed; byte-for-byte binary rebuilds are not claimed.

The release uses game-owned legacy SDL, Cg, Newton, codecs and ALUT libraries
from the player's licensed installation, rather than harvesting system DLLs.
OpenVR's Valve license, HPL GPL/assets/shader notices, OALWrapper's license,
community translation readmes and texture provenance are retained in product
subfolders. Optional translation/texture components default off; their notices
remain available even when the component is not selected.

The Microsoft x86 app-local CRT is selected from the explicit official Visual
Studio redist directory by pinned hashes. It is a build input, not a DLL copied
from Windows. Distribution remains subject to that toolchain's redistribution
terms. The historical mixed dependency bundle is retained in the source tree
as build/provenance input; it does not establish blanket relicensing rights.
Review `THIRD_PARTY.md` and the recorded provenance before public publication.

## Building the versioned candidate

Use a clean committed checkout, Visual Studio 2022 C++ Win32 tools, CMake
3.25+, Windows PowerShell 5.1, the vendored OpenVR SDK, and an official VS
`VC/Redist/MSVC/<version>/x86/Microsoft.VC143.CRT` directory whose two DLLs
match `products/overture/runtime-dependencies.json`. Do not supply system DLLs.
Download the two official source inputs named and SHA-256 pinned by
`assets/deployment/redistribution.json` into a separate source-input directory.
The builder validates every input before creating artifacts.

```powershell
.\tools\Build-PenumbraVrRelease.ps1 `
  -OutputDirectory .\dist\v1.0.0-candidate `
  -RuntimeDirectory 'C:\path\to\official\x86\Microsoft.VC143.CRT' `
  -SourceDirectory 'C:\path\to\pinned-source-inputs'
```

The output directory must be new. The builder compiles/tests root Release and
the autonomous Overture product, then exports exact committed source, upstream
sources, notices, `PenumbraVR-Setup-1.0.0.zip`, `PenumbraVR-Source-1.0.0.zip`,
`build-report.json` and `SHA256SUMS.txt`. `-PackageOnly` skips repeating root
build/tests and records that limitation; it still builds/tests Overture.
The report records commit, toolchain, x86 PE imports/version/hash, pinned inputs
and acceptance gates. Archive timestamps and ordering are fixed; equality
requires identical file bytes. This does not claim deterministic compiler
output from different machines or reconstructed historical static-library flags.
The builder does not tag, upload, publish or launch games.
