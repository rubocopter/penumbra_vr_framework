# Debug handoff — known VR problem boundaries

This file prevents repeated symptom-level fixes from replacing evidence-backed investigation. Read it before revisiting any of these issues.

## Current headset/offline checkpoint (2026-09-16)

PID 25484 is the newest focused headset evidence. The user reported that the
session felt good. `Analyze-BlackPlaguePresentation.ps1 -CollisionComfort`
reported 12,949 frames, 12,859 gameplay frames, 12,858 presentation frames,
zero stereo failures and 2,477 body samples (`2407 free / 43 blocked / 27
partial`) across 5,488 active room-scale render samples. Crouch telemetry
calibrated standing, reached physical entries/exits `3/3`, native entries/exits
`7/7`, ended standing in move-state `0` with VR ownership released, captured a
stable button-only state-`4` interval and covered `0.961 m` of continuous tracked
render Y. Direct metric locomotion was accepted in 650 body samples. This is
positive headset evidence for the current crouch/Y, direct-locomotion and
rejected-direction comfort path.

The focused helper still cannot claim a complete automatic pass from this log:
the Hybrid combined state was captured once, but there is no later periodic
sample with `physical=0 button_latched=1 ... native_state=4` after that combined
state. Treat the Hybrid release-hold interval as missing evidence, not as a
demonstrated gameplay failure. Blocked-stand/low-ceiling and yaw/bob remain
separate gates.

PID 22096 is the current presentation evidence. The `7f84235` run crossed
menu -> gameplay and produced 15,990 logged frames, 15,887 gameplay frames and
zero stereo/compositor failures. The single-consumption sequence gate therefore
has positive headset evidence; do not reopen compositor error 108 without a new
contradicting run. The same log is also the current evidence source for the
remaining short-X/Z comfort defect. Offline analysis found 2,060 meaningful
physical samples (`1755 free / 215 blocked / 90 partial`) and repeated render
prediction resets around a subset of rejected collision solves. The active
working-tree fix carries the last physical reconciliation into render and
projects out only the prediction component that continues into the rejected
direction. This preserves tangential slide and retreat. PID 25484 subsequently
headset-exercised the filter with positive subjective comfort and the collision
sample distribution above. Keep deliberate wall/slide and the exact PID 20520
pullback reproduction as narrower follow-up evidence rather than treating the
filter as host-only.

PID 6016 showed that `ca099ca` did not close the compositor regression. The
transition log contained one successful tracked-menu submission, then gameplay,
then a left-eye `VRCompositorError_AlreadySubmitted (108)`. The added
acquisition/reuse counters were `0/0` in the logged failure frame, proving the
earlier diagnosis of nested visibility acquisition was not sufficient. The
remaining contract violation was stale snapshot reuse across world frames: the
presentation snapshot was freshness-bounded by time but not by consumption.
The current code records the last successfully submitted presentation sequence
and rejects any zero/equal/older sequence before rendering eyes. Do not remove
this gate to make a stalled frame render; a skipped world submit is preferable
to submitting an eye twice between `WaitGetPoses` calls. Telemetry now logs
`presentation_pose_stale_rejects` plus every acquisition/reuse event so the next
headset run can distinguish missing visibility publication from duplicate
consumption. Debug, Release and SDK-less Release each pass 34/34 after the
change; metadata validation passes. The exact-build verifier was later rerun
successfully. This paragraph is retained as failed-candidate history; PID 22096
above is the later positive headset result.

The first two headset attempts from candidate `3333be1` (PIDs 23656 and 21396)
failed at the presentation boundary, not at locomotion/crouch ownership. Both
initialized the probe/controller path, then the first gameplay stereo submit
returned OpenVR error 108 (`VRCompositorError_AlreadySubmitted`). The regression
was introduced by visibility-owned presentation sampling: nested
`UpdateRenderList` calls from the stereo eye passes could call `WaitGetPoses`
again and replace the sample owned by the outer presentation frame. The current
fix rejects non-gameplay visibility cameras before sampling and makes nested
eye callbacks reuse the existing presentation snapshot. Debug, Release and
SDK-less Release each pass 34/34 after the fix; metadata, the exact-build BP
verifier and the Overture `-Full` regression also pass. Treat `3333be1` as a
failed headset candidate; PID 22096 later proved normal menu/gameplay transition
and sustained stereo submission for the single-consumption fix.

