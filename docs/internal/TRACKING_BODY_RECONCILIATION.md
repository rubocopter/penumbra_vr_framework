# Tracking/body reconciliation — implementation and validation report

Date: 2026-09-11. Base: `main`, `7da8f9f283e3c7f429059575a6319ff31fef77af`.
The checkout was clean and `HEAD...origin/main` was `0 0` before editing.
Rework was inspected at `23c890f7dbd06b939be9951d282e6e948d9a6623`.
No commit, push, PR, game launch or SteamVR launch was performed.

**Status: implemented, portable tests passed; Windows host gates pending.**
Do not call this milestone host-complete or ready for live testing yet.
BP physical request injection remains unavailable and positional translation is zero.

## 1. Exact Rework sequence

Sources: Rework `PenumbraOverture/Player.cpp:2221–2351`,
`HPL1Engine/include/game/VRTracking.h`, `PenumbraOverture/ButtonHandler.cpp`
(recenter and seated-baseline handling), and
`HPL1Engine/sources/physics/CharacterBody.cpp` (VR displacement branch).

1. Compare horizontal body/head-anchor separation with `0.8 m`. If greater,
   replace the anchor with the full body position **before** adding tracking delta.
2. Subtract previous raw HMD translation from current raw translation, remove Y,
   rotate by world yaw, and advance tracking history.
3. Add physical delta to the anchor. Only when delta is nonzero, request the
   horizontal anchor-to-body displacement, limited to `0.05 m`.
   This is one capped step, not a loop subdividing and draining the full delta.
4. Overture sets its source-added `vr_velocity`, enables `vr_stepstaticonly`,
   calls its body update and disables that flag. The source engine consumes
   displacement directly and returns from its special VR branch before the
   ordinary external-force path. These source additions are not a BP contract.
5. Project accepted horizontal motion along the request, clamp to `[0, requested]`,
   and subtract rejected distance along the request from the anchor only if
   rejection exceeds `0.002 m`. Lateral slide is not acceptance along the request.
6. Execute stick locomotion separately at `1.5 m/s`, `2.25 m/s` sprint or
   `0.5 m/s` constrained. Carry the anchor only if the Rework 3D distance
   comparison with `0.001 m` hysteresis says carrying does not make it worse.
7. Set anchor Y from feet, publish the player-world tracking anchor and consume
   movement intent once. Outside allowed body-motion states, track the head
   without moving the body.

Tracking scale remains `1.0`. Height mapping remains
`(raw_y - 0.2) * 1.065 + calibration + posture + seated_offset`.
Seated-baseline policy remains in Overture; recenter changes yaw and reseeds
previous tracking, preserving position/height. Yaw rotates direction deltas,
not world anchors. Neither policy is retuned here.

Rework's tiny physical-request projection guard is `0.0001 m`; the existing
Framework helper uses `0.000001 m`. This extraction preserves the Framework
helper; either is below the unchanged `0.002 m` correction threshold. Rework
zeros locomotion Y before carry. The Framework Overture path already accepted
the full observation; this extraction preserves that path exactly. BP explicitly
uses only horizontal native motion for carry and observes feet Y separately,
because its normal native tick includes jump/gravity processing.

## 2. Ownership table established before implementation

| Behavior | Current Overture owner | Shared policy | BP mechanism |
|---|---|---|---|
| Tracking transform | `VrTrackingSpace` | Existing transform | Raw render-boundary pose |
| Calibrated height | `VrTrackingSpace` | Existing mapping | Horizontal shadow needs no height tuning |
| Seated/standing | `UpdatePlayMode` + tracking offsets | Existing offset composition | No posture control added |
| World yaw | Tracking + input | Existing direction transform | Render camera/rotational anchor alignment |
| Recenter | `HandleInput` | Existing yaw behavior | Reset shadow history when anchor is recaptured |
| Physical horizontal delta | `UpdatePlayer` history | Neutral world delta input | Latest real tracking snapshot at each body tick |
| Maximum physical step | Existing clamp helper | `0.05 m`, reused by plan | Plan only |
| Body request | Source adapter | Data in metres | Missing physical injection capability |
| Accepted displacement | Source adapter + factory | Existing `VrAcceptedBodyMotion` | Existing post-native-tick fan-out |
| Collision rejection | Previously `UpdatePlayer` | Extracted physical reconciliation | Unknown for uninjected physical plan |
| Locomotion anchor carry | Previously `UpdatePlayer` | Extracted carry phase | Accepted native X/Z only |
| Large separation/rebase | Previously `UpdatePlayer` | Extracted pre-delta rebase | Same plan plus lifecycle reset |

