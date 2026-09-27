# Unified installer design

Penumbra VR should install as one product while preserving the different
integration model required by each game. A two-game host-tested candidate now
combines Overture and Black Plague, but the production three-game installer is
not implemented yet. This document defines the durable contract it must consume.

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

`tools/Get-PenumbraInstallations.ps1` now performs read-only Steam library and
manual-folder discovery using the executable fingerprint tool. It lists Black
Plague and Requiem separately even though both reside under the same Steam
`redist`, and reports unknown hashes as non-installable. The known Framework
Overture checkpoint is recognized as Overture but remains outside the unified
installer until its ownership/upgrade transaction is implemented.
Malformed executables are listed with a probe error rather than stopping the
whole discovery pass. The Black Plague candidate installer uses this discovery
when no path is supplied; multiple compatible copies require an explicit
`-GamePath` so it cannot select one silently.

## Transaction rules

Every write is planned before modification. A transaction records the detected
game/build, original hashes, backups, intended payload and expected installed
hashes.

Apply must:

1. revalidate source/target identity immediately before writes;
2. create and hash-verify backups;
3. prepare transformed/replacement files without destroying the only original;
4. verify prepared PE/payload state;
5. replace files atomically where possible;
6. write the installation record only after every step succeeds;
7. roll back changed files on failure.

Repair repeats validation against the recorded state. Uninstall restores verified
originals and refuses to overwrite unexpected third-party/user modifications
silently.

## Executable policy

Overture uses a Framework-owned rebuilt source executable. Black Plague and
Requiem are exact-build binary integrations.

The observed Black Plague/Requiem executables are 32-bit PE32 without Large
Address Aware. `src/deployment/pe_large_address.*` implements the one-bit LAA
transform and the exact canonical/transformed hashes are recorded in the build
catalogue/manifests.

The production installer may apply that transform only when:

- the canonical x86 build is allowlisted;
- a verified backup exists;
- only the `IMAGE_FILE_LARGE_ADDRESS_AWARE` bit changes;
- the transformed hash matches the recorded variant;
- rollback restores the exact canonical executable.

The Black Plague candidate now offers `-LargeAddressAware` as an opt-in,
host-tested path. Its packaged tool calls the existing PE transformation,
accepts only the recorded canonical Black Plague/Requiem hashes and verifies
the exact transformed hash. The Black Plague installer owns a hash-verified
canonical backup, restores it on uninstall, preserves a pre-existing LAA
executable, and includes both executable paths in its in-process rollback.
Requiem gameplay/deployment and complete production repair remain open; the optional
transform alone is not evidence of runtime support.

## Payload contract

`assets/deployment/manifest.json` is the source of truth for installable payload
ownership. It records logical IDs, source classes, destinations and restore
policy so development deploys and the future installer do not maintain separate
hand-written file lists.

The current Black Plague development product definition includes:

- ALUT bootstrap proxy;
- Framework probe DLL;
- `openvr_api.dll`;
- shared OpenVR action/binding assets;
- Rework hand diffuse used by the imported hand renderer;
- Framework-owned Spanish localization;
- generated `alsoft.ini` HRTF configuration ownership.

The normal-Steam Black Plague development installer consumes this contract and
its bootstrap path has reached gameplay VR from Steam's ordinary **Play** button.
That live result validates the bootstrap concept; later runtime changes still
retain their own capability-specific evidence levels.

The development installer now validates recorded managed files, the OpenVR
action directory and the original ALUT/localization backups before its first
upgrade or restore write. It rejects an unowned OpenVR directory on a fresh
install. A fixture using the canonical retail executable exercises install,
upgrade, rejected external edits, clean restore and preservation of unrelated
files. It also snapshots each managed target before writes and restores that
snapshot when install, upgrade or restore raises an ordinary filesystem error.
A locked-file fixture exercises interrupted install, upgrade and restore. The
snapshot is removed on success and retained if rollback itself fails. The
Black Plague candidate now also publishes `.penumbravr-bp-journal` before
managed writes. `-Recover -GamePath <executable>` validates every saved file
and OpenVR directory before restoring them, including when the executable is
missing. A separate process is terminated during install, LAA upgrade and
restore in a fixture; recovery preserves the prior exact executable and
deployment record. A corrupted journal copy is rejected without writes.
Explicit `-Repair` now replaces damaged recorded mod payloads, including
missing OpenVR binding files, from the candidate package. It requires the
verified retail ALUT and any Spanish localization backup. Foreign files or
directories inside the managed OpenVR tree block repair. A damaged or missing
managed LAA executable is reconstructed only from its hash-verified canonical
backup with the exact-build transform, then checked against the known LAA hash.
The installer regenerates a missing mod-created OpenAL configuration only when
the recorded hash matches its known default. It rejects changed personal audio
settings and a missing user-owned configuration. A damaged canonical executable
without a verified LAA backup cannot be repaired from the mod package.
The final three-game transaction remains open.

