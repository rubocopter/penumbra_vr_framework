# Tracking/body reconciliation — post-push host validation

Date: 2026-09-11.
Feature checkpoint: `18c63ef39adb9d3af6df0a6e9f97d0889df5194c`.
CI hardening checkpoint: `ea12955f494976ed7e3fb2913a3da6221d7dcff8`.

This file follows up `TRACKING_BODY_RECONCILIATION.md`. That report is an implementation-time snapshot from the Linux editing environment and intentionally records the Windows gates that could not be run there. The post-push GitHub state below supersedes only those pending-host statements; it does not rewrite the historical evidence or promote any live/headset claim.

## Host result

**Status: host-tested; Black Plague live shadow validation still pending.**

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

## Remaining pre-live gate

Hosted CI cannot verify the protected exact-build initialized image used for Black Plague research. Before a new DLL is exercised live, rerun the local exact-build verifier against the supported initialized capture / local binary evidence and ensure the freshly built probe matches the allowlisted research build.

If that passes, the next evidence collection is deliberately shadow-only:

1. set `PVR_BP_RECONCILIATION_SHADOW=1` in the game process before adapter installation;
2. keep positional translation at zero and preserve existing VR settings;
3. capture a stationary baseline and small horizontal physical HMD movements;
4. exercise ordinary native locomotion in free space, into a wall and tangentially along it;
5. exercise recenter and an ordinary body-replacement/load transition where convenient;
6. confirm shadow history resets cleanly, requests stay bounded to `0.05 m`, native body updates remain approximately 60 Hz with one `D6E00` per native tick, and no shadow output changes camera/body position;
7. stop/deactivate through the normal flow and retain the periodic shadow summaries plus existing body telemetry.

Do not enable room-scale or combine this capture with speed, crouch, jump, camera/bob or interaction tuning.

## Next implementation decision after the shadow capture

A successful shadow run is evidence that the extracted policy and lifecycle wiring behave correctly in the live process. It is **not** authorization to enable positional tracking. The next implementation milestone must first identify and demonstrate the missing bounded collision-aware physical displacement request mechanism for Black Plague. Only after that mechanism has separate host/live evidence should active tracking/body reconciliation and headset positional validation begin.
