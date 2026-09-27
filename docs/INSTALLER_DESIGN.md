# Unified installer design

Penumbra VR should install as one product while preserving the integration model
required by each game. A host-tested two-game candidate exists for Overture and
Black Plague; the production three-game installer is not complete. This file
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

Current Black Plague payload ownership includes the ALUT bootstrap proxy,
Framework probe DLL, `openvr_api.dll`, shared OpenVR actions/bindings, imported
hand texture, Spanish localization and managed HRTF configuration boundary.

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
- a combined candidate that can select/install/repair/recover/restore one or
  both implemented games and logs operations as JSONL;
- complete package verification before discovery or writes.

The combined candidate deliberately rejects Requiem because no production
Requiem deployment transaction exists yet. This two-game prototype is not a
three-game release and does not promote runtime/headset support.

## Remaining production work

- integrate Framework-owned Overture upgrade/ownership and a verified Requiem
  deployment transaction into one multi-product flow;
- finish production OpenVR registration/versioning/rollback;
- apply localization and optional recommended settings transactionally;
- package licenses/attribution;
- validate clean install, upgrade, repair, interruption recovery, external
  modification handling and exact restore for every supported product; and
- expose the final product selection/UI only after those transactions are
  complete.