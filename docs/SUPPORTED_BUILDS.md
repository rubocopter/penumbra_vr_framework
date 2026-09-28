# Observed and supported builds

Recognition, development validation and public support are separate states. A
known hash can be valid research/backend input without being a supported public
release.

## Known executables

| Game/build | Architecture | SHA-256 | Repository status |
| --- | --- | --- | --- |
| Overture retail baseline | x86 | `95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448` | Recognized retail baseline only |
| Overture VR Rework v0.1.0 | x86, LAA | `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71` | Proven historical/public Rework reference |
| Framework-owned Overture Release checkpoint | x86, LAA | `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` | Initial Framework headset regression pass; no public Framework release |
| Black Plague Steam canonical build | x86 PE32 | `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF` | Allowlisted active backend; substantial live/headset evidence, incomplete support |
| Black Plague verified LAA transform | x86 PE32, LAA | `DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196` | Exact transformed variant; installer path host-tested, no headset validation |
| Requiem Steam canonical build | x86 PE32 | `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2` | Active exact-build backend; representative Push, free-body Grab, jointed Move and level transition headset-tested; portal presentation defect and other gates open |
| Requiem verified LAA transform | x86 PE32, LAA | `577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955` | Recognized transformed variant; offline/host verified only |

Canonical Black Plague and Requiem binary evidence lives under `manifests/`.
Transformed-variant entries preserve the canonical semantic build identity.

## Validation terminology

Use these states consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

Current product-level evidence:

- **Overture:** Rework v0.1.0 remains the proven public reference. The
  Framework-owned product builds and deploys autonomously and has an earlier
  functional headset/controller regression pass, but the current Framework
  candidate needs a new regression before release.
- **Black Plague:** tracked stereo, head/camera tracking and several body/input
  paths have real headset evidence; the managed Steam bootstrap has reached
  gameplay VR from Steam **Play**. Later host-tested changes still require the
  focused regressions listed in `ROADMAP.md`.
- **Requiem:** boot/menu/gameplay stereo, Sense locomotion, room-scale/body
  movement, crouch, visible hands and inventory/notebook have headset evidence.
  A representative Push puzzle, free-body `Grab=6` carry/snap/release sequence,
  jointed `Move=2` monolith puzzle and level-01 transition have headset evidence.
  Portal presentation has visible artifacts; stacked `Grab=6` blocks could
  shift during climbing and monolith turning felt resistant. Free-body `Move=2`,
  throwing, tool alignment and shutdown still need independent evidence.

None of the Framework-owned trilogy products is currently a supported public
Framework release.

## Support rules

- Unknown executable hashes fail closed.
- Binary signatures, RVAs, structure fields and calling conventions are
  exact-build evidence, never a generic HPL contract.
- Module-relative addresses remain required even when an image uses its
  preferred base.
- One native callsite has one Framework owner; additional consumers use explicit
  fan-out/status boundaries.
- Compile, unit-test and exact-image results do not imply live/headset support.
- Evidence from one game, build, capability or controller family does not
  automatically transfer to another.
- LAA transformation is permitted only for the recorded canonical build and
  belongs inside a verified transactional backup/rollback path.

Inspect a candidate executable without modifying it with:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 "C:\path\to\Penumbra.exe"
```

Use `-AsJson` when the result needs to be attached to an issue or research note.
