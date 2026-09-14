# Penumbra VR Framework — current implementation plan

This plan starts from the repository **after** the Astra High audit and the subsequent offline intervention. It must be read together with [`AUDIT_STATUS.md`](AUDIT_STATUS.md) and the preserved audit index at [`audits/ASTRA_HIGH_AUDIT.md`](audits/ASTRA_HIGH_AUDIT.md).

The objective is not to replay the audit phase by phase. The objective is to close the remaining evidence and implementation gaps without regressing already-proven owners, Overture behavior or Black Plague headset paths.

## Priority 0 — protect the current baseline

Before any code change:

1. inspect current HEAD and recent diffs;
2. run or review the current automatic gates;
3. identify whether the target item is already implemented;
4. preserve exact-build ownership boundaries;
5. state the current evidence level before changing it.

Do not reopen solved discovery tasks merely because they appear as open in the historical audit.

### Baseline that must remain intact

- Overture remains a Framework-owned source product and the Rework behavioral reference.
- Black Plague keeps one native character-body update owner (`D460A -> D6E00`).
- Black Plague keeps the proven `0xD7281` bounded horizontal injection boundary.
- Normal direct locomotion policy remains `1.5 m/s`; sprint remains `2.25 m/s`; demonstrated constrained movement remains `0.5 m/s`.
- Crouch remains a persistent desired-state policy applied through native `ChangeMoveState(4/0)` on the existing game-thread owner.
- Presentation samples are single-consumption compositor frames; never reintroduce duplicate `WaitGetPoses`/Submit ownership.
- Jump/vertical native ownership is not part of the horizontal reconciliation rewrite.

## Priority 1 — close the no-write Black Plague palm-query live gate

The exact supported-image ABI is now pinned and the diagnostic is host-tested. The next step is **not** to invent a gameplay palm implementation immediately.

Run the existing default-off query against a real loaded Black Plague map without starting VR:

```powershell
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-palm-query <pid>
```

Required evidence:

- query serviced on the existing game-thread/body-update owner;
- valid callback/contact output or a clean no-contact result;
- no mutation of the guarded native world/body/shape state;
- no crash, stale pointer or teardown failure;
- executable/build identity recorded.

### Acceptance

Only after the real-process no-write query passes may the project promote the pinned native contact boundary from host-tested to live-tested and proceed with owned palm shapes.

### If it fails

Investigate the exact mismatch. Do not fall back to guessed RVAs, guessed callback layouts or HPL-source assumptions.

## Priority 2 — focused Black Plague headset validation

Use the current room-scale helper and the authoritative headset checklist. This phase validates post-audit changes that are already implemented but not fully promoted.

### 2A. Short physical X/Z comfort

Validate:

- 5–10 cm physical translations and stop;
- wall block;
- oblique wall slide;
- movement away from the wall after rejection;
- stationary head tilt without locomotion;
- no pullback/snap caused by prediction into the previously rejected direction.

The current renderer filters only the rejected prediction component. Tangential slide and retreat must remain intact.

### 2B. Crouch and tracked Y

Validate at least:

- two full physical crouch/stand cycles;
- button-only crouch;
- Hybrid behavior;
- native move state `4/0` correlated with `0.95/1.65 m` body shapes;
- blocked stand under a low ceiling;
- retry and final stand after clearance returns;
- continuous tracked-Y placement tied to the reconciled feet anchor.

Do not accept aggregate counters as proof of final posture.

### 2C. Constrained locomotion

Validate exact-build Push/Move states `1/2` using the shared `0.5 m/s` policy. Confirm sprint does not incorrectly raise the constrained speed.

PID 8092 remains valid evidence for the earlier normal direct locomotion route; it is not evidence for this later constrained mapping.

### 2D. Seated/recenter/tracking loss

Validate:

- standing ↔ seated transitions;
- recenter without stale physical/stick carry;
- tracking loss and recovery;
- no duplicated pose/action consumption;
- no cross-epoch yaw or body request.

### Acceptance

