# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: Black Plague bootstrap and render-path research.**

This repository does not currently contain a playable mod, an installer, or a VR runtime. It does contain the first verified Black Plague research probe: it validates one exact executable build, loads without modifying the installation, observes `SDL_GL_SwapBuffers` and OpenGL matrix setup, and has identified the gameplay projection and moving view matrix at the OpenGL boundary. Transactional OpenGL eye-target allocation is tested both on a standalone WGL context and through a transient create/resize/destroy cycle inside the game. No eye target is used for world rendering yet. The probe restores every hook and unloads cleanly; it does not render VR yet.

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

Black Plague must currently be launched through Steam. Once it is running, attach or remove the probe with:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --attach <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-eye-targets <process-id>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <process-id>
```

Logs are written to `%LOCALAPPDATA%\PenumbraVR\logs`. This is a developer probe, not an end-user launcher.

The repository also provides a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), [docs/BLACK_PLAGUE_PROBE.md](docs/BLACK_PLAGUE_PROBE.md), [docs/BINARY_RESEARCH.md](docs/BINARY_RESEARCH.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime or hook code.

## Licensing

No project license has been selected yet and no third-party source has been imported. Overture/HPL1-derived work is GPLv3, while OpenVR uses Valve's BSD-style license. The licensing and attribution plan must be settled before shared code is moved from the Overture project. See [THIRD_PARTY.md](THIRD_PARTY.md).
