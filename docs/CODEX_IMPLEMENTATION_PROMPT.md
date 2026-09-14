# Codex / Sol implementation prompt — Penumbra VR Framework

Use this as the implementation objective for the next Codex/Sol pass.

---

Continue development of `rubocopter/penumbra_vr_framework` from the **actual current HEAD**.

Before modifying anything, inspect the repository state and read, in this order:

1. `AGENTS.md`
2. `docs/AUDIT_STATUS.md`
3. `docs/IMPLEMENTATION_PLAN.md`
4. `docs/DESIGN_DECISIONS.md`
5. `ROADMAP.md`
6. `ARCHITECTURE.md`
7. the relevant Black Plague/Rework research notes and headset checklist
8. the complete preserved Astra High audit through `docs/audits/ASTRA_HIGH_AUDIT.md`

The Astra audit was performed on `main` commit `73c70f0aad6fd2f33dc27983a44971ec44215f4a`. It is a historical diagnosis and implementation rationale, **not a list of tasks that are automatically still pending**. The repository advanced substantially after that commit. Inspect current source and recent diffs before changing code, then reconcile any contradiction against the current implementation and evidence.

## Behavioral reference

The demonstrated Overture/Rework behavioral reference used by the audit is revision:

`23c890f7dbd06b939be9951d282e6e948d9a6623`

Use it where it already proves VR behavior. Do not replace known-good algorithms with new approximations merely for architectural cleanliness.

## Current reconciled state

Treat these as the starting point unless current HEAD proves they changed:

- The audit found no evidence for a single global world-scale error. Do not retune world scale or Rework's vertical calibration formula to compensate for timing/contact defects.
- Overture is Framework-hosted and remains the behavioral reference/source integration; do not rewrite its proven loop or interaction paths without a demonstrated need.
- Black Plague keeps one native character-body update owner at `D460A -> D6E00` and the existing bounded horizontal injection boundary at `0xD7281`. Do not add a second native update or stacked hook owner.
- The post-audit body/reconciliation work replaced the old post-tick plan-for-next-tick flow with a same-tick transaction: resolve B0/body/generation and identified tracking before the native update; integrate logical locomotion using the physics tick; consume the `0xD7281` request once; observe B1; match tick/body/generation/epoch; reconcile once; carry accepted locomotion once. Preserve this contract.
- Crouch ownership is generation/session-bound, preserves native crouch outside VR ownership and applies the persistent desired stance through native `ChangeMoveState(4/0)` on the existing game-thread owner. Do not return to compensating press/release toggles.
- Tracking/presentation now carries explicit sample/yaw identity. Two real headset candidates exposed compositor ownership regressions; the current single-consumption presentation sequence was subsequently sustained successfully in PID 22096. Do not reintroduce nested pose acquisition or reuse of an already-submitted compositor sequence.
- Normal direct locomotion remains the shared Rework policy `1.5 m/s`, sprint `2.25 m/s`; exact Black Plague Push/Move action states `1/2` map to shared constrained `0.5 m/s`. The normal route has older headset evidence; the constrained mapping is host-tested and still needs headset evidence.
- The lifecycle intervention has an explicit component ledger, partial state, retryable teardown/rollback, indeterminate timeout semantics and safer suspended-thread setup. Preserve partial-state honesty.
- Interaction acquisition/hold state is generation-aware and the shared Rework hand-contact mathematics has been extracted.
- The Black Plague `CheckShapeWorldCollision` boundary is no longer an unknown ABI: the supported-image query/callback/contact layout, body shape/matrix accessors, `CreateBoxShape`, user count and destruction route are pinned. A default-off `--validate-palm-query` diagnostic is host-tested. It has **not** yet been run as a successful real-process live gate, creates no owned palm shape and is not connected to gameplay palm resolution.
- The current rejected-direction render prediction filter was added after PID 22096 narrowed the remaining short-X/Z pullback to render continuation into the direction rejected by the previous physical solve. That filter is host-tested only; the PID 20520 comfort report remains open until a fresh headset wall/slide/short-motion pass.
- PID 22096 also gives ordinary headset evidence for physical/button/Hybrid crouch and all-direction direct locomotion, but blocked stand under a low ceiling and explicit tracked-Y/collider correlation remain separate gates.
- Mirror/focus, tracking-world-yaw ownership and final body/footstep bob are separate validation tracks.
- Jump/gravity/native vertical ownership and Black Plague body dimensions are not to be retuned without new discriminating evidence.
- Jointed mechanisms are not supported by free-body grab; keep them on native paths until their actual states/constraints are adapted.
- Requiem is not a copy of Black Plague. Repeat exact-build research wherever evidence cannot safely transfer.

## Immediate priority order

### 1. Reconcile current HEAD

Inspect the exact current source and recent commits first. For every item below, explicitly state whether it is already implemented, partially implemented or still absent. Do not implement a duplicate path if HEAD already contains it.

