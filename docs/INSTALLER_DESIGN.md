# Unified installer design

Penumbra VR should install as one product while preserving the integration model
required by each game. The combined Overture and shared Black Plague/Requiem
candidate passes deterministic packaging, install/repair/restore and GUI
fixture checks. Production user acceptance and the release audit remain open. This file
owns the durable installer contract, not incremental packaging history.

## User-facing flow

```text
discover installation
        |
fingerprint executable
        |
match known game/build
        |
plan executable + payload + settings changes
        |
backup -> apply -> verify
        |
record state for repair/uninstall
```

A filename is only a hint. SHA-256/build manifests determine identity. Unknown
hashes fail closed and are never patched or injected.

`tools/Get-PenumbraInstallations.ps1` performs read-only Steam-library and
manual-folder discovery. Ambiguous compatible copies require explicit user
selection; malformed/unknown executables remain visible as non-installable
results rather than aborting discovery.

## Transaction rules

Every write must be fully planned before the first modification. A transaction
records detected game/build, original hashes, verified backups, intended
payload, expected installed hashes and restore ownership.

Apply must:

1. revalidate target identity immediately before writes;
2. create and hash-verify backups;
3. prepare transformed/replacement files without destroying the only original;
4. verify prepared executable/payload state;
5. replace files atomically where practical;
6. publish recoverable transaction state before destructive writes;
7. record the completed installation only after every step succeeds; and
8. restore the verified prior state on failure.

Repair validates the recorded state and replaces only owned damaged/missing
content. Restore/uninstall verifies originals and refuses to overwrite
unexpected third-party/user modifications silently. Interrupted transactions
must be recoverable from their durable journal.

## Executable policy

Overture uses a Framework-owned rebuilt source executable. Black Plague and
Requiem are exact-build binary integrations.

The observed Black Plague/Requiem executables are 32-bit PE32 without Large
Address Aware. `src/deployment/pe_large_address.*` owns the one-bit LAA
transform; canonical/transformed hashes are recorded in the build catalogue and
manifests.

The installer may apply LAA only when the canonical build is allowlisted, a
verified original backup exists, only `IMAGE_FILE_LARGE_ADDRESS_AWARE` changes,
the transformed hash matches the recorded variant and rollback restores the
exact canonical executable. A recognized transformed executable is not runtime
support evidence.

## Payload ownership

`assets/deployment/manifest.json` is the source of truth for installable payload
ownership. Development deploys and the final installer must consume the same
logical IDs, source classes, destinations and restore policy.

The Black Plague/Requiem executables share one `redist` directory and one ALUT
bootstrap proxy. Their probe DLLs, `openvr_api.dll`, OpenVR actions/bindings,
imported hand texture, both Spanish localizations and managed HRTF config are
owned by one transaction and one restore journal. Requiem payloads are enabled
only when its companion executable has a recognized exact hash; the proxy
dispatches to the matching probe after hashing the process image.

`assets/localization/manifest.json` owns Black Plague/Requiem localization
payloads and hashes. `assets/openvr` owns shared action/binding assets.
`assets/settings/recommended.json` owns optional maintainer-tested settings;
machine-specific resolution, saves, key bindings and personal VR calibration
remain user state.

## Current prototype baseline

The existing host-tested packaging code provides the implementation baseline to
preserve while the final installer is built:

- deterministic Overture and Black Plague candidate ZIPs with payload hash
  verification;
- exact-build discovery/selection and opt-in verified LAA transformation;
- transactional install/upgrade/restore with preflight validation and rollback;
- durable recovery journals for interrupted Overture and Black Plague writes;
- explicit repair of recorded owned payloads while rejecting foreign changes;
- a combined candidate that can select/install/repair/recover/restore Overture
  and the shared Black Plague/Requiem root and logs operations as JSONL;
- complete package verification before discovery or writes.

Controller distribution checks compare `vr/actions.json` and all eight default
graphs against their owning source tree by SHA-256 in both packaged products,
isolated installs of all three games and repairs of both roots. Deleted Vive
and corrupted Sense defaults are restored by repair. Metadata rejects a dropped
family, binding-path drift, lost implemented actions and incompatible direct
output types. Controller hardware validation is a separate gate; layout limits
are recorded in `TRILOGY_PARITY_PLAN.md`.

