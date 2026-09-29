# Release preparation audit

This records the inspected baseline and gaps for release engineering. It is
not a completed v1.0 audit. Dependencies are owned by
[RUNTIME_DEPENDENCIES.md](RUNTIME_DEPENDENCIES.md); remaining release gates by
[CLOSURE_STATUS.md](CLOSURE_STATUS.md). Temporary investigation belongs in `work/`.

## Starting state

Baseline `e73bf4d403985f0579f90a94a7cd2e36a2fcec25`, `main`, clean before
this task, 14 commits ahead and zero behind configured upstream. The table
records the starting gaps; current implementation and remaining acceptance
are owned by INSTALLER_DESIGN and CLOSURE_STATUS. Release work uses an isolated
branch and never deploys to or launches the actual game installations.

| Area | Existing implementation | Confirmed release gap |
| --- | --- | --- |
| Package | `Package-FrameworkCandidate.ps1` nests built O/B/R products, selector/GUI/discovery and SHA-256 manifest | Candidate naming, no coordinated v1 identity or release/source publication workflow |
| Overture | Source EXE replaces Steam launcher with original backup; HPL/OALWrapper static libraries | Product version `VR`, root CMake version 0.0.1; no coordinated PE version resources |
| B/R | One `alut.dll` proxy dispatches by exact game hash to two probes; shared redist journal/state | R auto-enabled by EXE presence/hash; no expansion completeness or independent removal UI |
| Discovery | Hashes, modern Steam library paths, manual folder/EXE, managed O recognition | Default Steam path/fixed folder names; no app-manifest install directory; accepts EXE-only fixtures |
| GUI | Spanish Windows Forms, hidden VBS launcher, root selection/install/update/repair/uninstall/manual browse | Hides unsupported entries; lacks dependency/config/verification/diagnostic/recovery UX |
| Transactions | Verified originals, snapshots, durable recovery, modified-file guards, stale O retirement | Selected roots commit independently; settings/component ownership extensions absent |
| Repair | O same recorded file set; B/R probe/actions/LAA recovery | Explicit repair overwrites damaged/modified owned payloads; needs clear preview; O new-version file-set repair limited |
| Uninstall | Independent O versus shared B/R exact restore | No R-only removal retaining B; B removal must include dependent R |
| Configuration | Generated B/R `alsoft.ini`; runtime settings; versioned recommendations | No installer game XML patch; recommendations include personal/language values inappropriate for blanket application |
| Controllers | Eight profiles, six sets, 42 actions, O and shared manifest variants | Graph/distribution evidence is not non-Sense hardware validation |
| Dependencies | O app-local OpenVR/OpenAL/CRT; B/R OpenVR; game legacy DLLs | B/R system OpenAL reliance; no preflight/dynamic-codec closure check |
| Optional mods | Shared transaction installs both Spanish translations; O defaults to 231 reviewed textures | Core requires translations; unified selector does not expose `SkipTexturePack` |
| Logging | Top-level JSONL operation/hash/result; product console messages | No release version or structured file/config/backup/rollback/verification report |
| Diagnostics | Developer build/discovery/validation scripts | No bounded, privacy-filtered user ZIP |
| CI | Root and autonomous O builds/tests | No combined release-package or clean-user acceptance job |

## Payload and generated-state ownership

O package sources: autonomous executable, pinned OpenVR, OpenAL Soft and two
CRT DLLs; `products/overture/data`; 16 VR Cg shaders; flashlight light resource;
actions/bindings; scripts/docs/notices. Purchased game content/config and legacy
DLLs remain required. `Penumbra.exe` is replaced for normal Steam Play; no
`dinput8.dll` proxy participates in this production path.

B/R payload IDs/destinations come from `assets/deployment/manifest.json`:
ALUT proxy, two probes, OpenVR, `vr`, hand texture, Spanish B/R files and
generated `alsoft.ini`. The original ALUT becomes
`PenumbraVR_alut_original.dll`. R has no separate root or duplicated shared
actions/loader; its executable and `expansion01` content remain game-owned.
B LAA is opt-in/verified; this installer does not transform R's executable.

O `.penumbravr/deploy-state.json`/backup tree and B/R
`PenumbraVR.BlackPlague.install.json`/original backups, plus recovery journals,
are generated ownership state. Build outputs, stamps, checksums, package staging,
runtime logs and settings are generated. Existing packaging explicitly selects
build outputs; mock SDL/OpenGL test DLLs, PDBs, dumps and developer injection
tools must remain excluded. The build requires a documented pinned external
Visual Studio CRT input, not an arbitrary globally installed runtime.

Both runtime consumers set the adjacent action manifest path. No persistent
SteamVR application manifest is provided; this alone does not justify adding
global registration or OpenXR switching.

## Configuration and compatibility

Preserve B/R `PostEffects=true` and `Refractions=true`, the corrected effect
path; O has its own recommendations. Recommended desktop VSync/FPS cap/FSAA,
motion blur/DOF/noise changes should be targeted and optional. Keep physics at
60 Hz. Shadows, render scale, textures and anisotropy are recommendations or
performance choices, not new universal launch requirements. Headset FOV comes
from VR projection rather than a desktop preset.

Exclude language, resolution, saves/key bindings, height/handedness/turn choices,
personal calibration and experimental mirror choices from automatic presets.
`recommended.json` requires an installer allowlist rather than whole-object
application. No new clean-baseline community-mod compatibility claim was proved.
Unknown modifications need visible backups/preservation or preflight rejection;
never adopt anonymous experiment files as owned payloads.

## Verification record

Fresh Release Win32 root build succeeded with VS Build Tools 17.14.37614.0,
MSBuild 17.14.51 and MSVC directory 14.44.35207, using pinned OpenVR.
`ctest --preset release --output-on-failure`: **46 passed, zero failed**, including
the real-driver eye-target test. This is host evidence, not headset acceptance.

Discovery fixture, metadata validation and controller-profile regression script
passed. The latter rejected four intentional mutations (missing family, path
drift, lost turn and wrong haptic type), not four new installer scenarios.

`tests/deployment/Test-FrameworkCandidate.ps1` passed: both products rebuilt,
two fixed-input archive hashes matched, GUI creation/discovery passed and
temporary installs/repair/exact restore completed for O and shared B/R. It
also checked package tampering, missing/corrupt controller repair, BP LAA
repair, shared-root deduplication and multi-root selection/logging. Its
Overture build ran **289 checks, zero failures**. This is one combined fixture
script, not proof that every A–F clean-machine scenario was executed.

The two fixture archives were removed by the test's temporary-directory cleanup;
no v1 release artifact was retained or published. Clean-machine/manual launch/
VR/save-load acceptance and final v1 artifact size/hash/tag remain pending.
No game was automatically launched. Subsequent release implementation is
recorded in the branch commits and the versioned artifact build report.