### 2. Preserve and extend automatic regression coverage

Run the relevant Debug/Release/SDK-less root tests, metadata verifier, exact-image Black Plague verifier and Overture `-Full` regression when the environment permits. The last documented engineering checkpoint had 34/34 root CTest in all three configurations plus the Overture product gates.

Never promote live/headset status from host tests.

### 3. Close the Black Plague no-write palm-query live gate if the environment permits

Use the existing default-off diagnostic against a real loaded Black Plague map **without VR**:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-palm-query <pid>
```

The gate must prove that the pinned native shape query is serviced on the existing game-thread/body-update owner without mutating guarded native world/body/shape state or destabilizing teardown.

If a live game process is not available, do not fabricate success and do not bypass this gate by inventing a palm RVA/ABI. Leave the evidence requirement explicit.

### 4. Do not rewrite host-complete Astra phases while their remaining work is validation

The following are primarily validation/evidence tasks now, not broad redesign tasks:

- same-tick body transaction;
- crouch ownership/session-player generation;
- partial/retryable probe lifecycle;
- presentation sample identity/single-consumption;
- logical locomotion integration with the body tick;
- shared play-mode/hand-contact extraction;
- hold generation/lifecycle checks.

Change them only if a current test, code inspection or fresh live/headset result demonstrates a remaining defect.

### 5. Prepare/maintain the focused headset gates

Keep the authoritative headset checklist aligned with the code. The next focused pass must distinguish these independent contracts:

- short 5–10 cm physical X/Z movement, stop, wall block, slide and retreat with the current rejected-direction prediction filter;
- button, physical and Hybrid crouch with two full `4/0` cycles, `0.95/1.65 m` shape correlation, blocked stand under low ceiling and retry after clearance;
- continuous tracked-Y correlation from the reconciled feet anchor;
- constrained Push/Move `0.5 m/s` behavior, without sprint increasing constrained speed;
- seated/standing, recenter, tracking loss/recovery and yaw-epoch transitions;
- long-body grab/contact, tracking loss and player/map change while holding;
- flashlight/glowstick geometry;
- mirror/focus/Alt+Tab separately;
- tracking-world-yaw and final camera/body/footstep bob separately.

Do not combine all of those into one global pass/fail state.

### 6. Implement gameplay palm collision only after the live no-write gate passes

When the real-process no-write query is proven:

- create/destroy owned Black Plague palm shapes through the already-pinned lifetime boundary;
- adapt the shared Rework-derived sweep/refinement/recovery resolver;
- keep native query/shape/body access in the Black Plague adapter/backend;
- keep neutral contact/recovery mathematics in the shared runtime;
- exclude the currently held body as required by the Rework contract;
- make selection, visual palm and grab acquisition consume the same resolved contact/palm result;
- add host/exact-image tests first, then live/headset gates;
- do not hide geometry/contact errors with global offsets, longer arbitrary rays or spring-based grab behavior.

### 7. Leave mechanism support separate

Doors, levers, sliders and other joints remain native until their exact states/constraints are mapped. Do not route them through generic free-body grab and call the work complete.

## Non-negotiable constraints

- No speculative RVAs, offsets or calling conventions.
- No Black Plague/Requiem ABI inferred solely from Overture source declarations.
- No second `D6E00` body update.
- No overlapping independent hook owner on an already-owned callsite.
- No world-scale/calibration retune as a generic comfort fix.
- No jump/gravity tuning as a side effect of horizontal reconciliation.
- No degradation of Black Plague's richer finger articulation merely to match Overture.
- No global tool/hand offsets replacing measured per-model profile data.
- No promotion from `implemented`/`host-tested` to `live-tested`/`headset-validated` without matching evidence.
- No claim that the historical SDL crash has a known cause unless a new dump/stack proves it.

## Required workflow for every change

1. Inspect current implementation and exact evidence.
2. State the smallest missing contract.
3. Identify the owner and validation level before editing.
4. Implement the narrowest change that preserves demonstrated behavior.
5. Add or update a contract/regression test.
6. Run all relevant automatic gates available in the environment.
7. Update documentation together with code so `README.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `AUDIT_STATUS.md`, implementation plan and relevant backend notes do not contradict each other.
8. Record exactly what remains unvalidated.

## Definition of success for this pass

Success is not “finish Penumbra VR”. Success is:

- current source and documentation agree;
- no Astra task already implemented is redundantly rebuilt;
- remaining evidence gates are explicit;
- any implementable gap is closed without violating ownership boundaries;
- automatic regressions remain green;
- live/headset claims are made only from real matching runs;
- the next developer/Codex pass can determine the real next task from the repository alone.

When a requested gate cannot be executed because the required Black Plague process or headset is unavailable, stop at the correct evidence boundary, document it and continue only with work that does not require inventing that evidence.
