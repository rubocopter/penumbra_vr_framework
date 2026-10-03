# Observed and supported builds

Recognition, development validation and public support are separate states. A
known hash can be valid research/backend input without being a supported public
release.

## Known executables

| Game/build | Architecture | SHA-256 | Repository status |
| --- | --- | --- | --- |
| Overture retail baseline | x86 | `95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448` | Recognized retail baseline only |
| Overture VR Rework v0.1.0 | x86, LAA | `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71` | Proven historical/public Rework reference |
| Framework-owned Overture Release checkpoint | x86, LAA | `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1` | Included in public Framework v1.0.0; earlier headset regression plus v1.0 maintainer install/game-entry acceptance |
| Framework-owned Overture v1.0.1 | x86, LAA | `5DCCCD1EBCEACFBA10D12C964F7B529A711331915930B2D85ADD3FA29E429097` | Public Framework v1.0.1; exact installed hash verified and maintainer drawer/crank gameplay accepted in headset |
| Black Plague Steam canonical build | x86 PE32 | `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF` | Allowlisted active backend; substantial live/headset evidence, incomplete support |
| Black Plague verified LAA transform | x86 PE32, LAA | `DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196` | Exact transformed variant; installer path host-tested, no headset validation |
| Requiem Steam canonical build | x86 PE32 | `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2` | Active exact-build backend; representative interaction, first transition, portal refraction and final flashlight alignment headset-validated; broader regression open |
| Requiem verified LAA transform | x86 PE32, LAA | `577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955` | Recognized transformed variant; offline/host verified only |

Canonical Black Plague and Requiem binary evidence lives under `manifests/`.
Transformed-variant entries preserve the canonical semantic build identity.

## Validation terminology

Use these states consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

Current product-level evidence:

- **Overture:** Rework v0.1.0 remains the proven behavioral reference. The
  Framework-owned product builds and deploys autonomously, has an earlier
  functional headset/controller regression pass, and is included in public
  Framework v1.0.0. Public v1.0.1 adds maintainer acceptance of the exact new
  installer and an Overture headset play session including drawer/crank
  interaction. Broader current-build headset regression remains follow-up.
- **Black Plague:** tracked stereo, head/camera tracking and several body/input
  paths have real headset evidence; the managed Steam bootstrap has reached
  gameplay VR from Steam **Play**. Later host-tested changes still require the
  focused regressions listed in `ROADMAP.md`. The surface-contact Grab port has
  headset evidence from chair acquisition at different contact points. Special
  effects rendered correctly in the observed scene; its matching exact-build
  log records full-eye refraction copy/resize activity. This does not validate
  every smoke/material variant or held snap/throw/mechanism behavior. The same
  headset run exposed mini-hops against ventilation walls; the cause is unknown.
- **Requiem:** boot/menu/gameplay stereo, Sense locomotion, room-scale/body
  movement, crouch, visible hands and inventory/notebook have headset evidence.
  A representative Push puzzle, free-body `Grab=6` carry/snap/release sequence,
  jointed `Move=2` monolith puzzle and level-01 transition have headset evidence.
  The exact-build refraction-copy correction removed the portal's large
  rectangular/striped artifact with Refractions enabled in a headset clip;
  runtime telemetry confirmed the full-eye copy path and about 90 FPS, and HPL
  logged a successful exit on that tested build. Subtle motion ghosting remains
  unclassified. Stacked `Grab=6` blocks could shift during climbing and
  monolith turning felt resistant. Flashlight, glowstick and flare now stay
  with the visible hand during stick locomotion in a headset clip; the matching
  log records resolved-palm attachment and no raw-palm use. The final combined
  forward-facing, native-size flashlight attachment is headset-validated by
  the user's acceptance and image showing the beam at the housing edge.
  Slight finger penetration remains a known limitation.
  Nine intermittent startup dumps identify invalid `SDL_mutexP` arguments
  at Requiem return site `0x5A9DA3`. Delayed OpenVR did not prevent the fault;
  its upstream cause remains unknown. Free-body `Move=2`, throwing and broader
  progression still need independent headset evidence. The representative
  interaction milestone is concluded; this does not imply full-game support.

Framework v1.0.0 is the first public trilogy release for the allowlisted builds;
v1.0.2 is the current public patch. Its Index thumbstick correction is host-tested;
Index hardware and new-build headset acceptance remain pending. Publication does
not promote every capability, controller family or
game path to `supported`; the evidence levels above remain authoritative.

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

Inspect an executable without modifying it with:

```powershell
.\tools\Get-PenumbraBuildInfo.ps1 "C:\path\to\Penumbra.exe"
```

Use `-AsJson` when the result needs to be attached to an issue or research note.
