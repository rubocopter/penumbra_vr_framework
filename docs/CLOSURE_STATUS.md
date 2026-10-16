# Framework release status

## Published state

Penumbra VR Framework **v1.0.0 was published on 2026-09-29**. The published
`PenumbraVR-Setup-1.0.0.exe` is the exact artifact recorded in
[the installer acceptance record](releases/1.0.0-installer-ui-candidate.md),
promoted unchanged so its automated and manual acceptance evidence remains tied
to the distributed binary.

The maintainer repeatedly exercised real install/uninstall flows with that EXE
and successfully entered Overture, Black Plague and Requiem. Automated fixtures
cover topology A–F, repair/recovery, component transitions, all eight bundled
controller profiles and exact restoration. These results are sufficient for the
initial public release while remaining distinct from an independent clean-PC or
full hardware/headset certification matrix.

Compile, host, live and headset evidence remain separate. Build identity and
validation levels remain owned by [SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md).

## Current release — 1.0.8

Framework v1.0.8 packages focused Black Plague interaction/progression and text
readability corrections. See [release notes](releases/1.0.8.md), the
[changelog](CHANGELOG.md) and the scoped
[maintainer acceptance record](releases/1.0.8-headset-acceptance.json).
The accepted cases include reported barrel/lever/bucket behavior, PC reach and
cursor/Accept, and centered enlarged Examine text. Earlier dog/altar and arm
puzzle completion evidence remains scoped in the capability ledger.

The standalone installer and matching committed source are built and verified
as a new versioned artifact set. Its [artifact validation record](releases/1.0.8-validation.json) records
60 Framework tests, 298 Overture checks, English/Spanish standalone extraction,
topology A-F and upgrade/repair/exact-restore checks from 1.0.7. The artifacts
come from clean source commit `aef43546c8a88b5d20da1beebf490f65fba6fa80`;
the release tag adds documentation only, including independently updated
maintenance policy. These checks remain separate from accepted local runtime hashes. Exact
new-Setup headset and independent clean-PC acceptance are not implied.

Equipped-light menu visibility, flashlight attachment, broader articulated
contact and physical-only/low-obstacle/leaning correction remain focused gates.
The included motion-attribution and step-normal changes have host evidence;
low-obstacle traversal is not declared fixed. Requiem's intermittent startup
cause and other-device limitations remain unchanged.

## Historical release — 1.0.7

Framework v1.0.7 publishes BP/Requiem release-edge recovery, transient movement
dead-zone preservation and analog-data guards, with Windows installer/diagnostics
CI. See [release notes](releases/1.0.7.md) and the detailed [changelog](CHANGELOG.md).

The maintainer accepted the installed local trilogy checklist on 2026-10-10.
The [acceptance record](releases/1.0.7-headset-acceptance.json) identifies the
tested runtime hashes, accepted menus/input/obstacle/visibility cases and
conditional cases that have no separately identified device evidence. This
closes those focused runtime gates without claiming full-game support or
headset acceptance of the newly versioned Setup artifact. Successful Requiem
startup feedback does not identify or fix its intermittent crash cause.

The [1.0.7 validation record](releases/1.0.7-validation.json) pins the clean
build source and exact Setup/source/report hashes, 59 Framework tests, 298
Overture checks, final English/Spanish Setup smoke, A-F topology and upgrade
from 1.0.6. The tag adds only provenance/status documentation to that build
source. These verified artifacts are published unchanged with matching source.
Build-report pending readiness fields remain build-time data; installed-local
headset acceptance stays scoped to its separate record.

## Historical release — 1.0.6

Framework v1.0.6 includes the maintainer-accepted BP/Requiem feedback fixes,
first-level English/Spanish VR Settings across the trilogy, and the host-tested
BP mixed stick/HMD obstacle correction. See the [release notes](releases/1.0.6.md)
and [preceding feedback acceptance](releases/1.0.6-feedback-acceptance.json).
The accepted preceding binaries do not validate the new shared menu extraction,
Overture/Requiem menu routes, mixed-obstacle correction or exact new installer.
Requiem's intermittent startup failure remains unresolved.

The [v1.0.6 validation record](releases/1.0.6-validation.json) pins the clean
build source, exact artifact/PE hashes, 59 Framework tests, 298 Overture checks,
English/Spanish final-EXE smoke, A-F topology and update from v1.0.5. The release
tag adds provenance documentation and concurrent README landing/video updates
to the build source; runtime/installer source is unchanged. The tested installer
and source archive are published without rebuilding. Build-report pending
headset/clean-PC fields remain historical build-time readiness data, separate
from the maintainer's publication authorization and preceding feedback acceptance.

## Historical consolidated release — 1.0.5

Framework v1.0.5 consolidates the controller, Overture interaction/input,
Black Plague Push/height/contact/pacing and installer improvements introduced
after v1.0.0. See the concise [release notes](releases/1.0.5.md).
The v1.0.1–v1.0.4 GitHub releases are superseded by this single update;
their tags, committed implementation and scoped validation records remain
historical evidence, not current downloads.

The maintainer accepted Overture gameplay on the preceding local changes and
completed the Black Plague tutorial without performance problems or freeze-related
box drops. BP's corrected hand-nudge units are host-tested; collision recoil
remains unresolved. The chemical-use shelf launch is native tutorial scripting.
Prior gameplay feedback does not establish exact new-installer headset acceptance.
The external Index reporter's finger mismatch remains non-blocking and unresolved.

