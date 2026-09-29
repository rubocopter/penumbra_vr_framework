# v1.0 release closure

## Definition of done

Penumbra VR Framework v1.0.0 is release-ready when a clean supported installation
of Overture, Black Plague, Black Plague + Requiem, or the complete trilogy can
be detected, installed, configured, verified, launched, repaired and
uninstalled without relying on undocumented files or machine-specific state
from the development PC.

The current task accepts gameplay as the working baseline and prioritizes
release engineering. Non-blocking polish does not reopen v1.0 development.
Compile, host, live and headset evidence remain distinct. The candidate is
**not yet release-ready**. Build support remains owned by
[SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md).

## Release blockers

- Finish public-publication provenance review in [SOURCE-AND-NOTICES.md](SOURCE-AND-NOTICES.md)
  and validate the delivered app-local OpenAL on the current B/R candidate.
- Demonstrate clean-user install/update/repair/interruption recovery/uninstall
  using the finite matrix below; development fixtures alone cannot close it.
- Resolve or obtain new discriminating validation of the documented Requiem
  intermittent startup crash before claiming reliable Requiem launch. Its cause
  remains unknown; do not hook SDL or modify gameplay speculatively.

Only reproducible crashes, inability to launch VR, installation/dependency/input
failures, broken save/load/progression, incorrect files or blocking configuration,
destructive repair/uninstall and undocumented machine dependencies block v1.0.

## Implemented host gates

The [single-EXE candidate acceptance record](releases/1.0.0-single-exe-candidate.md) identifies
the tested artifact/source commit, checksums, toolchain and passed A–F fixtures.

The 1.0.0 candidate supplies manifest-based Steam/manual discovery, essential
game-content/local x86 dependency checks, all-root install preflight, optional
packs/settings, independent O and shared B/R transactions, R-layer removal,
repair/recovery and passive final verification. Targeted settings restoration
preserves subsequent user edits. Diagnostics are bounded and redacted.
Release identity, pinned upstream sources/notices, a standalone Setup EXE and matching Source ZIP
and checksum/build-report generation are implemented. The automated fixtures
cover component transitions, tamper/conflict rejection, repair and exact restore;
they use synthetic runtime/content fixtures and do not close clean-machine or
headset gates. [SOURCE-AND-NOTICES.md](SOURCE-AND-NOTICES.md) distinguishes fixed-input
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

## Clean-install acceptance matrix

Every row is **pending on a clean machine**, not a completed test record.

| Scenario | Selected components | Detection/install/verification | Manual launch/VR/input/interaction/save-load | Repair/reinstall/uninstall |
| --- | --- | --- | --- | --- |
| A: Overture only | O plus required shared files | Pending | Pending | Pending |
| B: Black Plague only | B plus required shared files | Pending | Pending | Pending |
| C: Black Plague + Requiem | B + R layer, shared files once | Pending | Pending in both games | Pending |
| D: Overture + Black Plague | Two independent roots | Pending | Pending in both games | Remove one, verify other |
| E: complete trilogy | O root and B/R shared root | Pending | Pending in all games | Remove one root, verify other |
| F: add later | O then B/R; B then R; B/R then O | Pending | Pending in new game | Preserve existing target |

Use licensed, clean, recognized Steam files and fresh Windows without developer
dependencies. Record Windows/GPU, candidate hash, components, executable hashes
and dependency results. Include a non-system-drive Steam library and manual
folder selection. Apply only documented configuration, start SteamVR, then
**launch manually** through normal Steam Play. Reach gameplay, check stereo/
tracking, movement, essential UI/interaction and one save/load where applicable.
Representative deployment sanity suffices; do not repeat complete playthroughs.

Include locked/interrupted writes, unknown modifications, damaged owned files,
repeat repair/reinstall and exact restore with saves/custom settings preserved.
Keep installation-file verification separate from headset readiness and gameplay
results. Record the actual candidate tested; older headset evidence does not
automatically validate a newer package.