The current tree contains the five-phase intervention plus the rejected-direction
render-prediction comfort filter. Debug, Release and SDK-less Release pass the
documented root CTest gate. Metadata validation and the supported-image BP
verifier pass without modifying a process. PID 25484 gives headset evidence for
the current crouch/Y, direct-locomotion and short-X/Z comfort path; presentation
epochs beyond sequence consumption, interaction generation edge cases and the
partial lifecycle ledger still require their narrower validation. Overture
`-Full` remains the shared-runtime regression gate.

Remaining Hybrid release-hold, blocked-stand/low-ceiling posture edges, yaw,
palms and lifecycle retain their documented validation states until fresh live
evidence covers them. Short physical-motion comfort now has positive PID 25484
headset evidence, without implying every wall/slide edge was exercised.
Static exact-image evidence now pins `CheckShapeWorldCollision` at `0xD4830`, its nine stack
arguments, callback slot, legacy contact layout, body matrix/shape accessors,
CreateBoxShape, shape user count and destruction route. PID 28412 live-tested
the default-off no-write query with one callback/eight contacts and no selected
native-memory change. PID 8644 then live-tested the backend-owned shape lifecycle
plus the Rework-derived resolver behind the second default-off gate. Gameplay is
now connected and host-tested: held-body exclusions feed the resolver and the
resolved palm drives visible hands, held-object motion and tools. Acquisition
intent separately follows Rework's bounded raw-controller extension from the
collision-stopped palm. PID 23000 reached that
path in the headset, but severe FPS loss and a right-controller dropout make it
inconclusive for promotion; the next palm evidence must come from a clean A/B
run.

## 0. Hook and loader lifecycle

The first 2026-09-13 room-scale helper attempt stopped before launching the game
with `Flushing the VR settings transaction failed with Win32 error 2`. The
all-null `WritePrivateProfileStringW` cache-flush form returns zero when it
flushes; that return is not a normal success Boolean. The settings store now
ignores that special return, opens the temporary file with write access, calls
`FlushFileBuffers`, and only then performs the existing atomic replacement.
Do not diagnose a repeat from an old launcher: rebuild before retrying.

The 2026-09-12 maintenance pass made IAT writes transactional when protection
restoration fails, added complete/partial-state handling to OpenGL matrix
telemetry and spatial interaction, and retained the OpenVR loader handle after a
failed `FreeLibrary`. These changes are implemented and statically reviewed but
were not built or executed in that session.

The follow-up lifecycle hardening adds compensating shutdown after launcher
initialization failures and moves spatial interaction/native input teardown to
hook removal followed by callback quiescence waiting. These changes are also
only statically reviewed and do not change the validation state.

The probe DLL remains process-resident by policy. Active-callback counters only
cover wrappers after entry and are not proof that a DLL can be unloaded safely.
A future production bootstrap must retain resident lifetime or introduce a
stronger dispatch/quiescence protocol before calling `FreeLibrary` on the probe.

## 1. Black Plague positional head/body movement

### Observed

The user previously reported world displacement / collision discomfort while moving physically in VR. Leaning the HMD can move the view while the native body remains elsewhere, producing substantial discomfort.

PID 24956 then reported that rotating/tilting the head while physically staying
in place made the character walk in that direction. Stick movement added to or
opposed that unintended locomotion. Do not normalize this as expected vector
addition: only a deliberate horizontal head/torso translation is room-scale
movement. Static comparison also found a Framework-only amplification source:
the combined tick's physical component was reconciled and then reused by the
locomotion-only anchor carry. Movements caused while partially removing the
headset reached the existing `0.05 m` per-tick clamp and are unsuitable for
comfort tuning, but did not cause the diagnosed sequence difference.

