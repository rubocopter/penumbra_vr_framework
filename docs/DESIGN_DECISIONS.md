# Penumbra VR Framework — design decisions and invariants

This document records decisions that should not be casually reopened during iterative debugging. It derives from verified Rework comparison and subsequent implementation/validation evidence.

These are not immutable forever. They may change when new evidence disproves them, but not merely because another implementation is aesthetically cleaner.

## 1. One product, asymmetric integrations

Penumbra VR Framework is one user-facing project, but the three games do not need identical integration mechanics.

- Overture is source-integrated through the Framework-owned HPL1 product host.
- Black Plague uses an exact-build binary backend.
- Requiem will use exact-build research where closed-game evidence is required.

Do not force a universal DLL/backend flow where the games demonstrably differ.

## 2. Rework is a behavioral reference, not a permanent dependency

Overture Rework revision `23c890f7dbd06b939be9951d282e6e948d9a6623` is the primary behavioral reference for already-proven Overture VR behavior.

Port the demonstrated behavior and extract neutral policy where appropriate. Do not rebuild known-good systems from scratch solely to make them look more generic.

## 3. World scale is not the default explanation

Repository and Rework comparison found no evidence for one global world-scale error explaining the Black Plague symptoms.

The Rework vertical calibration factor is not a global world-scale knob. Do not retune world scale, `(HMD.y - 0.2) * 1.065`, hand distance or tool offsets to mask timing, collision, pose-epoch or contact defects.

## 4. Binary ownership is singular

An exact-build callsite has one owner.

For Black Plague:

- `NativeInputBridge` owns native movement/input query boundaries;
- `D460A -> D6E00` remains the single character-body update owner;
- `0xD7281` remains the bounded horizontal request injection boundary;
- other consumers receive status/fan-out rather than installing overlapping hooks.

Never add a second `D6E00` call to imitate Rework's source-level sequencing.

## 5. Same-tick body reconciliation is the current contract

The former post-tick plan-for-next-tick structure was replaced after the audit.

The supported contract is:

```text
before native body update
  resolve body/generation/B0
  consume identified tracking + logical locomotion intent
  plan the current tick

inside existing 0xD7281 owner
  consume the bounded horizontal request once

native D6E00 update exactly once

after native update
  observe B1
  match tick/body/generation/epoch
  reconcile once
  carry accepted locomotion once
  publish body/anchor result
```

Do not restore next-tick planning against stale B0.

## 6. Horizontal VR movement does not own native vertical gameplay

Shared horizontal room-scale/direct locomotion must preserve native Y, gravity and jump ownership unless new evidence requires otherwise.

Do not retune jump force/gravity as a side effect of horizontal comfort work.

## 7. Crouch is persistent desired state plus native application

The current contract is derived from Rework:

- shared policy owns desired stance/latch semantics;
- Black Plague applies crouch/walk through native `ChangeMoveState(4/0)` on the existing game-thread owner;
- native crouch input remains native when VR is not the owner;
- stance ownership is bound to session/player generation;
- blocked stand is distinct from desired crouch and must retry when clearance returns.

Do not return to compensating press/release toggles.

## 8. Presentation samples are single-consumption compositor frames

Real headset candidates exposed two presentation regressions after the audit:

1. nested visibility callbacks could reacquire compositor poses inside eye rendering;
2. an already-submitted presentation sequence could be reused by a later world frame.

The current contract is one owned presentation sample per compositor frame with explicit sequence freshness/consumption. Nested consumers reuse the owner sample; submitted/older sequences are rejected.

Do not remove freshness rejection merely to avoid a skipped world frame.

## 9. Validation status is part of the architecture

Use:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

A subsystem may legitimately have different statuses for different contracts. Do not flatten them into a single “works” label.

## 10. Palm contact and free-body grab are different contracts

Free-body grab may place a body kinematically relative to a resolved grab pose. That is not the same as reproducing Rework's collision-resolved palm/contact behavior.

The Black Plague native shape-query ABI and no-write diagnostic are live-tested (PID 28412). PID 8644 also live-tested the corrected backend-owned palm-shape lifecycle and isolated Rework-derived resolver with balanced create/reuse/destroy behavior and unchanged selected gameplay memory. Gameplay palm publication and any generalized character/held-body exclusion contract remain separately evidence-gated.

Do not solve long-body/contact defects with springs, global offsets or longer rays before validating the contact contract.

## 11. Jointed mechanisms stay native until adapted deliberately

Doors, levers, sliders and other joints are not free-body props.

Keep them on their native path until their state/constraint boundaries are mapped and adapted. Do not claim mechanism support merely because generic picking can select them.

## 12. Rich Black Plague hand articulation must not regress

Black Plague's richer finger articulation is useful evidence for shared semantics. Preserve it while separating rig-specific bone/profile data.

Do not downgrade it to Overture's exact implementation just for symmetry.

## 13. Tool sockets are profile data

Measured flashlight/glowstick sockets and model geometry belong to per-game/per-tool profiles.

Do not turn those offsets into global hand calibration or use them to compensate for an unresolved palm/contact mismatch.

## 14. Exact-build evidence does not generalize automatically

A source declaration or one game's reverse-engineered RVA is evidence only for the environment it actually describes.

- Overture source symbols do not prove Black Plague ABI.
- Black Plague ABI does not prove Requiem ABI.
- A hooked initialized capture is not automatically a pristine executable image.

Record hashes/provenance and fail closed on unknown builds.

## 15. Lifecycle may be partial

Probe initialization/shutdown can end in a partial state if rollback or teardown fails. The state machine must describe what is actually installed and remain retryable.

Do not report clean/ready merely because a high-level operation returned.

## 16. Historical crash causes remain historical until proven

The old SDL APPCRASH was not causally attributed. Later OpenVR error-108 regressions had different, demonstrated compositor-ownership causes.

If an SDL crash reappears, capture dump/stack/registers/modules. Do not blame the activation mutex, shadow or latest feature solely because they were nearby in time.

## 17. Extraction follows demonstrated reuse

Shared runtime should own neutral units, transforms, policy and algorithms only after their semantics are demonstrated. Backends/adapters own native execution, ABI, state classification and engine phase placement.

Do not generalize Requiem or future games prematurely. NO abstraction is preferable to a false cross-game contract.

## 18. Rework is the baseline; Framework is the forward integration line

The proven Overture Rework revision is a behavioral floor for already-solved VR behavior, not a permanent upper bound on Overture inside the Framework.

When Black Plague, Requiem or later Framework work demonstrates a better behavior that is genuinely game-neutral, the preferred direction is:

`proven behavior -> Framework validation -> shared runtime -> all compatible consumers`

Promotion still requires evidence and a real second consumer; one backend's engine-specific implementation is not sufficient proof of a shared abstraction. Once promoted, the Framework-owned Overture product should consume the improved shared policy so advances made elsewhere also improve Overture. Keep game-specific mechanics, exact-build data, model/profile values and incompatible native ownership in their existing backend/profile boundaries.

The separate Rework repository remains the historical/proven comparison reference. The maintained product evolution happens in Penumbra VR Framework.
