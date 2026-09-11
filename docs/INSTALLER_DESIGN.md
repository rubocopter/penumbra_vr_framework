# Unified installer design

## User-facing model

Penumbra VR is installed as one product. The installer discovers any supported
Penumbra installations, identifies each executable by content rather than by
filename, and deploys the integration required by that exact game build.

```text
discover installations
        |
fingerprint each executable
        |
match an exact-build manifest
        |
plan backup + LAA + payload
        |
apply transaction and verify
```

Overture may receive a rebuilt source-based executable. Black Plague and
Requiem require a bootstrap and an exact-build binary backend. This difference
is internal; users still install, repair and remove one Penumbra VR product.

## Detection rules

- A filename is a hint, never proof of identity. Overture and Black Plague may
  both be named `Penumbra.exe`.
- SHA-256 selects an exact-build manifest.
- An unknown hash is reported as unsupported and is never patched or injected.
- The installer records the original and installed hashes so it can distinguish
  a clean game, a known Penumbra VR installation and an externally modified
  executable.
- Steam discovery is supplemented by explicit manual folder selection.

## Transaction model

Every installation change must be planned before the first write. A transaction
contains the detected game/build, original file hashes, intended payload,
backup locations and expected installed hashes.

The apply sequence is:

1. Revalidate every source hash immediately before modification.
2. Write backups and verify that their hashes match the originals.
3. Prepare changed files in the same filesystem without replacing live files.
4. Verify the prepared files, including their PE and LAA state where applicable.
5. Replace files atomically where Windows permits it.
6. Write the installation record only after every replacement succeeds.
7. On any failure, restore all files already changed and report the exact step.

Repair repeats the same validation against the recorded state. Uninstall uses
the verified backups and refuses to overwrite unexpected user or third-party
changes without an explicit recovery choice.

## Large Address Aware policy

The observed Overture, Black Plague and Requiem retail executables are 32-bit.
The observed Black Plague and Requiem Steam binaries are PE32 images without
`IMAGE_FILE_LARGE_ADDRESS_AWARE`; Overture VR Rework enables the equivalent
linker option for its rebuilt executable.

The unified installer should enable LAA for every allowlisted 32-bit game build
used by Penumbra VR. High-resolution stereo, depth buffers, MSAA and Enhanced
visuals consume substantially more address space than the original renderer.
LAA does not improve image quality by itself; it gives the process enough
virtual address space for those resources on 64-bit Windows.

The transformation must:

- accept only an allowlisted x86 PE32 executable;
- preserve every byte except the `IMAGE_FILE_LARGE_ADDRESS_AWARE` bit;
- be idempotent when the bit is already set;
- operate on a prepared copy, never directly on the only installed executable;
- record and verify both the original and transformed hashes;
- restore the exact original executable during rollback.

`src/deployment/pe_large_address.*` implements and unit-tests the pure in-memory
PE inspection and one-bit transformation. The build catalogue and canonical
Black Plague/Requiem manifests now also record the exact transformed LAA hashes,
and metadata validation checks each transformed variant against its canonical
build identity. Offline verification confirmed the transform changes only the
PE characteristics byte required for `0x010F -> 0x012F` and reproduces the
catalogued hashes.

Deployment remains deliberately incomplete: the filesystem transaction that
revalidates a known canonical hash, prepares the transformed copy, backs up the
original, atomically replaces it and rolls it back has not been implemented.

## Payload selection

Common payloads may include the OpenVR loader, action manifest, controller
bindings, configuration schema and shared runtime. Each exact-build manifest
selects one backend and its bootstrap method. A backend must never be chosen by
probing arbitrary addresses in an unknown process.

Spanish localization is also a Framework-owned deployment payload for the two
binary games. `assets/localization/manifest.json` records the exact repository
file, SHA-256 and install-relative destination for each game:

- Black Plague: `redist/config/Espanol.lang`;
- Requiem: `redist/expansion01/config/Espanol_exp.lang`.

The original attribution notice supplied with each translation is retained
under `assets/localization/<game>/leeme.txt`. The installer must treat an
existing destination language file like any other owned replacement: record and
verify it before modification, back it up, verify the copied translation, and
restore the original on uninstall/rollback. Overture's Spanish file remains in
its source-product overlay at `products/overture/data/config/Espanol.lang`.

The action files under `assets/openvr` are now consumed by the framework's
runtime input path: the shared action manifest/bindings are copied into the
build output, real OpenVR action polling is implemented, and Black Plague's
native intent bridge consumes the resulting logical actions. This does **not**
mean they are installer-managed yet. Production deployment, registration,
versioning and rollback of these files remain installer work.

## Work still required

- choose and validate the production bootstrap mechanism;
- implement installation discovery and manual selection;
- implement the transactional filesystem layer and installation record;
- wire the exact canonical/transformed catalogue identities into transactional apply/repair/uninstall;
- package per-build backends and shared runtime assets;
- define the exact ownership/versioning rules for deployed OpenVR action files;
- deploy the imported Black Plague/Requiem Spanish localization payloads through
  the transactional install/repair/uninstall path;
- test install, repair, upgrade and rollback on clean game copies;
- design a recovery flow for missing backups and externally modified files.
