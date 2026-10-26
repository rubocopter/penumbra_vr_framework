# Framework release status

## Current public release — v1.0.11

**v1.0.11**, dated 2026-10-26, publishes the maintenance audit corrections:
installer recovery/uninstall, Overture essential input/recenter and Requiem
selection/interaction/resource lifecycle. It also includes the native level05
ScriptArea and slider-endpoint adaptations. New behavior is **host-tested**;
live/headset acceptance remains separate. See [release notes](releases/1.0.11.md).

Framework host regressions pass in Release and Debug (77 tests each); the
autonomous Overture product passes 298 checks. Installer fixtures cover current
and legacy recovery journals, deleted preferences, path conflicts, language
removal, rollback, verified ALUT restoration and Setup extraction. The
[v1.0.11 validation record](releases/1.0.11-validation.json) pins the clean build
source, all four artifacts and six shipped x86 PE identities. Exact final Setup
checks cover English/Spanish extraction, destination separators, topologies A–F
and upgrade/repair/exact restore from v1.0.10. The release tag adds documentation
over that build source; the packaged source commit remains explicit. Older
headset evidence cannot be transferred automatically to the new version.

The v1.0.0 installer, published 2026-09-29, had maintainer real installation,
removal and game-entry acceptance across the trilogy and fixture validation
of component ownership and restoration. The exact accepted artifact is
identified in the [installer record](releases/1.0.0-installer-ui-candidate.md).
This is not independent clean-PC or all-headset certification.

## Open problems and evidence boundaries

- **Requiem level04 sewer water/crouch:** unresolved. The diagnostic
  samples native capsule height; there is no confirmed recovery correction.
- **Requiem level05 buttons and filing cabinets:** v1.0.11 ScriptArea and
  slider-endpoint corrections have targeted host evidence only. Actual
  Quest 3 activation/progression and script behavior are unverified.
- **Requiem level03 throw and Examine/ladder:** the v1.0.10 host correction
  awaits headset usability and progression feedback.
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
[CHANGELOG.md](CHANGELOG.md) and [docs/releases](releases/1.0.11.md).
The [2026-10-10 installed-trilogy acceptance](releases/1.0.7-headset-acceptance.json)
and [v1.0.8 BP focused acceptance](releases/1.0.8-headset-acceptance.json)
retain their scoped hashes. Each version's validation JSON records its clean
source and exact artifacts. v1.0.1–v1.0.4 are superseded by v1.0.5;
their historical tags and records remain available. The
[release preparation audit](RELEASE_PREPARATION_AUDIT.md) is a historical
pre-v1.0.0 snapshot, not the current installer backlog.
