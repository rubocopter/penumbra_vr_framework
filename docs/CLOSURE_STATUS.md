# Framework release status

## Local progression candidate — v1.0.14

The 2026-10-28 candidate adds the Requiem remaining-level interaction wiring
described in [the notes](releases/1.0.14.md). It is not a stable/full-playthrough
acceptance claim. Actual authored cable/rope/connection and ending completion
remain headset gates; water/crouch recovery and the SDL crash remain unresolved.
Public release status below continues to identify the preceding public artifact
until a separate publication is verified.

The exact local installer, matching source and checksums are pinned in the
[1.0.14 artifact record](releases/1.0.14-validation.json), built from clean
`1592ba45232541b2987dd6526c801e0527e3abac`. Both Framework configurations pass
83/83 tests; Overture passes 298 checks. EN/ES standalone extraction, payload
hashes, source bytes, topology A–F, 1.0.13 upgrade/repair/exact restoration and
20 uninstall-safety cases pass in isolated roots. This remains a local candidate.

## Current public maintenance candidate — v1.0.13: headset validation pending

**v1.0.13**, dated 2026-10-27, adds bounded native inventory pickup and ScreenGui
controller input for Requiem level08, retaining all preceding maintenance
corrections. **The developer has not tested this version in-game or in a VR
headset. Both level08 changes require headset validation; completion of the
level has not been demonstrated.** See [release notes](releases/1.0.13.md).

The interaction implementation passed 79 Framework host tests in Debug and
Release and 298 Overture Release checks. Final versioned installer/package
verification is recorded separately in the release artifact record; these host
results do not establish actual gameplay or independent clean-PC acceptance.
Matching committed source, dependency sources, provenance and checksums accompany
the candidate. Previous headset evidence does not transfer to this build.

The [artifact record](releases/1.0.13-validation.json) pins four final files,
the exact clean build commit and six shipped x86 PE identities. The final Setup
passed bilingual standalone extraction, embedded payload hashes, topology A-F,
upgrade from v1.0.12, repair and exact restoration in isolated roots. The source
ZIP matches every file of the build commit and its pinned upstream inputs.
Hosted CI passed on that build source. The release tag adds only documentation.
No installed game was modified and no game was launched during validation.

The v1.0.0 installer, published 2026-09-29, had maintainer real installation,
removal and game-entry acceptance across the trilogy and fixture validation
of component ownership and restoration. The exact accepted artifact is
identified in the [installer record](releases/1.0.0-installer-ui-candidate.md).
This is not independent clean-PC or all-headset certification.

## Open problems and evidence boundaries

- **Requiem level04 sewer water/crouch:** unresolved. The diagnostic
  samples native capsule height; there is no confirmed recovery correction.
- **Requiem level08 orb/keypad:** corrections are host-tested; actual acquisition,
  GUI operation and progression remain pending in-headset confirmation.
- **Requiem levers and sphere throwing:** Caelus reports both working perfectly
  after the latest corrections; no installed hash was supplied. This scoped
  feedback does not validate every mechanism or this candidate's artifacts.
- **Requiem level05 buttons and filing cabinets:** native-area and slider fixes
  retain host evidence; actual activation/progression remains unverified.
- **Requiem level03/07 progression:** broader puzzle outcomes remain separate;
  the level07 sphere's physical lip stoppage has no established cause.
- **Requiem intermittent startup crash:** SDL mutex failure observed, root
  cause and correction unknown. No speculative fix is warranted.
- **Other known limits:** minor finger/contact intersection and monolith
  resistance, incomplete broader interaction/effects coverage, Index finger
  mismatch and other-controller physical validation.
- **Deployment breadth:** local PS VR2 runtime and installer fixture results
  are not full-playthrough, independent clean-PC or new-Setup headset acceptance.

[TRILOGY_PARITY_PLAN.md](TRILOGY_PARITY_PLAN.md) owns capability evidence;
[VR_HEADSET_TEST_CHECKLIST.md](VR_HEADSET_TEST_CHECKLIST.md) owns acceptance
steps; [SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md) owns executable hashes.
[ROADMAP.md](ROADMAP.md) owns the maintenance and release decision policy.
A build or test cannot substitute for live or headset validation.

## Distribution and installation

The unified installer recognizes supported game files, owns Overture separately
from the shared Black Plague/Requiem root, verifies dependencies, distributes
eight controller profiles and optional localization/settings, and supports
transactional update, repair and exact original-file restoration. Host fixtures
cover topology A–F, modified-file rejection, repair and component transitions.
See [installer design](INSTALLER_DESIGN.md),
[dependency audit](RUNTIME_DEPENDENCIES.md) and
[source/notices](SOURCE-AND-NOTICES.md) for the authoritative contracts.

### Independent clean-PC acceptance (still pending)

| Scenario | Distinct installation risk | Independent PC |
| --- | --- | --- |
| A | Overture only and launcher restoration | Pending |
| B | Black Plague only and shared-root ownership | Pending |
| C | Black Plague + Requiem, one shared redist | Pending |
| D | Overture + Black Plague, independent roots | Pending |
| E | Entire trilogy, selective removal and retention | Pending |
| F | Add components later in either order, repair/restore | Pending |

The maintainer's accepted real installations and host fixtures are separate
evidence. A new acceptance record should pin the Setup and game hashes,
Windows/GPU/dependencies, licensed Steam/manual target, components and
configuration, manual Steam Play launch, VR input and relevant save/load or
progression plus restoration. Do not infer one game's acceptance from another.

## Historical releases

Historical changes and individual version notes are retained in
[CHANGELOG.md](CHANGELOG.md) and [docs/releases](releases/1.0.13.md).
The [2026-10-10 installed-trilogy acceptance](releases/1.0.7-headset-acceptance.json)
and [v1.0.8 BP focused acceptance](releases/1.0.8-headset-acceptance.json)
retain their scoped hashes. Each version's validation JSON records its clean
source and exact artifacts. v1.0.1–v1.0.4 are superseded by v1.0.5;
their historical tags and records remain available. The
[release preparation audit](RELEASE_PREPARATION_AUDIT.md) is a historical
pre-v1.0.0 snapshot, not the current installer backlog.
