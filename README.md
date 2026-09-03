# Penumbra VR

**One VR framework for the Penumbra trilogy.**

Penumbra VR aims to provide one installer and one user-facing mod for:

- Penumbra: Overture
- Penumbra: Black Plague
- Penumbra: Requiem

Internally, each game is allowed to use the integration method it actually needs. Overture can be built from the released source code, while Black Plague and Requiem require version-specific binary integration.

## Status

**Pre-alpha: architecture validation and binary research.**

This repository does not currently contain a playable mod, an installer, a VR runtime, or working game hooks. The first target is a minimal Black Plague proof of concept: load code into one known executable build, establish a stable frame hook, and record enough camera/rendering information to plan stereo rendering.

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

## Current work

The repository currently provides documentation and a read-only executable fingerprinting tool:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 `
  "C:\path\to\Penumbra.exe", `
  "C:\path\to\Requiem.exe"
```

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md), and [docs/SUPPORTED_BUILDS.md](docs/SUPPORTED_BUILDS.md) before adding runtime or hook code.

## Licensing

No project license has been selected yet and no third-party source has been imported. Overture/HPL1-derived work is GPLv3, while OpenVR uses Valve's BSD-style license. The licensing and attribution plan must be settled before shared code is moved from the Overture project. See [THIRD_PARTY.md](THIRD_PARTY.md).