PID 21548 confirmed that the carry correction removed that locomotion symptom,
and the helper passed `201/201` queue/consume/inject/match plus all physical and
crouch gates. The user still saw continuous world shake. Across the fresh log,
stick-zero frames had a 9.65 mm median reconciled offset, a 15.77 mm median
sample-to-sample change and 430 direction-reversal windows out of 539. This is a
real comfort failure, not headset jitter being mislabeled by the helper. The
renderer was holding body-owned translation at ~60 Hz while rotation and stereo
presentation ran near 90 Hz, and it applied the offset relative to Black
Plague's native smoothed/bobbed camera. Rework places its view from the VR world
anchor and updates tracking at 90 Hz. The current implementation now predicts
from the fresh reconciled anchor to the latest render pose and replaces native
camera X/Z rather than inheriting it. At that implementation checkpoint the
correction was compile-only; PID 13672 below provides the later headset evidence.

PID 13672 headset-exercised that render-placement correction. The user reported
that the continuous shake was gone and the overall sensation was much improved.
The helper's final `blocked` failure is now understood as a classifier mismatch:
it used total accepted-vector magnitude, whereas Rework measures accepted motion
along the requested direction. A lateral native solver correction could
therefore make a direct block look like slide/partial. The helper now uses the
Rework projection rule; no gameplay collision code changed. The remaining user
  report was directional: forward stick could feel offset unless a recenter was
  done. Static comparison found that the remap was deriving heading through the
  rendered/native camera even though Rework derives movement from current HMD world
  orientation. PID 11804 exercised the tracking-only correction and confirmed
  that the direction now follows the HMD without recenter while presentation
  remains stable. It also exposed signed native-axis speed asymmetry: corrected
  visual forward could still inherit the slower native backward speed. The run
  did not capture blocked or slide/partial, so the helper correctly left those
  outcome classes incomplete.

There is one demonstrated turn-ownership difference worth testing. Rework
`23c890f::UpdateVRTurn` applies snap/smooth turn through
`vr_tracking.AddWorldYaw`, then builds locomotion directly from the current head
world forward/right vectors. Black Plague still applies the shared turn amount
through native `cPlayer` yaw (`0x9CD00`), while the transient direct locomotion
path now builds direction from the tracked head world pose. That mixed ownership
must be exercised over several VR turns before changing turn ownership; the
difference alone is not proof of a bug.

### Established facts

- Framework positional HMD translation remains default-off. PID 21548
  live-exercised its transient active mode and all physical outcomes. The carry
  correction is now live/headset evidenced for removing tilt-induced
  locomotion. PID 13672 headset-exercised the subsequent render-placement
  correction and removed the reported continuous shake. PID 11804
  headset-exercised the subsequent tracking-only stick heading and confirmed its
  direction, but exposed native-axis speed asymmetry. PID 8092 later supplied
  headset evidence for the corrected direct metric locomotion and collision
  route, and PID 25484 adds focused evidence for the current crouch/Y and comfort
  composition. Keep the narrower constrained-state, deliberate wall/slide and
  yaw ownership gates separate.
- `cPlayer+0x274` maps to the native `iCharacterBody` on the supported exact build.
- Current/previous position, active size, physics body and physics world are mapped and live-observed.
- The active standing player shape is a `0.70 x 1.65 x 0.70 m` cylinder, radius `0.35 m`.
- The normal body update is `D460A -> iCharacterBody::Update(D6E00)` at `dt=1/60`.
- Horizontal collision begins at `D7312 -> CheckShapeWorldCollision(D4830)` and later native phases apply step/gravity/state work.
- PID 30896 live-observed free movement, total block, slide/partial acceptance and up to ~`0.436 m` unapplied HMD/body divergence.
- The first narrow `BlackPlagueBodyAdapter` is now live-tested. It never calls `D6E00`; it binds to the existing movement/body-update owners and observes accepted displacement after the one native tick.
- PID 8628 live-observed free movement, total blocking, sliding and body replacement with the adapter active. Native body updates remained ~60 Hz; no second `D6E00` was introduced.
- `runtime::VrAcceptedBodyMotion` is shared by Overture and Black Plague as the game-neutral accepted-displacement observation.
- Shared reconciliation planning/rebase, physical-rejection correction and
  locomotion-anchor carry are implemented in `vr_locomotion.*`. The Black
  Plague shadow consumer remains observation-only and default-off; a separate
  default-off physical request exists at `0xD7281` and is now live-tested in
  PID 26144.
