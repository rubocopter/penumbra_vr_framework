# Penumbra VR Framework — audit reconciliation

This document reconciles the Astra High audit performed on `main` commit `73c70f0aad6fd2f33dc27983a44971ec44215f4a` with the repository state after the subsequent implementation work.

The original audit is preserved under [`docs/audits/ASTRA_HIGH_AUDIT.md`](audits/ASTRA_HIGH_AUDIT.md). It remains the historical diagnosis and design rationale. This file is the current-state overlay: it says which findings are closed, which were only implemented/host-tested, and which still require live or headset evidence.

## Evidence vocabulary

Use these states literally:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A later state is never implied by an earlier one. In particular, a passing build, CTest suite or synthetic exact-image harness does not prove in-game behavior or headset comfort.

## Baselines

- Astra audit baseline: `73c70f0aad6fd2f33dc27983a44971ec44215f4a`.
- Behavioral Overture/Rework reference used by the audit: `23c890f7dbd06b939be9951d282e6e948d9a6623`.
- First post-audit engineering checkpoint: `3ffa634b8a9efb56beff2c448c4da7fee1cd3a53` (`feat: complete offline VR contract intervention`).
- The repository then added the Black Plague no-write palm-query gate, constrained locomotion mapping, presentation ownership fixes and collision-comfort handoff before this documentation reconciliation.
- Always inspect the actual current HEAD before implementing. Do not use any SHA in this file as permission to ignore newer code.

## Executive reconciliation

Astra's central diagnosis remains valid: there was no evidence for a single global world-scale error. The real defects were contract/ownership problems across body tracking, presentation, posture, interaction and lifecycle. The repository subsequently implemented most of Astra's offline phases instead of retuning world scale.

The current engineering boundary is therefore **not** “implement the Astra plan from scratch”. The current boundary is:

1. preserve the post-audit contracts already implemented and host-tested;
2. finish the live/headset gates that those contracts still require;
3. complete Black Plague palm collision only after the already-pinned no-write native query passes live;
4. keep mechanisms, final tool geometry, mirror/focus, yaw/bob and Requiem as independent evidence tracks.

## Finding-by-finding status

| Astra finding / phase | Current repository state | Evidence level | Remaining work |
| --- | --- | --- | --- |
| **R1 / Phase 3 — body planning used the wrong epoch** | Reworked into one same-tick transaction around the existing `D460A -> D6E00` owner. Planning occurs from current B0/latest tracking before the native update; `0xD7281` consumes once; B1 is matched by tick/body/generation and reconciled once. The old post-tick plan-for-next-tick route was removed. | **host-tested** for the new transaction; the underlying body/collision boundary remains live-tested from earlier runs | Fresh headset short-motion/wall/slide comfort gate. Do not reopen a second native update or a next-tick planner. |
| **R2 / Phase 4 — culling/render/controllers lacked a common pose/yaw epoch** | Tracking samples, pose identity and yaw epochs were added. Presentation ownership was then corrected through two headset regressions: nested visibility reacquisition and stale compositor-sequence reuse. The current single-consumption presentation contract was sustained successfully in PID 22096 with no stereo/compositor failures. | Presentation sequence: **headset evidence**. Broader pose/yaw ownership: mixed host/live status | Keep tracking-world-yaw turn ownership, recenter/tracking-loss correlation and final yaw/bob as explicit gates. Do not infer complete Phase 4 closure from presentation success. |
| **R3 — locomotion/posture policy incomplete** | Direct locomotion now publishes logical intent and is integrated with the physics tick. Shared normal/sprint policy remains `1.5/2.25 m/s`; exact Black Plague Push/Move action states `1/2` select shared constrained `0.5 m/s`. Shared play-mode policy exists. | Normal direct route has prior headset evidence (PID 8092); constrained mapping is **host-tested** | Headset-test constrained Push/Move, seated/standing, recenter and posture transitions. Keep jump/vertical native and separate. |
| **R4 / Phase 1 — crouch query ownership suppressed native behavior outside VR** | Native crouch queries are preserved outside the VR ownership window. VR stance is bound to session/player generation and applies native `ChangeMoveState(4/0)` on the game thread. Desired/effective stance and blocked-stand are separated; a real NativeInputBridge contract harness exists. | Core correction **host-tested**; PID 22096 gives ordinary physical/button/Hybrid headset evidence | Explicit low-ceiling blocked-stand/retry and tracked-Y/collider correlation remain headset gates. |
| **R5 / Phase 5A — selection/grip/hold lifecycle inconsistent** | Selection/acquisition and hold lifetime gained generation-aware revalidation; stale holds are dropped rather than carried across player/world identity. Shared hand-contact mathematics was extracted from Rework. | **host-tested** | Headset/live validation for long bodies, loss of tracking, map/player replacement while holding, tool geometry and final contact visual alignment. |
| **R5 / Phase 5B — Black Plague palm ABI unknown** | The supported-image `CheckShapeWorldCollision` boundary is now pinned, including callback/contact layout, body shape/matrix accessors, `CreateBoxShape`, user count and destruction route. A default-off no-write `--validate-palm-query` diagnostic and synthetic mutation guard are host-tested. | **host-tested only** | Run the no-write query in a real BP map without VR. Only after that gate: create owned palm shapes, connect Rework sweep/refinement/recovery and validate in gameplay/headset. |
| **R6 / Phase 2 — startup/shutdown not transactional** | Probe lifecycle now has an explicit component ledger, partial state, retryable shutdown/rollback and indeterminate remote-timeout reporting. rel32 setup prepares resources before suspending peer threads and avoids diagnostic allocation in the suspended region. Fault harnesses were added. | **host-tested** | Repeated live initialize/stop/reinitialize/exit and failure-recovery evidence. Do not call partial state clean/ready. |
| **R7 — historical SDL crash** | No causal attribution was established. Later presentation regressions were diagnosed separately as compositor-frame ownership defects and corrected. | Historical cause **open** | If the original-style APPCRASH reappears, capture dump, stack/registers and module list. Do not attribute it to the shadow or activation mutex by timing alone. |
| **Mirror/focus** | Still experimental and deliberately separate from body reconciliation. | partial/live evidence only | Fresh mirror-on/off, Alt+Tab and focus recovery gate. |
| **Jump / body radius / global world scale** | Intentionally not retuned as part of the audit intervention. | preserved | Change only with new discriminating evidence. |
| **Requiem** | Catalog/deployment preparation exists; no functional backend is demonstrated. | planned | Repeat exact-build research; do not transfer Black Plague RVAs/contracts speculatively. |

