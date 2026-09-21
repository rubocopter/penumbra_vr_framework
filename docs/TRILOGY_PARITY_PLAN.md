# Trilogy parity plan

This is the capability ledger for proving that Penumbra VR is a framework rather
than three unrelated ports. Rework `23c890f` is the Overture behavioral
reference; Black Plague is the second-consumer proof; Requiem is the planned
third consumer.

## What counts as ported

A reusable capability is complete for a target only when all applicable steps
are true:

1. the proven reference behavior is identified;
2. game-neutral policy/data is shared where reuse is real;
3. the target backend consumes that shared behavior, or documents a technically
   incompatible native equivalent;
4. the target reaches the required validation level independently.

Extraction alone is not parity. Shared-but-unused code remains open work.
Validation uses the common ladder:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

## Framework-readiness gate before Requiem gameplay

Requiem gameplay becomes the active milestone when each demonstrated Overture
runtime/gameplay capability is one of:

- consumed by Black Plague;
- implemented through a documented Black Plague-specific equivalent;
- classified Overture-only/not applicable with evidence; or
- deliberately deferred as release tooling rather than runtime parity.

Per-controller physical validation and the finished unified installer are
release gates rather than prerequisites for beginning Requiem gameplay, but the
shared binding graph and available-hardware backend paths must remain green.

## Capability ledger

| Capability | Shared state | Black Plague current state | Remaining parity work |
| --- | --- | --- | --- |
| Stereo/compositor lifecycle | Shared session/stereo policy | Consumed; substantial live/headset evidence | Revalidate current per-loop pose acquisition/focus edges |
| Head rotation/camera composition | Shared transforms/camera policy | Consumed and headset exercised | Preserve through transition/focus tests |
| Positional room-scale | Shared tracking/reconciliation policy | Consumed; projection/clamp plus injected-step direction/static eligibility host-tested after the mixed-contact headset regression | Headset revalidate wall/tunnel and low-prop mixed-motion contacts |
| Walk/sprint locomotion | Shared `1.5/2.25 m/s` policy | Consumed through native body boundary | Focused production-path speed/direction regression |
| Crouch/tracked Y | Shared play-mode/crouch policy | Consumed with headset evidence | Hybrid release-hold and blocked stand |
| Recenter/turn/world yaw | Shared yaw/settings/input policy | Consumed | Revalidate turn-hand continuity, map transitions and tracking loss |
| Accepted-motion footsteps | Shared cadence policy | Consumed/host-tested through native footstep call | Headset surface/cadence and bob comfort observation |
| Controller actions/bindings | 42 actions, 6 sets, 8 bindings | Consumed | Per-device hardware reports before support claims |
| Hand pose/finger conditioning | Shared conditioning/articulation semantics | Consumed; BP keeps richer skeletal channels | Revalidate all five fingers on imported mesh |
| Palm collision | Shared resolver/contact policy | Production consumer host-tested; earlier partial headset evidence | Wall/table/locker slide and turn continuity |
| Direct hand nudge | Shared bounded policy | Consumed with target classification | Revalidate dynamic-prop behavior |
| Free-body grab/throw | Shared pose/throw policy | Consumed through BP `Grab=6` and free `Move=2`; current-sample held-palm refresh/held-body exclusion is host-tested, including same-sample resolver reuse; because BP routes ordinary props such as barrels through `Move=2`, that path adapts Rework's palm-contact/throw behavior while mechanisms retain native offsets | Headset-confirm reliable acquisition and stable held placement around multiple nearby contacts |
| Jointed mechanisms | Shared servo/contact math where applicable | Verified slider/hinge/Object adapters; unknown families fail closed | Headset validate representative mechanisms |
| Tool/light presentation | Shared behavior plus profile data | Authored held-tool pose and BP profiles consumed | Final geometry/socket headset validation |
| Tracked menu/UI | Shared panel/input policy | Inventory/notebook, death/full-screen pointer routes and native HUD capture mapped; Rework-equivalent inventory drag/default/context-action routing plus the `UseItem=4` controller-aim ray and native red/green usability beam are host-tested through exact native boundaries | Headset usability/progression, item-use targeting and subtitle validation |
| Native VR settings | Shared schema/editor policy | Native Options-page consumer host-tested | Headset navigation/persistence pass |
| Haptics | Shared event policy | Pickup/drop/UI/contact plus mapped light/damage/melee consumers | Controller/headset validation |
| HRTF | Shared startup config | Consumed/host-tested | Audio-device/headset validation |
| Environmental reverb | Shared preset/bus policy | Exact-build EFX consumer host-tested | Audio-device/headset validation |
| Distance/occlusion low-pass | Shared Rework behavior identified | No safe BP consumer yet | Map target boundary and consume |
| Enhanced Visuals | Shared eye-stage calibration | Transferable Rework v4 final eye stage consumed/host-tested; headset use exposed an overly dark/saturated Black Plague result | Map the Black Plague pre-tone material/ambient/light response before retuning the shared final curve; keep renderer-specific behavior backend-owned |
| Map-start heading | Shared yaw ownership semantics | BP spawn-yaw compensation host-tested | Headset transition regression |
| Camera-facing particles | Rework per-eye behavior identified | BP per-eye refresh adapter host-tested | Headset validate representative effects |
| Body lifecycle safety | Shared generation/lifecycle concepts | BP exact native active/death guard host-tested | Preserve through death/menu/map transitions |
| Recommended configuration | Shared schema + per-game data | Overture/BP profiles present; Requiem game profile prepared | Installer consumption later |
| Deployment payload ownership | Shared manifest contract started | BP development deploy consumes it | Unified transactional installer |

## Current Black Plague blockers

The framework-readiness gate is primarily blocked by focused validation and a
few missing consumers, not by the absence of a VR foundation:

- presentation pacing/focus regression on the current candidate;
- physical wall/tunnel/mixed-motion comfort after the latest reconciliation;
- reliable palm acquisition/held placement and representative mechanisms;
- final imported-hand finger/tool validation, including correcting the current
  fullbright-looking hand presentation: the imported Overture hand is authored
  as a non-emissive lit material, while the Framework compatibility draw path
  currently presents its diffuse texture without the original lighting response;
- UI/settings/HUD headset validation;
- HRTF/reverb headset validation and the missing distance/occlusion consumer;
- Enhanced Visuals parity beyond the final eye pass: the Framework uses the
  accepted Overture Rework v4 final calibration, but Black Plague currently
  reaches it without the corresponding Rework material/ambient/light
  preconditioning. Headset evidence therefore does not justify changing the
  shared exposure/saturation/contrast/gamma constants in isolation;
- representative chapter progression after the visual pipeline is corrected.

The detailed current work order is in `../ROADMAP.md`; the repeatable hardware
procedure is in `VR_HEADSET_TEST_CHECKLIST.md`.

## Requiem implementation contract

When the gate closes:

1. fingerprint and research Requiem independently;
2. map renderer, player/body, input, UI, interaction and audio boundaries from
   its own executable evidence;
3. consume existing shared policy first;
4. keep Requiem layouts, signatures, model values and gameplay state in its own
   backend/profile;
5. add a new shared abstraction only after a real second/third consumer proves
   the boundary;
6. validate Requiem independently while preserving Overture and Black Plague
   regression gates.