- Rework resolves physical movement and stick movement in two ordered body
  updates. Black Plague must keep one native `D6E00`, so it combines them at
  `0xD7281`; only the non-physical remainder may feed Rework's locomotion carry.
- PID 28172 live-tested the shadow consumer through the transient mutex with positional translation still zero: stationary/small-head-motion planning, native free/block/slide, recenter/body replacement and the existing ~60 Hz single native tick were observed without a second `D6E00`.
- GitHub Actions has host-tested the current shared extraction on Windows x86: root metadata/Debug/Release tests pass, and the autonomous Overture Release regression job also passes.

### Do not try again

- Do not add a fake head collider.
- Do not translate the camera from an unconstrained raw HMD origin. The active
  path must start from the fresh reconciled head anchor. It may advance that
  anchor by only the HMD delta since the body sample to bridge the demonstrated
  60/90 Hz boundary, and must apply the same result to camera visibility and
  controllers.
- Do not tune arbitrary positional multipliers to hide clipping or push-back.
- Do not add another owner for `MoveForward/MoveSideways` or `D460A`.
- Do not call `D6E00` from `BlackPlagueBodyAdapter`.
- Do not treat the shadow physical plan as accepted/rejected movement; it is not injected.

### Next evidence

The shared reconciliation phases and default-off BP shadow consumer are now
**live-tested** through PID 28172. The earlier forwarded-`LoadLibraryW` startup
race is also live-confirmed past the point that previously failed: the fresh
Steam-started process installed the bridge/body adapter and activated the shadow
from the transient mutex.

The shadow-only mode still plans without injection. Separately, the bounded
physical X/Z request boundary is now live-tested at exact-build
RVA `0xD7281`, immediately before the native X/Z comparison/collision path. It
injects at most `0.05 m` horizontally, forces Y to zero, is one-shot for the
current body, and is consumed by the existing single native tick without a
second `D6E00`. `MoveForward/MoveSideways` remains native acceleration state and
is not used as this metric displacement mechanism.

PID 26144 closed that live gate: queue → matched injection → native collision
solver → measured acceptance/rejection was observed with positional HMD
translation disabled. The corrected classifier treats sub-2 mm native/physical
X/Z motion as stationary headset jitter, matching the fact that Rework 23c890f
reacts to any non-zero HMD delta. Re-analysis of the session yields 12
stationary, 37 free, 2 blocked and 15 slide/partial samples. Camera/bob remains
separate comfort work.

The next path is now implemented behind the separate
`Local\PenumbraVR.BlackPlague.RoomScaleValidation` request. It fails closed
unless physical validation is also active, expires camera samples after 250 ms
and applies the reconciled/predicted X/Z anchor consistently to camera, culling
and controller space. Run `tools/Start-BlackPlagueRoomScaleValidation.ps1`; do not promote it
past `implemented` until host evidence and the required headset pass
exist.

`tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1` enforces that gate
rather than treating activation as success. It also classifies stationary,
free, blocked and slide/partial samples from the existing pre-injection physical
request/accepted-displacement fields. Its stationary classification now accepts
sub-2 mm injected tracking jitter instead of requiring a mathematically zero
request. PID 18392 deliberately ended after
activation only and the helper correctly failed because no queued,
consumed/injected, reconciled or injected-body telemetry existed. Do not count a
future repetition as successful on activation evidence alone.

## 2. Black Plague locomotion speed / timing

### Observed

VR movement has felt substantially faster than Overture/Rework and native walking/bobbing is uncomfortable in-headset.

### Established facts