## Post-audit evidence that changes the original plan

The important deltas after `73c70f0` are:

- `3ffa634b` implemented the offline contract intervention: same-tick body transaction, crouch ownership, play-mode/sample/yaw identity work, interaction generation checks and transactional lifecycle hardening, plus new harnesses.
- `1db673bb` pinned the native Black Plague palm-query ABI/lifetime boundary and added a default-off no-write diagnostic. The audit's “ABI still unknown” statement is therefore historical; the **live query and gameplay palm integration** are what remain open.
- `3333be19` added the exact constrained Push/Move locomotion mapping and prepared a headset candidate.
- `ca099ca6` and `7f842350` fixed two successive presentation ownership defects exposed by real headset candidates.
- PID 22096 subsequently validated the single-consumption presentation path through sustained gameplay and provided ordinary crouch/direct-locomotion evidence.
- `eaab1424` added a host-tested rejected-direction prediction filter after PID 22096 narrowed the remaining short-X/Z pullback to render continuation into a direction rejected by the last body solve. The PID 20520 comfort report is **not closed** until a fresh headset obstacle run passes.
- `cff61d01` added the shared `0.85 m` VR step-distance policy constant. Treat it as policy data, not as evidence that native step/collider behavior has been validated across games.

## What is actually blocking promotion

The next meaningful promotion gates are evidence gates, not broad rewrites:

1. **Black Plague no-write palm query live, without VR.** This proves the pinned native query/lifetime boundary in the actual process before gameplay palm shapes are introduced.
2. **Focused headset comfort/posture pass.** Validate the current rejected-direction filter, low-ceiling stand blocking/retry, continuous tracked-Y correlation, constrained `0.5 m/s` Push/Move, seated/recenter/tracking loss and regression of the already-good normal locomotion route.
3. **Interaction lifecycle/headset pass.** Long body contact, map/player change while holding, tracking loss, flashlight/glowstick geometry and visual/contact consistency.
4. **Separate presentation extras.** Mirror/focus/Alt+Tab and tracking-world-yaw/final body-footstep bob remain separate gates.
5. **Palm gameplay implementation.** Only after gate 1 passes: owned palm shapes plus the Rework sweep/refinement/recovery resolver, followed by live/headset validation.

## Frozen constraints from the audit

- Do not use world scale or the Rework vertical calibration formula as a compensating knob for timing/contact defects.
- Keep exactly one native `D6E00` body update owner and the existing `0xD7281` injection boundary.
- Do not stack independent hooks over an owned exact-build boundary; use fan-out/status from the owner.
- Keep jump/gravity/Y-native ownership separate unless evidence explicitly requires a change.
- Do not invent Black Plague or Requiem RVAs from Overture source symbols.
- Do not force doors/levers/joints through free-body grab and call that mechanism support.
- Keep Black Plague's richer finger articulation; do not regress it merely to match Overture implementation details.
- Preserve measured tool sockets as profile data; do not hide contact defects with global offsets.
- Preserve validation labels exactly and update them only when the matching evidence exists.

## Source of truth order

When documents disagree, resolve in this order:

1. current source and exact-build verifier/tests;
2. fresh live/headset evidence tied to a known build/PID;
3. this reconciliation and [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md);
4. `ROADMAP.md`, `ARCHITECTURE.md`, backend research notes and checklists;
5. the preserved Astra audit as historical diagnosis;
6. older changelog/research notes.

Do not silently “fix” a contradiction by choosing whichever document is newest. Verify the code/evidence and update the contradictory documents together.