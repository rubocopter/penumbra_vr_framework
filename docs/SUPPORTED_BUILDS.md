# Observed and supported builds

This document distinguishes **observed** executables from **supported** backends. Recognition by the fingerprinting tool does not imply that a hook backend exists.

## Observed executables

| Game/build | Architecture | Size | SHA-256 | Status in this repository |
|---|---:|---:|---|---|
| Overture retail executable observed before VR deployment | x86 | 3,104,768 | `95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448` | Recognized only |
| Overture VR Rework v0.1.0 validated executable | x86 | 3,314,176 | `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71` | Maintained by the separate Rework repository |
| Black Plague Steam executable observed during initial audit | x86 | 3,338,240 | `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF` | Research target; unsupported |
| Requiem Steam executable observed during initial audit | x86 | 3,350,528 | `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2` | Research target; unsupported |

The first evidence manifest is stored at [../manifests/black_plague/FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.json](../manifests/black_plague/FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.json). A manifest records observations for one hash; its existence does not make that build playable or supported.

## Support rules

- A binary backend supports only hashes explicitly validated in-game.
- Module-relative RVAs are still required even when an observed executable uses a preferred fixed image base.
- Signatures must be derived from and tested against real binaries; placeholder byte sequences are not accepted.
- A signature should be unique in the intended executable and accompanied by contextual verification.
- Unknown hashes fail closed and generate diagnostics without installing or activating hooks.
- Modified executables are fingerprinted independently from their retail originals.

To inspect a candidate executable without modifying it:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 "C:\path\to\Penumbra.exe"
```

Use `-AsJson` when attaching the result to a research note or issue.
