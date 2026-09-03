# Binary research workflow

## Why initialized memory is required

The observed retail/Steam executables for Overture, Black Plague and Requiem enter through a final `.bind` section. For the known Black Plague and Requiem builds, more than 99.6% of `.text` differs after Steam has initialized the process. The on-disk sections have near-maximum byte entropy and do not disassemble as normal code; the initialized sections do.

Consequences:

- do not create HPL function signatures from the on-disk `.text`
- do not treat zero matches against the protected file as evidence that a runtime function is absent
- resolve imported API hooks from PE metadata, which remains available
- derive internal RVAs and signatures only from a hash-matched initialized process

## Inspect without dumping

Build the x86 Release targets, launch a catalogued game through Steam, and run:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --inspect <pid>
```

This compares `.text` in memory with the executable on disk and reports changed bytes and Shannon entropy. It does not write to or modify the game process.

## Create a local analysis image

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe `
  --capture-image <pid> `
  .\local\captures\game-memory-text.analysis.exe
```

The command copies the original executable and replaces its raw `.text` contents with the initialized section. The result is for local static analysis only:

- never run the reconstructed image
- never commit or publish it
- never derive a distributable patch containing original game code
- record only the minimal signatures, RVAs, layouts and behavioral conclusions needed by the backend

The reconstructed file retains the original PE headers/imports and `.bind` entry point. It is an analysis aid, not a runnable unpacked release.

## Accepting a symbol

An internal symbol can enter a build manifest only when its entry contains:

1. exact executable SHA-256
2. module-relative RVA
3. calling convention and argument evidence
4. short unpacked-memory signature and mask
5. match count within the intended section
6. source-level or behavioral basis for the proposed identity
7. status distinguishing static mapping from runtime hook validation

Prefer stable semantic structure over long compiler-specific byte sequences. Absolute imported-function addresses must be wildcarded if included in a future cross-build signature.
