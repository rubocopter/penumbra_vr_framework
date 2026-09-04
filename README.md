# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: Black Plague bootstrap and render-path research.**

This repository does not currently contain a playable mod or installer. It contains a narrow OpenVR session component and the first verified Black Plague research probe: the probe validates one exact executable build, observes `SDL_GL_SwapBuffers` and OpenGL matrix setup, and has identified the gameplay projection and moving view matrix at the OpenGL boundary. Transactional eye targets have been created inside the game at OpenVR's runtime-recommended dimensions, a diagnostic FBO has survived 120 controlled extra world passes, and two OpenVR-derived eye views and asymmetric projections have survived 60 controlled stereo frames with byte-exact camera restoration. Three subsequent runs submitted 900 static stereo frames to the headset; native full-view stereo was observed at the deliberately low diagnostic resolution. Deactivation restores every hook but deliberately keeps the research DLL resident until the game exits. Head tracking and a persistent playable mode are not implemented yet.

SteamVR still shows Black Plague on a virtual cinema screen during normal execution. That is desktop mirroring, not stereo VR. The experimental stereo-submission command temporarily switches to native headset presentation by passing two rendered eye textures to the OpenVR compositor, then returns to the normal desktop path after 300 frames.

The existing, playable Overture implementation remains in [rubocopter/penumbra_vr_rework](https://github.com/rubocopter/penumbra_vr_rework). It is the behavioral reference for this project; it has not yet been copied into this repository.

## Project principles

- One product and installer for users; game-specific backends internally.
- Detect exact executable builds and fail closed on unknown versions.
- Keep the VR runtime independent from game addresses and object layouts.
- Treat Overture source integration and binary hooks as different adapters.
- Require evidence for every signature, RVA, structure offset, and calling convention.
- Do not mark a backend supported until it reaches an in-headset milestone.
- Preserve upstream attribution and licenses when code is eventually imported.

## Intended components

```text
Penumbra VR
├── runtime                 OpenVR, tracking, actions, configuration and logging
├── bootstrap               Host identification and backend loading
├── backends
│   ├── overture_source     Source-level HPL1/Overture integration
│   ├── black_plague_binary Version-specific binary integration
│   └── requiem_binary      Version-specific binary integration
├── installer               Detection, deployment, backup and rollback
├── manifests               OpenVR action manifests
├── bindings                Controller bindings
├── tools                   Build fingerprinting and research utilities
└── tests                   Host-independent verification
```

These are architectural boundaries, not claims that the components already exist. Directories and build targets will be added when their first working implementation is ready.

## Build and current probe

The native targets are Windows/x86 because the observed game executables are 32-bit:

```powershell
cmake --preset vs2022-win32
cmake --build --preset release
ctest --preset release
```

OpenVR support is optional at configure time. Point `PENUMBRA_VR_OPENVR_SDK` at an unpacked SDK containing `headers/openvr.h` and `bin/win32/openvr_api.dll`; the SDK remains external to this repository:

```powershell
cmake --preset vs2022-win32 `
  -DPENUMBRA_VR_OPENVR_SDK=C:\path\to\openvr-2.15.6
```

Black Plague must currently be launched through Steam. Once it is running, attach or remove the probe with:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --attach <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-openvr-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-world-duplication <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-matrices <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-submission <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <process-id>
```

The OpenVR-sized target, controlled world-duplication and stereo commands are experimental. The stereo-matrix command renders only to hidden 512×512 diagnostic targets. The stereo-submission command displays 300 static stereo frames in the headset at that same low resolution, without head tracking, before restoring the normal desktop path. OpenVR is only available in builds configured with `PENUMBRA_VR_OPENVR_SDK`. Logs are written to `%LOCALAPPDATA%\PenumbraVR\logs`. This is a developer probe, not an end-user launcher.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime or hook code.

## Licensing

No project license has been selected yet and no third-party source has been imported. Overture/HPL1-derived work is GPLv3, while OpenVR uses Valve's BSD-style license. The licensing and attribution plan must be settled before shared code is moved from the Overture project. See [THIRD_PARTY.md](THIRD_PARTY.md).