`tools/Package-BlackPlagueCandidate.ps1` assembles a candidate ZIP from the
Release build and `assets/deployment/manifest.json`. It includes the installer,
DLLs, actions/bindings, localization, hand texture and licenses; its fixed ZIP
entry ordering and timestamps make identical inputs byte-identical. The package
test extracts it away from the source checkout and installs/restores a disposable
copy of the exact retail build. This is a Black Plague development candidate,
not the unified three-game release or evidence of headset validation.

The Framework-owned Overture source product also builds a standalone package.
`tools/Package-OvertureCandidate.ps1` checks every file against its
`SHA256SUMS.txt` and creates a deterministic ZIP. A disposable exact-executable
fixture extracts that ZIP and verifies install/restore without the source tree.
Its existing Overture installer still needs the unified transaction and
known-build selection policy before a final release.
Upgrade and restore validate every previously managed file and original backup
before writing. Overture deployment now also snapshots and verifies the managed
file set and deployment state before its first write, verifies copied backups,
payloads and restored originals, and rolls back ordinary install, upgrade and
restore failures. An isolated fixture injects a late copy failure in each phase
after earlier files changed and checks that the previous hashes/state return.
The Overture candidate also stages that verified snapshot inside the game root
and publishes `.penumbravr-journal` before changing managed files. An explicit
`-Recover -InstallRoot <game folder>` verifies every journal copy before writes
and restores the prior files/state, including when `Penumbra.exe` disappeared.
A fixture terminates a separate installer process during install, upgrade and
restore, then exercises recovery; it also confirms that a damaged journal copy
is rejected without modifying the game. A resolved journal is renamed before
cleanup so a crash during cleanup cannot undo a committed operation.
Explicit `-Repair` requires existing deployment state and the same managed
file set in the package. It verifies every original backup, then replaces
damaged or missing recorded paths, including `Penumbra.exe`; unrelated files
are untouched. Its ordinary failure and interruption handling use the same
verified snapshot and recovery journal as install and restore.

`tools/Package-FrameworkCandidate.ps1` combines the two verified ZIP inputs
into one deterministic candidate archive. Its selector lists detected paths,
allows an explicit game/executable or manual folder, installs or restores one
chosen game, and rejects Requiem before writes. A disposable fixture exercises
both games from the extracted archive. Its selector exposes recovery for both
games with an explicit game path even when the executable is absent. Both paths
expose repair with explicit game and path after verifying the complete package;
the two-game candidate does not satisfy the final three-game release gate.
For Overture, the selector passes its detected SHA-256 to the product installer,
which rechecks the executable before any deployment write.
The combined ZIP records every packaged file in `SHA256SUMS.txt`; its selector
verifies the complete extracted payload before discovery or writes. A fixture
alters a packaged file and checks that the selector rejects it.

Black Plague deploy preserves/restores a pre-existing Spanish language file and
creates the default HRTF config only when no prior `alsoft.ini` exists. Runtime
settings remain authoritative for later HRTF changes.

## Localization and OpenVR assets

`assets/localization/manifest.json` owns the binary-game Spanish payloads and
hashes:

- Black Plague: `redist/config/Espanol.lang`;
- Requiem: `redist/expansion01/config/Espanol_exp.lang`.

Original attribution notices remain beside each payload. Overture keeps its
Spanish file inside the source-product overlay.

`assets/openvr` owns the shared action manifest and controller bindings. They are
already runtime/build inputs; transactional production registration/versioning
and rollback remain installer work.

## Recommended settings

`assets/settings/recommended.json` contains maintainer-tested game/profile values
for an optional installer preset. It intentionally excludes save state, key
bindings and machine-specific display resolution.

Recommended settings must be applied reversibly and must not silently overwrite
personal calibration such as height, handedness, turn preference or UI scale in
an existing profile.

## Remaining installer work

- connect discovered paths to a multi-product selection UI;
- transactional filesystem/install-state implementation;
- known-build LAA apply/repair/uninstall path;
- Overture/Requiem payload entries as their unified installer paths become
  active;
- production OpenVR action registration/versioning/rollback;
- transactional localization and recommended-settings application;
- license/attribution packaging;
- clean install, upgrade, repair, external-modification and rollback tests.
