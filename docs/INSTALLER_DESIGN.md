# Unified installer design

Penumbra VR should install as one product while preserving the different
integration model required by each game. The installer is not implemented yet;
this document defines the durable contract it must consume.

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

The filesystem transaction that performs this safely is still open work.

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

- Steam discovery plus manual folder selection;
- transactional filesystem/install-state implementation;
- known-build LAA apply/repair/uninstall path;
- Overture/Requiem payload entries as their unified installer paths become
  active;
- production OpenVR action registration/versioning/rollback;
- transactional localization and recommended-settings application;
- license/attribution packaging;
- clean install, upgrade, repair, external-modification and rollback tests.
