# Tracking/body reconciliation — host and live shadow validation

Updated: 2026-09-13.
Feature checkpoint: `18c63ef39adb9d3af6df0a6e9f97d0889df5194c`.
CI hardening checkpoint: `ea12955f494976ed7e3fb2913a3da6221d7dcff8`.

This file follows up `TRACKING_BODY_RECONCILIATION.md`. That report is an implementation-time snapshot from the Linux editing environment and intentionally records the Windows gates that could not be run there. The post-push GitHub state below supersedes only those pending-host statements; it does not rewrite the historical evidence or promote any live/headset claim.

Later checkpoints: PID 26144 subsequently live-tested the physical request at
`0xD7281`. PID 21548 then passed the full default-off active room-scale helper
and native crouch recovery after the combined-tick carry correction. The user
confirmed that in-place head tilt no longer moved the character, but continuous
world shake still failed comfort. Rework comparison identified the retained 60
Hz body offset over Black Plague's native camera as a presentation difference.
Horizontal render placement now uses the reconciled anchor plus the latest HMD
delta since that sample; this second correction compiles and awaits repeat
live/headset validation. Statements below
that positional translation is compile-time zero describe this earlier
shadow-only checkpoint.

## Host result

**Status: shared extraction host-tested; Black Plague shadow path live-tested.**

GitHub Actions run `34616035023` validated the feature commit on Windows Server 2022. The existing `Windows x86 (without OpenVR SDK)` job completed successfully through:

- metadata and OpenVR asset validation;
- Visual Studio 2022 Win32 CMake configuration;
- Debug build and CTest;
- Release build and CTest.

The root CMake configuration contains 27 tests after the new `body_reconciliation` registration. Hosted SDK-less CI intentionally excludes only `opengl_eye_targets`, whose real-driver WGL/FBO pixel path remains a separate local validation gate.

Commit `ea12955` then added a dedicated `Overture Release regression` job. GitHub Actions run `34616820448` completed both jobs successfully:

- the root Windows x86 metadata/Debug/Release gate passed again;
- `./tools/Build-OvertureProduct.ps1 -Configuration Release -Full` passed from a clean Windows 2022 checkout.

That Overture script is the existing Framework-owned product gate: it runs the retained project checks, shader/visual/texture checks, metadata validation, a forced Release rebuild, Large Address Aware verification and the historical `VRTrackingTest` executable. Therefore the shared reconciliation extraction no longer has an outstanding Overture host-regression gate.

## What this does and does not prove

This closes the host-validation gap for the shared reconciliation extraction and the Black Plague shadow wiring. It does **not** change these boundaries:

- Black Plague positional HMD translation remains compile-time zero;
- the shadow path does not inject its physical request into the game;
- native accepted displacement is still native locomotion/body motion, not acceptance of the uninjected physical plan;
- `MoveForward` / `MoveSideways` remain acceleration/native-movement boundaries, not a physical displacement contract;
- active room-scale still requires a separately demonstrated collision-aware physical X/Z request in metres, resolved by the existing single native tick;
- jump/vertical ownership, physical crouch, speed tuning and camera/bob remain separate milestones;
- no new live or headset validation is implied by CI.

The single-owner model remains mandatory: `NativeInputBridge` owns movement callsites, `BodyCollisionProbe` owns `D460A -> D6E00`, and `BlackPlagueBodyAdapter` receives fan-out without adding a second native update.

## Reproducible shadow request path

The first implementation sampled `PVR_BP_RECONCILIATION_SHADOW=1` from the **game process** during body-adapter installation. That remains supported for advanced/manual workflows where the game actually inherits the variable, but it is not a reliable one-click launch mechanism: `--launch-vr` starts the protected build through `steam://rungameid/22120`, so an already-running Steam process does not inherit variables set only in the launcher shell.

The repository therefore provides the internal validation helper `tools/Start-BlackPlagueShadowValidation.ps1`. It stays under `tools/` rather than adding another diagnostic launcher to the repository root.

The helper first runs `Test-BlackPlagueInputMap.ps1` against the initialized exact-build image. It then holds the named mutex `Local\PenumbraVR.BlackPlague.ReconciliationShadow` for the complete validation session. `BlackPlagueBodyAdapter` samples either the original environment variable or that mutex exactly once during installation. The helper now watches the fresh PID's probe log and fails closed unless installation explicitly reports `body_reconciliation_shadow enabled=1 source=mutex`; merely launching the game no longer counts as shadow validation. The mutex is disposed when the validation session exits, so no preference, registry value or persistent environment state remains for the next run.

By default the helper expects `artifacts\black-plague-22000-live.bin`. If that local research artifact is not present, pass `-ImagePath <initialized-capture>` explicitly. The helper fails closed rather than skipping the exact-build verification gate. The normal `Start-Black-Plague-VR.cmd` remains shadow-off.

## Live shadow result

PID 28172 completed the intended transient-mutex validation route. The fresh
process reported `body_reconciliation_shadow enabled=1 source=mutex`, kept
positional translation zero, exercised stationary/small horizontal HMD deltas,
native free/block/slide movement, recenter/body replacement and retained the
existing approximately 60 Hz native body tick without evidence of a second
`D6E00`. The earlier forwarded-`LoadLibraryW` startup race is therefore also
live-confirmed past its former failure point.

This proves the shadow lifecycle and observation wiring in the live process. It
does not demonstrate acceptance/rejection of physical VR displacement because
the shadow request is never injected.

## Next implementation decision

Identify and demonstrate the missing bounded collision-aware physical X/Z
displacement request mechanism for Black Plague, distinct from native analog
movement and consumed by the existing single native tick. Keep positional HMD
translation disabled until that mechanism has separate host/live evidence. Only
then should active tracking/body reconciliation and headset positional validation
begin. `Start-BlackPlagueShadowValidation.ps1` remains useful for regression
captures rather than as the next milestone.
