# v1.0 release status

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

## 1.0.1 patch candidate

The local 1.0.1 candidate addresses Overture physical acquisition and installer
startup/Apply responsiveness. Its changes and validation limits are recorded in
[the patch notes](releases/1.0.1.md). Published v1.0.0 remains the public baseline
until the new artifact is accepted and published; earlier acceptance does not
certify the new drawer behavior or installer timing.

## Remaining validation and release limitations

- Requiem's intermittent startup crash remains unresolved and unattributed. Do
  not claim reliable launch across all systems, hook SDL or modify gameplay
  speculatively without new discriminating evidence.
- The finite clean-machine matrix below remains useful post-release coverage;
  it was not completed on an independent PC before v1.0.0 publication.
- Broader current-build headset regression, especially the documented Overture,
  Black Plague and Requiem edge cases, remains follow-up validation rather than
  evidence automatically inherited from older runs.
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
- Black Plague mini-hops while touching ventilation walls: currently a comfort
  issue of unknown cause; reclassify only with evidence meeting a blocker above.
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
