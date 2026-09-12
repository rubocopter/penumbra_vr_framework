# Observed and supported builds

This document distinguishes **observed executables**, **validated development targets** and **supported releases**. Recognition by the fingerprinting tool or successful use by a research backend does not by itself make a public build supported.

## Observed executables

| Game/build | Architecture | Size | LAA state | SHA-256 | Status in this repository |
|---|---:|---:|---|---|---|
| Overture retail executable observed before VR deployment | x86 | 3,104,768 | Not recorded | `95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448` | Recognized retail baseline only |
| Overture VR Rework v0.1.0 validated executable | x86 | 3,314,176 | Enabled at link time | `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71` | Historical/proven Rework reference |
| Framework-owned autonomous Overture Release executable | x86 | 3,302,912 | Enabled/validated by product build | `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` | Initial functional SteamVR/headset/controller validation; not a supported public release |
| Black Plague Steam executable used by the exact-build backend | x86 PE32 | 3,338,240 | Disabled (`Characteristics=0x010F`) | `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF` | Allowlisted experimental backend/research target; stereo/head rotation and body-adapter boundaries have live/headset evidence, but gameplay support is incomplete |
| Black Plague exact LAA transform of the row above | x86 PE32 | 3,338,240 | Enabled (`Characteristics=0x012F`) | `DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196` | Recognized transformed variant of the same semantic build; host/offline verified only, not a supported release |
| Requiem Steam executable observed during initial audit | x86 PE32 | 3,350,528 | Disabled (`Characteristics=0x010F`) | `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2` | Static research evidence only; not an active backend |
| Requiem exact LAA transform of the row above | x86 PE32 | 3,350,528 | Enabled (`Characteristics=0x012F`) | `577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955` | Recognized transformed variant of the same semantic build; host/offline verified only, not an active backend |

Evidence manifests currently exist for the canonical [Black Plague](../manifests/black_plague/FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.json) and [Requiem](../manifests/requiem/B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2.json) builds. Their `executable.transformedVariants` entries record the exact LAA fingerprints above while preserving the canonical semantic build identity. A manifest or transformed-variant entry does not make that executable a supported release.

## Validation terminology

Use these states consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

Examples at the current checkpoint:

- the autonomous Overture Framework build has an initial functional `headset-validated` integration pass, but is not yet a supported release;
- the Black Plague body/collision, narrow body-adapter and default-off reconciliation shadow boundaries are `live-tested` on the allowlisted hash;
- Black Plague native stereo/yaw tracking have prior headset validation;
- the Black Plague collision-aware physical X/Z request is implemented and host-tested on the allowlisted hash, but is not yet live-tested;
- Black Plague active positional HMD/body reconciliation remains disabled pending live queue/injection/collision/reconciliation evidence for that request path;
- Requiem remains research/planned work.

## Support rules

- A binary backend installs hooks only for hashes explicitly allowlisted and validated for that evidence level.
- Module-relative RVAs remain required even when an observed executable uses a preferred fixed image base.
- Signatures must be derived from and tested against real binaries; placeholder byte sequences are not accepted.
- A signature should be unique in the intended executable and accompanied by contextual verification.
- Unknown hashes fail closed and generate diagnostics without installing or activating hooks.
- Initialized-process validation must account for Framework-owned hooks already installed by an earlier owner; one callsite still has exactly one owner, and consumers bind through verified fan-out/status rather than re-patching it.
- Known transformed executables are fingerprinted independently and map back to their canonical semantic build identity; all other modified hashes still fail closed.
- The repository recognizes the exact host-verified LAA variants above, but installed executables must eventually be changed only through the known-build-gated backup-and-rollback transaction. Recognition is not permission to patch arbitrary executables.

To inspect a candidate executable without modifying it:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 "C:\path\to\Penumbra.exe"
```

Use `-AsJson` when attaching the result to a research note or issue.