- `cPlayer::MoveForward/MoveSideways` feed native movement state through `iCharacterBody::Move(D4F50)`.
- The native body update consumes/decelerates movement once at `D460A -> D6E00`.
- Effective native walk/sprint limits are about `3.0 / 4.5 m/s`.
- Rework uses direct VR displacement policy of `1.5 / 2.25 m/s`, with `0.05 m` physical steps.
- Scaling analog input only changes native input/acceleration magnitude; it is not equivalent to Rework's locomotion policy.
- The earlier ratio≈2 timing diagnostic measured `cButtonHandler::Update` callbacks, not physics. The body probe consistently observes the real body tick at ~60 Hz.
- The transient active room-scale path now ports Rework's direct `1.5 / 2.25
  m/s` policy through the existing `0xD7281` owner. PID 8092 supplied headset
  evidence for this technical route. The default path still preserves native
  tuning.
- PID 17612 proved the first direct-locomotion permission gate was impossible:
  full left-stick analog reached the controller frame, but all `locomotion_*`
  fields stayed zero. `MoveForward/MoveSideways` test `amount == 0` before their
  later `+0x264 = 1` write, so clearing the VR native axis prevented that byte
  from ever becoming the intended permission signal. The corrected path invokes
  only the exact pre-Move state gates and checks `+0x268/+0x26C`, then queues the
  metric request through the existing collision owner and mirrors `+0x264` after
  successful publication. Do not reintroduce `+0x264` as a pre-publication
  oracle.
- PID 28996 proved that the first replacement predicate still decoded the two
  indexed state containers incorrectly. The exact functions load
  `vector +0x2C4 / index +0x2BC` first and `vector +0x2D8 / index +0x2D0`
  second. The earlier code omitted the `0x200` portion and reversed the
  vector/index arguments, so both state gates failed before publication.
  PID 8092 then headset-validated the corrected mapping: stick locomotion works,
  `direct_locomotion=True`, all physical outcome classes were captured and
  queue/consume/inject/match was `201/201/201/201`.

### Do not try again

- Do not declare the issue fixed by changing `MoveSpeed` alone.
- Do not alter simulation rate or gravity to compensate for perceived speed.
- Do not route direct VR movement through a second `D6E00` body update. Black
  Plague owns one native update and one exact-build pre-collision request.

### Next evidence

The direct metric path has PID 8092 headset evidence. PID 20520 nevertheless
reported a remaining pullback sensation during short physical X/Z translation,
so positional comfort is not closed. The focused crouch/Y run must retest small
`5–10 cm` movements without requiring room-scale walking. Explicitly record
whether native footsteps, bob and body animation still trigger during direct
locomotion, because that remains a separate presentation/effects observation.

## 3. Black Plague jump

### Established facts

- Jump dispatch selects state index `3`, source-correlated with `cPlayerMoveState_Jump`.
- `EnterState` establishes vertical force before the following `D6E00`.
- First observed accepted vertical speed is ~`+5.53 m/s`; apex is ~`0.95 m` above baseline.
- Landing is resolved by the native body tick before state `3 -> 0` on the next state update.
- `requested.y` and the first horizontal solver Y stay zero throughout the burst; final accepted Y changes in the later native vertical/state path.
- `+268` is a 25-tick ground-grace counter.
- `+26C` remains an unclassified alternate jump-eligibility branch.
- `+204=0.3` is a Jump hold threshold, not a hard maximum for `+200`.

### Do not try again

- Do not fold vertical movement into the shared horizontal intent contract.
- Do not copy Overture jump constants into Black Plague without a separate design/validation milestone.
- Do not rename derived acceleration regimes as “gravity constants” without native ownership evidence.

### Next evidence

None is required for the shadow-only tracking/body validation milestone. Keep native jump ownership intact. VR jump comfort/tuning can be a later isolated change after the body path is stable.

## 4. Black Plague crouch

### Established facts

- Crouch is a real native body/shape transition, not merely a camera offset.
- Standing size: `0.70 x 1.65 x 0.70`.
- Crouched size: `0.70 x 0.95 x 0.70`.
- Feet Y remains fixed while body centre changes.
- The active physics-body pointer changes and restores.
- Exact native entries `0x9CFA0/0x9CFD0` are the crouch pressed/released
  dispatches owned by the game's existing move-state/body-shape path. They are
  affected by the native hold/toggle crouch setting and must not be treated as
  unconditional crouch/stand setters. A failed stand leaves the `0.95 m` shape,
  which permits a safe retry without importing Overture's `CanStand` layout.
- PID 20520 proved tracked-height entry worked in the first integration but
  native exit did not remain synchronized: standing physically could leave the
  native shape crouched until another crouching gesture. The old helper passed
  falsely because it compared aggregate counters and an uncorrelated shape
  sequence.
- Rework `23c890f` uses one persistent button latch and computes desired crouch
  as physical OR button. The failed Black Plague build instead sent
  held/released edges into a configurable native toggle path.
- Shared policy now owns the Rework latch, `(0.90, 2.20) m` plausible range,
  upward-settling baseline, `0.25 m` default depth and `0.08 m` hysteresis. The
  existing Black Plague game-thread owner applies desired stance with the exact
  native entries, adopts legacy edges, collapses duplicate OpenVR/legacy input,
  retries blocked stand and logs desired/native correlation.
- PID 23260 separated policy/input from the native stance defect. Physical
  policy reached `10/10` entries/exits and `button_latched` toggled on and off,
  but native stance reached crouch and never returned: `native_exits=0`, final
  `native_crouched=1`, `vr_owned=1`, `stand_retries=17832`. Therefore the right
  stick/OpenVR button route is not the missing boundary. In native toggle mode
  the release dispatch leaves crouch latched. A release/second-press adaptation
  fixed that stuck shape but was not the final owner.
- PID 24948 exposed the remaining conceptual error. The user saw a short
  down/up motion instead of persistent crouch/stealth, while telemetry reached
  `native_entries=14` and `native_exits=14`. The body shape was being toggled,
  but the backend was still feeding a persistent desired state through native
  configurable press/release callbacks. Exact-build decoding identifies
  `cPlayer::ChangeMoveState` at `0x9C750`; the original normal-state crouch
  handlers prove move-state `4` is crouch and `0` is walk. The game-thread owner
  now applies those states directly, matching Rework's ownership model. Probe
  telemetry includes `cPlayer+0x2D0`, and validation requires move-state and
  collider shape to agree. Release build, exact-image verification and 30/30
  host tests pass; this direct-state correction is not live/headset validated.
- Rendering now composes continuous physical HMD Y through `VrTrackingSpace`
  from the reconciled feet anchor. A physical crouch does not add the native
  full camera drop; button-only crouch applies the configured posture offset.
  This corrected combination is host-tested only.

### Do not try again

- Do not import Overture `CanStand` semantics into Black Plague without evidence.
- Do not implement physical crouch as a camera-only height change.
- Do not feed the shared desired posture back through Black Plague's configurable
  held/released toggle queries.

### Next evidence

Run `tools/Start-BlackPlagueRoomScaleValidation.ps1` and require two correlated
physical/native entry/exit cycles with move-state `4/0`, final native standing
with VR ownership released, a stable button-only state-4 crouch/stealth interval
followed by state 0, Hybrid composition, at least `0.15 m` tracked-Y range,
post-crouch stick recovery and subjective short-range X/Z comfort. A
blocked stand should increment retry telemetry and remain crouched until clear;
it must not be described as validated until exercised in the headset.

## 5. Black Plague held-object collision

### Established facts

- Exact build: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- `iPhysicsBody +0x3C8` controls the native character-aware collision behavior observed at the mapped world/ray/contact boundaries.
- The current grab path snapshots the original value, disables it only while an eligible free body is owned, then restores the exact original value on release.
- Parent/joint bodies are excluded from the free-body path.
- The binary has no separate Rework-era player-only collision filter demonstrated yet; the current implementation is intentionally conservative.
- Static exact-image evidence pins `CheckShapeWorldCollision` at `0xD4830`
  (`ret 0x24`), callback slot zero, the VC7 `cCollideData` pointer/count layout,
  `0x1C` contact stride, body matrix/shape accessors, CreateBoxShape and
  reference-counted destruction through `DestroyShape` at `0xD4210`.
- The same exact image now pins the filtering order needed by the palm path:
  `D48FB/D4903` rejects bodies marked as characters when stack argument 8
  (`collideCharacter`) is false, while `D4919` independently rejects the exact
  stack-argument-4 `skip_body`. The host resolver test verifies its native call
  uses `skipStatic=false`, `isCharacter=false`, `collideCharacter=false` and the
  supplied skip body. The current gameplay path now publishes the held body per
  hand into that slot, but no tracked held-body contact has been promoted beyond
  host-tested because the only headset exercise was inconclusive.
- `hand_contact_probe.*` retains the live-tested default-off no-write diagnostic. It
  runs from the existing post-`D6E00` fan-out, reuses the current character
  body shape and rejects any selected native-byte mutation. PID 28412 passed it.
- The same backend now owns a separate default-off palm resolver gate. It creates
  the shared-size box through the pinned `CreateBoxShape` slot, reuses it in the
  same world, safely replaces it when world identity changes, destroys it through
  `0xD4210`, and feeds callback contacts to the shared Rework-derived resolver.
  Host tests cover normal reuse, world replacement and gameplay-memory guards.
  The gameplay integration now applies the resolved grip to visible hands,
  physical interaction and tools while keeping aim raw.

### Next evidence

PID 8644 passed `--validate-palm-resolver` in a loaded map without VR: one
create, one reuse, six native queries and one destroy, with `shape_type=1`,
`shape_users=0` and `gameplay_memory_changed=false`. The lifecycle/resolver gate
is therefore live-tested. Character and single-body exclusion semantics are now
exact-image pinned and host-tested. Per-hand held-body publication and gameplay
resolved-palms are implemented and host-tested. PID 23000 exercised this path,
but severe FPS loss and a right-controller dropout make the session inconclusive;
repeat representative contact only after a clean reboot and compare palm
collision disabled/enabled before attributing the frame-rate issue.

PID 4720 then produced an apparent room-scale/crouch regression during the
focused palm helper. The log disproves a body-path conclusion from that run:
startup had `physical_displacement_validation=0`, `room_scale_validation=0` and
`positional_translation_enabled=0`; only the palm mutex became active. The palm
helper now requests the known-good physical-displacement/room-scale stack in the
same process and checks for active room-scale rendering plus valid tracked-crouch
telemetry before it accepts the session.

The same report that picking had become difficult exposed a real Framework /
Rework difference. Exact `23c890f` uses the collision-resolved palm for visible
contact and ownership, but constructs an interaction pose whose translation may
follow the raw controller by up to `0.18 m`. The old Black Plague adapter used
the stopped palm for both ray selection and the final contact-distance guard,
so a prop just beyond a blocked visible palm could be rejected on acquisition.
The adapter now uses the bounded Rework interaction pose for
selection/revalidation and keeps the resolved palm for the actual Grab/Move
anchor. The synthetic adapter test covers that regression.

The same PID 23000 session exposed a separate placement symptom: many props
appeared far from the hand while long wooden boards/bars behaved better. Exact
state analysis showed that only action-state `Grab=6` used the rigid VR grab;
many props entered `Move=2` and retained native distance manipulation. The
current host-tested fix pins Move's own state slots/contact fields and ports the
Rework free-body behavior: preserve the selected local contact, drive that point
to the resolved palm with force, and leave jointed/mechanism bodies native.

PID 30032 reached this gate and failed at the creation-validation boundary before
resolver execution. Review against Rework/HPL1 exposed the host assumption that
caused the false negative: `iCollideShape` starts at zero users and
`CreateBoxShape` does not increment it; bodies increment the count only when
they adopt a shape. The host fixture had used one user and therefore failed to
model the native standalone lifecycle. The boundary and fixture now require
`user_count=0` plus exact box type/world/vtable identity. PID 8644 then passed
the corrected gate in a fresh process, closing that false-negative boundary.

## 6. Long bars, doors and mechanisms

### Established facts

Rigid free-body palm-relative attachment is appropriate only for eligible free bodies. Long tables/bars rigidly following the palm is a limitation of that model. Doors, levers, sliders and joints must use native mechanism state.

### Do not try again

Do not add arbitrary springs, offsets or special-case rigid-body behavior to make a mechanism look physical.

### Next evidence

Map the exact joint/slider/hinge state and update boundary for one representative mechanism, then build a dedicated backend adapter.

## 7. Glowstick / flashlight placement

### Established facts

Black Plague and Rework use different DAE resources. Rework's exact grip constants therefore cannot be copied blindly. The Framework now shares the proven attachment composition rule (local model-to-hand orientation followed by translation of the measured grip point to the hand origin), while Black Plague keeps its own measured flashlight/glowstick points and +90-degree X orientation. Existing exact matrix tests prove that this refactor preserves the prior BP placement numerically. The current BP profile is still visibly wrong against provisional hand geometry, so it is not a definitive placement result.

The Rework hand meshes/rigs already present at
`products/overture/data/models/hud_objects/hud_object_hand_rig.dae` and
`hud_object_hand_left_rig.dae` are now consumed by a narrow renderer-owned mesh
adapter. `tools/generate-rework-hand-mesh.py` verifies the authored rigid
one-bone-per-position skin, 17-joint hierarchy/inverse binds and triangle/UV
streams and emits checked-in renderer data plus a reduced copy of the existing
diffuse material. `DrawTrackedHands` keeps the collision-resolved Black Plague
palm as its world transform and maps the richer shared `VrHandArticulation`
output onto Rework's demonstrated long-finger/thumb axes. No HPL mesh hook or
new exact-build ownership boundary was added. The real-driver OpenGL path is
host-tested; hand scale/orientation/articulation and frame pacing remain a
headset gate.

### Do not try again

Do not tune the final socket against provisional hand geometry and then retune it again when definitive meshes/rigs land.

### Next evidence

Use the now-integrated hand geometry to measure the definitive per-game tool socket, then validate hand/tool scale, light direction and model placement together in the headset.

## Debugging rule

### 2026-09-14 body-transaction checkpoint

The short-motion pullback investigation now has a host-tested structural fix:
`BodyReconciliationShadow` no longer observes a finished native tick and plans
that physical delta for the next one. The existing character-update wrapper
prepares from B0 and the latest tracking sample before its one native update,
the existing `0xD7281` owner consumes that request in the same tick, and the
adapter completes from B1 only when tick/body/generation match. Recenter/body
replacement invalidates the transaction rather than carrying a request across
epochs. The body harness covers presentation rates above the 60 Hz physics tick,
repeated poses, ramp/stop, block/slide/jitter and same-tick physical/stick
partitioning. Debug/Release and SDK-less Release currently pass 34/34 host tests.

PID 22096 then showed that same-tick body ownership was not the whole comfort
story. The body solve could reject a physical direction while the render path,
running between body ticks, still extrapolated the newer HMD pose into that same
direction and then snapped back when the next reconciled anchor arrived. The
current `FilterPhysicalRenderPrediction` fix uses the last physical
reconciliation to remove only the into-rejection component; tangent and retreat
remain available. Host tests cover free, blocked and away prediction. This does
not yet prove the headset pullback is gone.

The direct-stick producer
now publishes logical direction/magnitude/sprint plus the identified head pose;
the body transaction integrates that intent with its physics `delta_seconds`.
Do not reopen the input-callback `dt` path. Exact-build analysis now identifies
Black Plague action-state indices `1` and `2` as Push and Move respectively, so
those two states feed the shared constrained `0.5 m/s` policy. That mapping is
**host-tested only**; PID 8092 predates it and is not headset evidence for the
constrained branch.

The current crouch ownership/lifecycle hardening is also host-only. Native
crouch results are preserved outside the VR ownership window, VR-owned stance is
generation-bound and `ChangeMoveState(4/0)` remains the application boundary;
blocked stand is represented separately in shared policy. Probe teardown now
retains partial state and can be retried. The real NativeInputBridge contract
harness now covers session/focus/disconnect/player replacement/double-edge and
low-ceiling cases, and the probe lifecycle harness injects every install,
rollback and teardown failure point. Both are part of the current 34/34 host
suite. These contracts remain **host-tested only** until fresh live evidence
exercises the corresponding Black Plague paths.

For any repeated defect, write down:

`OBSERVED → REWORK BEHAVIOR → CURRENT FRAMEWORK BEHAVIOR → DIFFERENCE → EVIDENCE → ROOT CAUSE → MINIMAL FIX → VALIDATION`

If root cause is not supported by evidence, stop changing behavior and collect the missing evidence first.