The shared transaction's rollback, install, repeated repair and exact restore
pass a fixture with both exact executables, preserving pristine localization
backups and deployment ownership. The combined package is deterministic and
its selector deduplicates the shared root. The VBS launcher starts the Windows
Forms GUI with a hidden PowerShell console; packaged control creation and
read-only discovery of pristine Overture plus BP/Requiem pass host checks.
The GUI blocks repeated operations while a transaction runs. Runtime/headset
support and final UI acceptance remain separate gates.

## Remaining production work

The release-engineering scope and finite acceptance gate are owned by
[CLOSURE_STATUS.md](CLOSURE_STATUS.md). Inspected dependency and packaging gaps
are in [RUNTIME_DEPENDENCIES.md](RUNTIME_DEPENDENCIES.md) and
[RELEASE_PREPARATION_AUDIT.md](RELEASE_PREPARATION_AUDIT.md).

- exercise the graphical install/repair/uninstall flow on a clean user setup;
- validate runtime readiness independently of passive OpenVR registration checks;
- complete the dependency/redistribution license audit; packaged notices already include both translations;
- validate clean install, upgrade, repair, interruption recovery, external
  modification handling and exact restore for every supported product; and
- accept the existing graphical product selection/UI on a clean user setup
  before production distribution.

## v1.0 extension — approved and implemented, acceptance pending

Keep the existing PowerShell transactions and Windows Forms front end. Extend
their inputs/verification rather than introduce another ownership engine.
The public download is one `PenumbraVR-Setup-1.0.0.exe`. The self-contained
Windows bootstrapper embeds the private combined package and project banner,
verifies its checksums, starts the existing Forms GUI with a hidden PowerShell
process, and removes the extracted temporary directory after the UI exits.
No separate .NET runtime or repository checkout is required. ZIPs remain
internal packaging inputs; source archives and reports are maintainer artifacts.
`Package-PenumbraVrSetup.ps1` builds the standalone EXE, while the versioned
release builder also exports matching source, notices, checksums and a report.

English is the default UI language. The three optional Spanish translations
start unchecked for new installations. Recorded installed components remain
selected when the installer opens. Choosing Español selects translations
automatically; their checkboxes remain editable. Each product records its own
localization ownership. Repair uses recorded selections.
Returning to English changes interface copy without deselecting owned or
explicitly selected translations. Maintenance selection survives refresh and
language changes; Modify components transfers those target roots to Install.
The main Install button preflights all selected roots
and applies them; maintenance operations retain their review step. Games are
launched manually through Steam after installation.

### Installation and maintenance presentation

The title and discreet language selector precede a full-width project banner.
The default Install tab shows one card per detected Overture or Black Plague
root, its path, detection result and recorded Framework version/status. Spanish
translations and Overture enhancement textures belong to their game's card.
Requiem detection and its VR/translation selections are nested inside the
matching Black Plague card; they never share selections across separate copies.

Recommended VR graphics settings are checked by default, with the existing
configuration backup explained alongside them. Advanced options contain the
default/current-user configuration scope. Refresh and Locate game remain
secondary; one primary button reads Install, Update, Apply changes or Repair
installation according to the selected roots. Repair restores recorded
components before they can be modified. Interrupted installations block the
normal action and direct users to maintenance.

The Maintenance tab reports installed components separately from draft choices.
It provides Modify components, Verify VR installation, Repair installation and
Uninstall, with diagnostics ZIP, installation log and Restore interrupted
installation under troubleshooting. The latter restores the verified snapshot
before an interrupted transaction; it is not a general backup browser.
Uninstall opens component selection. Removing Black Plague includes dependent
Requiem, while selecting only Requiem uses the internal expansion-removal
transaction and keeps Black Plague, including its recorded Spanish translation.
Both explicit and numbered selector routes preserve that base selection.
No standalone technical removal button is
shown on Install.

Every selected root preflights before the first write, even when different
copies of a game have different optional choices. Execution still consists of
independent root transactions; a later failure reports earlier completions.
Installation completion is followed by a read-only VR check of bindings,
dependencies, owned files and configuration. A failed check shows the affected
items instead of claiming complete readiness. The installer never launches a
game. Read-only fixture previews and live WinForms event checks exercise both
languages, nested selection, refresh, page changes and primary-button layout.