Historical v1.0.1 installation and Overture drawer/crank acceptance remain tied
to their original artifact hashes in [the acceptance record](releases/1.0.1-acceptance.json).
Subsequent host evidence remains in the versioned validation records.

The [v1.0.5 validation record](releases/1.0.5-validation.json) identifies the exact
installer/source hashes, clean build source, 57 Framework tests, 298 Overture
checks, standalone English/Spanish Setup and topology A-F. Updates from v1.0.0
and v1.0.4 pass with exact payload/default hashes, repair and restoration. The
release tag adds provenance documents to the build source; the tested artifacts
are published unchanged. Build-time pending headset/clean-machine fields remain
historical readiness data, separate from the maintainer's publication decision.

## Remaining validation and release limitations

- Requiem's intermittent startup crash remains unresolved and unattributed. Do
  not claim reliable launch across all systems, hook SDL or modify gameplay
  speculatively without new discriminating evidence.
- The finite clean-machine matrix below remains useful post-release coverage;
  it was not completed on an independent PC before v1.0.0 publication.
- The installed trilogy checklist is accepted at its recorded hashes. Broader
  progression, device-specific conditional cases and exact new-Setup headset
  acceptance remain separate from that focused runtime result.
- Non-Sense controller families remain bundled compatibility profiles without
  personal hardware acceptance; their action omissions stay documented.

Only reproducible crashes, inability to launch VR, installation/dependency/input
failures, broken save/load/progression, incorrect files or blocking configuration,
destructive repair/uninstall and undocumented machine dependencies block v1.0.

## Implemented host gates

The [published single-EXE acceptance record](releases/1.0.0-installer-ui-candidate.md) identifies
the tested artifact/source commit, checksums, toolchain and passed A–F fixtures.

The 1.0.0 release supplies manifest-based Steam/manual discovery, essential
game-content/local x86 dependency checks, all-root install preflight, optional
packs/settings, independent O and shared B/R transactions, R-layer removal,
repair/recovery and passive final verification. Targeted settings restoration
preserves subsequent user edits. Diagnostics are bounded and redacted.
Release identity, pinned upstream sources/notices, a standalone Setup EXE and matching Source ZIP
and checksum/build-report generation are implemented. The automated fixtures
cover component transitions, tamper/conflict rejection, repair and exact restore;
they use synthetic runtime/content fixtures and do not establish independent
clean-machine or headset coverage. [SOURCE-AND-NOTICES.md](SOURCE-AND-NOTICES.md) distinguishes fixed-input
archive determinism from unproven full binary reproducibility.

## Known issues

- Slight finger/contact intersection, resistant Requiem monolith rings and
  accidental stacked-block displacement in observed sequences.
- The focused Black Plague physical-contact mini-hop correction and local
  wall/step regression are accepted. Broader collision recoil/comfort coverage
  remains open; investigate only a new reproducible case.
- Non-Sense hardware lacks personal validation. Vive defaults omit several
  essential binary-backend actions; WMR defaults omit holster/skeleton outputs.
  Exact coverage/status remains in
  [the controller ledger](TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution).

## Post-1.0 / optional improvements

Hand/material polish, mechanism comfort, enhanced visuals, richer audio policy,
broader effects/device coverage and general refactoring are optional follow-up
work. They do not block v1.0 unless a concrete failure meets the definition above.
Incomplete/unverified controller families retain honest limited status; they
must not receive a universal essential-input support claim.

## Post-release clean-install acceptance matrix

Every row remains **pending on an independent clean machine**. The published EXE
has maintainer real-install acceptance and automated topology coverage; this
table tracks broader deployment evidence rather than publication status.

| Scenario | Selected components | Detection/install/verification | Manual launch/VR/input/interaction/save-load | Repair/reinstall/uninstall |
| --- | --- | --- | --- | --- |
| A: Overture only | O plus required shared files | Pending | Pending | Pending |
| B: Black Plague only | B plus required shared files | Pending | Pending | Pending |
| C: Black Plague + Requiem | B + R layer, shared files once | Pending | Pending in both games | Pending |
| D: Overture + Black Plague | Two independent roots | Pending | Pending in both games | Remove one, verify other |
| E: complete trilogy | O root and B/R shared root | Pending | Pending in all games | Remove one root, verify other |
| F: add later | O then B/R; B then R; B/R then O | Pending | Pending in new game | Preserve existing target |

Use licensed, clean, recognized Steam files and fresh Windows without developer
dependencies. Record Windows/GPU, release hash, components, executable hashes
and dependency results. Include a non-system-drive Steam library and manual
folder selection. Apply only documented configuration, start SteamVR, then
**launch manually** through normal Steam Play. Reach gameplay, check stereo/
tracking, movement, essential UI/interaction and one save/load where applicable.
Representative deployment sanity suffices; do not repeat complete playthroughs.

Include locked/interrupted writes, unknown modifications, damaged owned files,
repeat repair/reinstall and exact restore with saves/custom settings preserved.
Keep installation-file verification separate from headset readiness and gameplay
results. Record the actual release tested; older headset evidence does not
automatically validate a newer package.
