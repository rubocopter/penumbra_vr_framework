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

The shared transaction's rollback, install, repeated repair and exact restore
pass a fixture with both exact executables, preserving pristine localization
backups and deployment ownership. The combined package is deterministic and
its selector deduplicates the shared root. The VBS launcher starts the Windows
Forms GUI with a hidden PowerShell console; packaged control creation and
read-only discovery of pristine Overture plus BP/Requiem pass host checks.
The GUI blocks repeated operations while a transaction runs. Runtime/headset
support and final UI acceptance remain separate gates.

## Remaining production work

- exercise the graphical install/repair/uninstall flow on a clean user setup;
- finish production OpenVR registration/versioning/rollback;
- apply localization and optional recommended settings transactionally;
- complete the dependency/redistribution license audit; packaged notices already include both translations;
- validate clean install, upgrade, repair, interruption recovery, external
  modification handling and exact restore for every supported product; and
- expose the final product selection/UI only after those transactions are
  complete.