Promote each sub-feature independently. A successful locomotion run does not automatically promote crouch, yaw or interaction.

## Priority 3 — interaction lifecycle and geometry validation

Before adding more interaction mechanisms, validate the current generation/lifecycle work:

- acquire/release a small prop;
- acquire a long body at different local contact points;
- rotate long bodies and verify the contact remains explainable;
- lose/recover controller tracking;
- open menu/change map while holding an object;
- replace player/body identity while interaction state exists;
- verify release/restoration occurs at most once and never writes to destroyed native memory;
- verify flashlight/glowstick socket composition with both hands and wrist rotations.

Keep the distinction between:

- free-body grab;
- collision-resolved palm contact;
- jointed mechanisms.

A working free-body path is not evidence that doors, levers or sliders have VR mechanism support.

## Priority 4 — implement Black Plague palm collision after Priority 1 passes

Once the native no-write query is live-tested:

1. add owned Black Plague palm-shape creation/destruction using the already-pinned native lifetime contract;
2. adapt the shared Rework-derived `vr_hand_contact` sweep/refinement/recovery policy;
3. keep shape/query/lifetime mechanics inside the Black Plague backend/adapter;
4. keep contact mathematics and recovery policy game-neutral;
5. exclude the currently held body as required by the Rework contract;
6. feed one resolved palm/contact result consistently into selection, visualization and grab acquisition;
7. add synthetic and exact-image tests before enabling the path by default;
8. validate against wall contact, occlusion, long bodies and tool geometry in the live game/headset.

Do not compensate incomplete palm geometry with global hand/tool offsets.

## Priority 5 — separate comfort/presentation gates

These remain independent and should not block unrelated backend work unless they cause regressions:

### Mirror/focus

- mirror on/off;
- Alt+Tab;
- menu ↔ gameplay presentation ownership;
- focus recovery;
- no duplicate compositor frame submission.

### Tracking-world yaw

- define and validate the owner of tracking-world yaw;
- handle native yaw changes/recenter through explicit epoch/rebase rules;
- test turn + stick in the same update;
- do not infer yaw correctness from rigid individual matrices alone.

### Camera/body/footstep bob

Map and validate only after body/tracking comfort is stable. Do not hide body reconciliation defects with camera offsets.

## Priority 6 — mechanisms, UI and release work

After the current Black Plague body/contact path is stable:

- map and adapt jointed mechanism states rather than routing them through free-body grab;
- validate definitive tool/light profiles;
- inventory, notes, HUD and subtitles;
- safe in-game VR settings insertion boundary;
- representative chapter-level validation;
- controller-profile live parity;
- frame-pacing work;
- production installer/bootstrap.

## Priority 7 — Requiem

Requiem reuses game-neutral policy but repeats exact-build research wherever evidence cannot safely transfer.

Never copy Black Plague RVAs, object layouts or native state indices into Requiem without exact-build proof.

## Validation gates

### Automatic baseline

At minimum, preserve the repository's current Debug/Release/SDK-less root tests, metadata checks, exact-image Black Plague verifier and autonomous Overture `-Full` regression. The last documented engineering checkpoint passed 34/34 CTest in all three root configurations plus the Overture product gates.

### Live/headset evidence

Record for each run:

- executable and DLL hash;
- commit/build identity;
- PID;
- active gates/settings;
- scenario/save used;
- result by feature, not a single global pass/fail;
- relevant telemetry window only.

## Completion rule

Do not declare the Astra intervention “done” as one binary state. It is complete only per contract:

- same-tick body transaction: implemented/host-tested, comfort still requires focused headset evidence;
- crouch ownership: implemented/host-tested with ordinary headset evidence, low-ceiling/Y edges still open;
- lifecycle: implemented/host-tested, live failure/restart evidence still open;
- presentation single-consumption: positive headset evidence exists;
- palm query ABI: host-tested, live query pending;
- gameplay palm collision: not yet connected;
- mechanisms/mirror/yaw/bob/Requiem: separate remaining tracks.