### Components and validation

Represent shared framework, O support, B support and R support explicitly.
Shared policy remains linked into each product; shared payloads install once
per consuming root. R requires validated B plus expansion content. Expose it
as a layer under B: add/remove R while retaining B; removing B also removes
dependent R. O stays independent. Record component versions and original/
installed hashes. Do not introduce a global game DLL directory.

Core excludes community translations and selected enhancement textures by
default; retain them only as explicit optional selections with their notices.
Preserve indispensable O VR shaders/models/map adaptations. Directory location
alone does not distinguish required overlays from texture enhancements.

Resolve Steam roots from registry/metadata/fallback and all library folders;
read app manifests for actual installation directories. Validate manual root,
redist and EXE choices with exact hashes, local dependencies and essential
game config/content. Show incomplete and unknown builds with actionable reasons.
Recognition must not become an unearned public-support claim.

### Dependencies and final-state verification

Consume a machine-readable dependency inventory with pinned distributable
inputs. Supply an app-local `OpenAL32.dll` compatibility proxy and private pinned
`PenumbraVR_OpenALSoft.dll` to B/R after exact redistribution
obligations are satisfied; preserve verified originals and stop on unknown
conflicts. The proxy maps empty, Generic Software and Generic Hardware playback
device requests to the default device, following Overture's proven fallback
boundary; explicit modern device names remain unchanged. Keep original OpenAL
backup filenames stable so prior ZIP ownership and recovery journals can upgrade
and restore safely. Retain O CRT pins. Detect SteamVR and game-owned legacy dependencies,
including dynamic codecs, with precise guidance. Never copy system DLLs or
switch OpenXR. Removal/recovery must work without SteamVR.

Preflight all selected roots before writes; retain independent root commits
and explicit partial-result reporting. Extend existing journals. Verify final
component graph, owned hashes, backups, dependencies, actions/bindings and
configuration. Expose read-only structured per-game verification. Say
"installation files verified"; headset readiness and manual gameplay acceptance
remain separate results.

### Settings, recovery and diagnostics

Preview an optional targeted game XML patch using an allowlist from existing
recommendations. Preserve resolution, personal calibration and experimental
mirror choices. Separately, install/update/repair checks the selected
`Game/LanguageFile` against the final language payload, including staged or
retired optional translations. Only a missing language falls back to the game's
bundled English file (`English_exp.lang` for Requiem). This launch-compatibility
check runs even when graphical recommendations are unchecked, and uses the
selected default/user-file scope (GUI defaults to both). Valid existing language
choices remain unchanged. Back up changed attributes, stage/parse and
journal changes. Removal restores only values still equal to installer-applied
values; preserve later user edits. Keep corrected B/R post-effects/refractions.

Configuration records distinguish retained restore ownership from attributes
actually scheduled for this invocation. Repeated repair must not replay an old
English fallback over a later valid language selection. Original absent-language
preferences remain recorded for conditional restoration; no configuration
folder, savegame or calibration is deleted.

Integrate GUI recovery and preview what explicit repair replaces. Preserve
SteamVR custom bindings and expose conflicts. Log version, plan, dependencies,
files/config, backups, failures/rollback and verification without debug spam.

Provide a bounded diagnostic ZIP: version, Windows/GPU, runtime detection,
candidate/game hashes, components/dependencies/verification, allowlisted VR
settings and redacted Framework log tails. Absent headset/controllers are
unknown. Exclude saves, dumps, credentials, unrelated files/logs and raw
user-path identifiers.

### Release and acceptance

Use one checked-in identity for build/PE metadata, installer, logs, diagnostics
and artifacts; schema versions stay separate. Build documented inputs from
the checkout with an explicit pinned CRT source; deliver notices, corresponding
source and SHA-256. Fixed-input ZIP determinism and full binary reproducibility
are separate checks.

Expand fixtures across A–F, dependency failures, unknown/damaged files,
add/remove layers, repeat operations and interrupted/locked-write rollback.
Run the finite clean-machine/manual-headset matrix in `CLOSURE_STATUS.md`.
Rewrite the README to actual download/hardware/settings/known-issue evidence.
Fixture success alone cannot justify a final release claim.
