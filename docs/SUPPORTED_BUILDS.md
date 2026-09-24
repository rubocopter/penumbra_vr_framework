# Observed and supported builds

Recognition, development validation and public support are separate states. A
known hash may be usable for research or an exact-build backend without being a
supported public release.

## Known executables

| Game/build | Architecture | SHA-256 | Repository status |
| --- | --- | --- | --- |
| Overture retail baseline | x86 | `95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448` | Recognized retail baseline only |
| Overture VR Rework v0.1.0 | x86, LAA | `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71` | Proven historical/public Rework reference |
| Framework-owned Overture Release checkpoint | x86, LAA | `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` | Initial Framework headset regression pass; no public Framework release |
| Black Plague Steam canonical build | x86 PE32 | `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF` | Allowlisted active backend; substantial live/headset evidence, incomplete support |
| Black Plague verified LAA transform | x86 PE32, LAA | `DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196` | Recognized transformed variant; offline/host verified only |
| Requiem Steam canonical build | x86 PE32 | `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2` | Static research/configuration preparation only |
| Requiem verified LAA transform | x86 PE32, LAA | `577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955` | Recognized transformed variant; offline/host verified only |

Canonical Black Plague and Requiem evidence lives in the corresponding files
under `manifests/`. Their transformed-variant entries preserve the canonical
semantic build identity.

## Validation terminology

Use these states consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

At the current checkpoint:

- **Overture:** the original Rework v0.1.0 is the proven public product baseline.
  The Framework-owned Overture product builds/deploys autonomously and has an
  initial functional headset/controller regression pass, but the Framework has
  no public release yet.
- **Black Plague:** tracked stereo, camera/head tracking and multiple body/input
  paths have real headset evidence. The normal-Steam bootstrap has also reached
  gameplay VR from the standard Steam **Play** button. The current development
  candidate contains later host-tested fixes for presentation pacing, hand asset
  deployment, physical-motion reconciliation, map-start yaw, particle refresh,
  native settings labels, same-sample held-palm refresh, inventory
  drag/default/context routing and controller-aim `UseItem` targeting. The
  current host-tested candidate additionally rejects negative interaction-ray
  distances seen in live acquisition logs, restores the context edge through
  BP's existing native context/widget implementation despite its top-level
  right-button discard, and reports live particle refresh counters; those fixes
  must be revalidated before their evidence level is promoted. The development
  DLL used in the 2026-09-24 headset pass also uses a deterministic
  free-prop grip, preserves held objects through snap yaw, presents inventory
  and notebook in the stereo world. That pass showed an inventory closing flash,
  misplaced notebook, subtitles visible in recording but not in the headset,
  difficult mechanism pickup and a death-to-menu crash. A later host-tested
  candidate, now installed for the next headset pass, corrects the null-body
  crouch transition, broadens selected Move
  pickup to the proven 0.40 m reach, restores Rework notebook/subtitle placement,
  retains the closing UI panel for one frame, and disables BP Enhanced Visuals.
  It has no headset validation yet. Mechanism motion, player repulsion, held-hand
  penetration, context text and blue/red effects remain open live observations.
- **Requiem:** executable identity, LAA transform, localization and recommended
  game settings are prepared; there is no active gameplay VR backend.

Capability-level state is maintained in `TRILOGY_PARITY_PLAN.md` rather than
expanded here into session chronology.

## Support rules

- Unknown executable hashes fail closed.
- Binary signatures/RVAs/calling conventions are exact-build evidence, never a
  generic HPL contract.
- Module-relative addresses remain required even when an image uses its preferred
  base.
- One native callsite has one Framework owner; additional consumers use explicit
  fan-out/status boundaries.
- A compiled test or exact-image check does not imply live/headset support.
- Evidence from one game, build or controller family does not automatically
  transfer to another.
- LAA transformation is permitted only for the recorded canonical build and
  ultimately belongs inside the transactional installer with verified backup and
  rollback.

Inspect a candidate executable without modifying it with:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 "C:\path\to\Penumbra.exe"
```

Use `-AsJson` when the result needs to be attached to an issue or research note.