No Requiem abstraction or generic player controller was introduced.

## 3. Shared interface and retained state

All three phases are in the existing `src/runtime/vr_locomotion.*`, so Overture's
existing MSBuild props already compile them; no Rework checkout dependency returns.

```cpp
VrBodyReconciliationPlan PlanBodyReconciliation(
    const std::array<float, 3>& head_anchor,
    const std::array<float, 3>& body_position,
    const std::array<float, 3>& world_tracking_delta) noexcept;

VrPhysicalReconciliationResult ReconcilePhysicalBodyMotion(
    const VrBodyReconciliationPlan& plan,
    const VrAcceptedBodyMotion& physical_motion) noexcept;

std::array<float, 3> CarryHeadAnchorWithLocomotion(
    const std::array<float, 3>& head_anchor,
    const VrAcceptedBodyMotion& motion) noexcept;
```

The plan contains validity, rebase, updated anchor and physical request.
The physical result contains validity, corrected anchor, correction vector and
rejected distance. The phases are stateless; hosts retain history and sequencing.
`VrAcceptedBodyMotion` is unchanged. No native pointers, offsets, RVAs, hook
ownership, BP speed flags or calling conventions enter the shared API.

Overture retains its existing raw-head history, anchor, tracking/calibration and
input state. BP's portable shadow consumer retains initialization, a neutral body
generation, previous raw head, previous accepted body position and shadow anchor.
The native adapter alone compares body identity (never dereferences that retained
identity), holds a synchronized pose/yaw snapshot and tracks sample freshness.
Every actual observation still dynamically resolves `cPlayer+0x274`.

## 4. Acceptance, rejection and lifetime behavior

- Free physical motion: no rejection correction.
- Full block: subtract the request from the planned anchor, subject to epsilon.
- Partial acceptance: subtract only the rejected projected portion.
- Sliding: project along the request; do not count lateral distance as forward
  acceptance and do not invent an additional lateral anchor correction.
- Stationary HMD: no new physical request, even if a residual anchor gap exists.
- Large separation: rebase before adding the new delta, matching Rework order.
- Body replacement: reset even at identical coordinates; seed from new body/feet,
  discard cross-body tracking/accepted-motion inference for that first interval.
- Shadow discontinuities: a horizontal tracking jump, body interval or gap over
  `0.8 m` resets history conservatively. These shadow-only guards are not changes
  to Overture behavior or evidence of teleport/collision mechanics.
- Missing/stale tracking, no owner, invalid pose/body/feet/dt: invalidate/no-op;
  recovery seeds a new baseline. Pose/body freshness is capped at `250 ms`.
- Native crouch: centre height changes do not imply body-generation replacement;
  externally observed feet height is used without stand-clearance assumptions.
- Native jump: original accepted Y remains available in telemetry. Physical
  requests and native carry corrections remain horizontal; feet Y is observed,
  never written to the native body.

No missing physical observation is converted into zero or total acceptance.
`physical_observation_available` remains false throughout BP shadow integration.

## 5. Overture equivalence

`UpdatePlayer` calls the extracted plan, then its existing physical adapter,
then physical reconciliation, then existing locomotion, then shared carry,
then feet/anchor publication. Source adapter, constants, body-motion gates,
input consumption, jump handling and seated/recenter/yaw ownership are unchanged.

The existing functional Overture test passes with its unchanged expectations.
The tracking/locomotion suite also passes. A deterministic differential harness
in `tests/overture_backend/reconciliation_trace.cpp` was built with the checkpoint
`overture_backend.cpp` and with the edited version using identical GCC C++20/O2
options. Both produced `53700beedf486b7f calls=944` over 512 frames: all hashed
result float bits and adapter call counts match. The trace covers full/block/
partial/slide, body rebases, constrained/sprint motion, disabled body motion,
yaw, recenter and seated/standing transitions. This is host evidence for these
cases, not exhaustive gameplay/headset equivalence or a Windows Release claim.

## 6. BP wiring, tick ownership and missing capability

The existing render callback publishes raw HMD pose and aligned world yaw to
`PublishBlackPlagueShadowTracking`. It uses the original rotational anchor,
not the hand-rendering anchor whose translation is overwritten each frame.
Invalid tracking/menu transitions invalidate shadow history. Recenter reseeds it.

The existing `BodyCollisionProbe` calls the adapter after its one native update,
now also passing the already-known physics dt. The adapter processes every
accepted tick directly rather than consuming the telemetry mailbox once per
render frame; intermediate accepted ticks are therefore not lost. Repeated HMD
samples produce zero new physical delta while native carry can still advance.

Ownership remains:

- `NativeInputBridge`: movement callsites.
- `BodyCollisionProbe`: existing native body-update callsite and original update.
- `BlackPlagueBodyAdapter`: fan-out consumer; no hooks and no direct native update.
- Shared policy and shadow consumer: pure data, no game writes.

The original `g_original_update(character_body, delta_seconds)` call is untouched.
The expanded Windows synthetic test checks counted native calls, owner failure,
dynamic replacement and actual hook → adapter → shared policy flow. It has not
been executed in this Linux environment; existing live evidence is not relabelled
as evidence for the new integration.

**Missing capability:** a bounded physical X/Z displacement request in metres,
resolved collision-aware within the existing single native tick, with accepted
physical motion identifiable separately from native locomotion. Overture has
source-added displacement/static-only mechanics; BP's mapped `Move` integrates
analog amount into acceleration/velocity. Multiplying input cannot establish
that equivalence. No injection, extra probing or direct body-position write was
added. Shadow validation alone cannot authorize positional translation.

## 7. Scope and diagnostics

Speed tuning, jump tuning, physical crouch, camera/bob, interaction, mirror,
Requiem and installer behavior remain unchanged. BP walk/sprint stay native.
`kPositionalWorldUnitsPerMeter` remains zero, now protected by a static assertion.
No shadow output reaches camera composition or movement publication.

`PVR_BP_RECONCILIATION_SHADOW=1` must be present in the **game process**, not just
in the separate injector, before adapter installation. It defaults off. No new
persistent setting is introduced. Its summary logs at most once per 300 swap
frames and reports tick/reset counts plus the latest physical delta/plan, native
accepted/body position, predicted anchor/carry correction, separation and rebase.
It explicitly labels physical observation unavailable and translation disabled.
The latest sample is not a full per-tick recording or proof that a short collision
was captured; existing body collision diagnostics remain available.

## 8. Tests and builds

Executed with `g++ -std=c++20 -O2 -Wall -Wextra -Werror`:

| Validation | Result |
|---|---|
| New shared reconciliation + portable BP shadow cases | Pass |
| Existing Overture backend test | Pass |
| Existing tracking-space/locomotion/interaction policy test | Pass |
| Existing VR matrix test | Pass |
| Body-adapter boundary test, linking its implementation once | Pass |
| Overture before/after differential trace | Identical |
| `git diff --check` | Pass |

The first manual tracking-test link omitted `vr_interaction_policy.cpp`; rerunning
with that target dependency succeeded. This was a manual harness dependency error,
not a source regression.

Added coverage includes zero/small/clamped tracking, finite safety, invalid dt,
rigid-pose validation, full/block/partial/slide/epsilon, native carry, no synthetic
physical rejection, zero vertical request/correction, same-position replacement,
load/teleport discontinuities, crouch feet and reset after missing samples.
The expanded Windows body-collision test additionally covers counted tick calls,
owner requirement, dynamic body resolution, tracking invalidation and default-off.
CMake registers the portable policy test as `body_reconciliation` (27 tests in
the corresponding full root configuration; not run here).

Attempts to run the requested Windows commands were blocked:

| Required gate | Limitation |
|---|---|
| Framework Release | `cmake` absent; Linux has no Windows/MSVC x86 toolchain |
| Full Release CTest | `ctest` absent; Windows tests not built |
| BP backend/probe Release | Same toolchain limitation |
| Exact-build verifier | `pwsh` absent; initialized image capture also absent |
| Metadata verifier | `pwsh` absent |
| Overture Release | `pwsh`/MSBuild absent |
| Historical Overture `VRTrackingTest` | Windows project; not built/run |

Do not replace these gates with the portable checks. A heavy package/deploy was
not attempted: no packaging rules, LAA transform or product assets changed, and
no Windows executable was produced. Overture Release and historical suite still
must run because its shared runtime changed.

## 9. Documentation and review state

Updated: this report, `CODEX_HANDOFF.md`, `DEBUG_HANDOFF.md`,
`docs/REWORK_PORTING_PLAN.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `CHANGELOG.md`,
and `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`. The README landing is unchanged.
The review snapshot below records final `git diff --stat` and
`git status --short --branch`, including new files marked intent-to-add so the
diff includes their contents. No content is staged for commit. HEAD is unchanged.

## 10. Next validation gate

**Not ready for live testing yet.** First run Windows Release/full CTest,
exact-build/metadata verification, Overture Release and its historical suite.
Resolve any failure before producing a test DLL; no commit is required here.

After those gates pass, the minimal live protocol is shadow-only:

1. Use the supported build and freshly built DLL. Enable only the shadow
   environment option in the game process before attachment; keep positional
   translation at zero and preserve existing VR settings.
2. Observe a stationary baseline, then small physical horizontal head movements.
   Plans must stay at or below `0.05 m`, without changing the displayed position.
3. Use existing keyboard/controller locomotion briefly in free space, against a
   wall and tangentially along it. Compare native accepted motion with predicted
   carry; do not interpret the physical plan as injected movement.
4. Recenter and use an ordinary save/load transition if convenient. History must
   reset without a fake large delta; body changes are not assigned an unproven cause.
5. Capture several periodic summaries and existing body telemetry, then stop/
   deactivate through the normal flow. Confirm expected native tick rate, valid
   owner state, no added tick and zero positional translation.

Do not enable room-scale, change speed, or start crouch/jump/camera tuning for
this capture. A physical injection capability requires a separate demonstrated
boundary before any later active positional test.

## Final review snapshot

`git diff --stat`:

```text
 ARCHITECTURE.md                                    |  23 +-
 CHANGELOG.md                                       |   3 +
 CMakeLists.txt                                     |  13 +-
 ROADMAP.md                                         |   3 +-
 docs/BLACK_PLAGUE_SPATIAL_NOTES.md                 |  16 +
 docs/REWORK_PORTING_PLAN.md                        |  12 +-
 docs/internal/CODEX_HANDOFF.md                     |  61 ++--
 docs/internal/DEBUG_HANDOFF.md                     |  26 +-
 docs/internal/TRACKING_BODY_RECONCILIATION.md      | 337 +++++++++++++++++++++
 .../black_plague/black_plague_body_adapter.cpp     |  90 +++++-
 .../black_plague/black_plague_body_adapter.hpp     |  17 +-
 src/backends/black_plague/body_collision_probe.cpp |   3 +-
 .../black_plague/body_reconciliation_shadow.cpp    |  91 ++++++
 .../black_plague/body_reconciliation_shadow.hpp    |  41 +++
 src/backends/black_plague/render_world_probe.cpp   |  14 +
 src/backends/overture/overture_backend.cpp         |  59 ++--
 src/probe/black_plague_probe.cpp                   |  25 ++
 src/runtime/vr_locomotion.cpp                      |  64 ++++
 src/runtime/vr_locomotion.hpp                      |  31 ++
 .../black_plague_body_adapter_test.cpp             |   2 +-
 .../body_collision_probe_test.cpp                  |  56 ++++
 .../native_movement_boundary_stub.cpp              |   9 +-
 tests/overture_backend/reconciliation_trace.cpp    |  61 ++++
 .../vr_tracking_space/body_reconciliation_test.cpp | 121 ++++++++
 24 files changed, 1070 insertions(+), 108 deletions(-)
```

`git status --short --branch`:

```text
## main...origin/main
 M ARCHITECTURE.md
 M CHANGELOG.md
 M CMakeLists.txt
 M ROADMAP.md
 M docs/BLACK_PLAGUE_SPATIAL_NOTES.md
 M docs/REWORK_PORTING_PLAN.md
 M docs/internal/CODEX_HANDOFF.md
 M docs/internal/DEBUG_HANDOFF.md
 A docs/internal/TRACKING_BODY_RECONCILIATION.md
 M src/backends/black_plague/black_plague_body_adapter.cpp
 M src/backends/black_plague/black_plague_body_adapter.hpp
 M src/backends/black_plague/body_collision_probe.cpp
 A src/backends/black_plague/body_reconciliation_shadow.cpp
 A src/backends/black_plague/body_reconciliation_shadow.hpp
 M src/backends/black_plague/render_world_probe.cpp
 M src/backends/overture/overture_backend.cpp
 M src/probe/black_plague_probe.cpp
 M src/runtime/vr_locomotion.cpp
 M src/runtime/vr_locomotion.hpp
 M tests/black_plague_body/black_plague_body_adapter_test.cpp
 M tests/black_plague_body/body_collision_probe_test.cpp
 M tests/black_plague_body/native_movement_boundary_stub.cpp
 A tests/overture_backend/reconciliation_trace.cpp
 A tests/vr_tracking_space/body_reconciliation_test.cpp
```
