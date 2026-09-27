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
| Render-thread diagnostics | Framework invariant: optional diagnostics must not block VR presentation; Overture's legacy synchronous per-draw `PrintLog` is disabled while VR is enabled | Render-world telemetry uses non-blocking publication and reports dropped diagnostic updates | Require the same rule for Requiem instrumentation as it is added |
| Stereo/compositor lifecycle | Shared session/stereo policy | Consumed; substantial live/headset evidence | Revalidate current per-loop pose acquisition/focus edges |
| Head rotation/camera composition | Shared transforms/camera policy | Consumed and headset exercised; the eye-derived native camera position is published with each eye's matrices. The latest messhall clip still shows light-ray artifacts; a new candidate avoids composing the tracked head a second time when `UpdateRenderList` runs inside an already-overridden eye pass | Headset revalidate messhall billboards plus transition/focus tests; classify any remaining artifact in the exact render path |
| Positional room-scale | Shared tracking/reconciliation policy | Consumed; projection/clamp plus injected-step direction/static eligibility host-tested after the mixed-contact headset regression | Headset revalidate wall/tunnel and low-prop mixed-motion contacts |
| Walk/sprint locomotion | Shared `1.5/2.25 m/s` policy | Consumed through native body boundary | Focused production-path speed/direction regression |
| Crouch/tracked Y | Shared play-mode/crouch policy | Consumed with headset evidence; delayed native blocked-stand feedback and the Hybrid physical/button release sequence now pass the Black Plague host contract | Headset revalidate Hybrid release-hold and blocked stand under a low ceiling |
| Recenter/turn/world yaw | Shared yaw/settings/input policy | Consumed | Revalidate turn-hand continuity, map transitions and tracking loss |
| Accepted-motion footsteps | Shared cadence policy | Consumed/host-tested through native footstep call | Headset surface/cadence and bob comfort observation |
| Controller actions/bindings | 42 actions, 6 sets, 8 bindings | Consumed; a host-tested freshness guard releases locomotion and interaction after 1.5 s of bit-identical moving axes and both tracked grip poses. The latest headset log shows a nonzero move axis and both finger-curl summaries frozen for 33 s, but did not record the grip matrices needed to prove this guard would have fired. The next probe logs moving-sample age and guard activation | Headset verify frozen-input recovery and per-device hardware reports before support claims |
| Hand pose/finger conditioning | Shared conditioning/articulation semantics | Consumed; BP keeps richer skeletal channels | Revalidate all five fingers on imported mesh |
| Palm collision | Shared resolver/contact policy | Production consumer host-tested; earlier partial headset evidence | Wall/table/locker slide and turn continuity |
| Direct hand nudge | Shared bounded policy | Consumed with target classification | Revalidate dynamic-prop behavior |
| Free-body grab/throw | Shared pose/throw policy | Consumed through BP `Grab=6` and free `Move=2`; held-palm refresh/held-body exclusion, negative Newton ray filtering and Rework's active/enabled/non-autodisable lifecycle are host-tested. Failed VR-origin acquisitions exit instead of falling through to native mouse manipulation. After the long headset clip exposed difficult pickup, the BP-specific `Grab=6` socket now anchors the body origin consistently for a fresh VR winner within 0.40 m; native-only picks keep the tighter contact guard. The published world-yaw epoch preserves free-body holds through snap turns without a one-frame Move force impulse; mapped mechanisms keep their native joint ownership. These latest changes are host-tested only | Headset-confirm repeated acquisition, held placement and snap-turn continuity for Grab, free Move and mechanisms |
| Jointed mechanisms | Shared servo/contact math where applicable | Verified slider/hinge/Object adapters; unknown families fail closed. A fresh VR-selected Move contact now uses the same 0.40 m reach as Grab after native Move=2 authorizes the target; the strict palm box still guards native-only/stale contacts. Host-tested only | Headset validate lever, doors, drawers and whether manipulation follows native joint limits without repelling the player |
| Tool/light presentation | Shared behavior plus profile data | Authored held-tool pose and BP profiles consumed | Final geometry/socket headset validation |
| Tracked menu/UI | Shared panel/input policy | Inventory drag/default/context routing and `UseItem=4` controller-aim beam are host-tested through BP's native boundaries. The right-button edge bypasses only BP's top-level discard. The native 800x600 queue is captured over the stereo world: inventory uses Rework's fixed panel; notebook follows the off hand with Rework's downward panel pitch; the first closing frame retains its previous panel transform to avoid a full-screen flash. Gameplay messages use Rework's centered 1/750 m plane at UI distance; `SubtitleScale` enlarges the captured surface. Host-tested only for these latest changes | Headset confirm context text, inventory/notebook spatial placement, closing transition, item-use targeting and subtitle legibility |
| Native VR settings | Shared schema/editor policy | Native Options-page consumer host-tested; 17 consumed Rework rows remain after hiding Black Plague's unvalidated Enhanced Visuals switch | Headset navigation/persistence pass |
| Haptics | Shared event policy | Pickup/drop/UI/contact plus mapped light/damage/melee consumers | Controller/headset validation |
| HRTF | Shared startup config | Consumed/host-tested | Audio-device/headset validation |
| Environmental reverb | Shared preset/bus policy | Exact-build EFX consumer host-tested | Audio-device/headset validation |
| Distance/occlusion low-pass | Shared Rework behavior identified | No safe BP consumer yet. The canonical on-disk executable has transformed code at an independently validated audio callsite (`0x15C26A`): its file bytes are not the `E8` call required by the live exact-build hook, so static disassembly cannot establish the sound-channel boundary | Map the loaded target boundary before adding a filter hook; consume shared policy and validate on audio hardware |
| Enhanced Visuals | Shared eye-stage calibration | Rework v4 final eye stage is implemented, but headset use exposed an overly dark/saturated Black Plague result; BP now disables that stage and hides its setting while preserving the saved preference | Map the Black Plague pre-tone material/ambient/light response before re-exposing the switch; keep renderer-specific HPL behavior backend-owned |
| Map-start heading | Shared yaw ownership semantics | BP spawn-yaw compensation host-tested | Headset transition regression |
| Camera-facing particles | Rework per-eye behavior identified | BP per-eye refresh adapter host-tested; live telemetry now reports native particle updates, successful eye refreshes and refresh misses so representative smoke/vapor can prove whether it traverses this path | Headset validate representative effects and use the counters to classify any remaining volumetric artifact before changing another render family |
| Body lifecycle safety | Shared generation/lifecycle concepts | BP exact native active/death guard host-tested. The 2026-09-24 death-to-main-menu dump identified native crouch OnEnter dereferencing a null character body; the bridge now clears VR stance ownership and suppresses native state transitions while the body is absent. Host-tested only | Headset retest death → main menu and map transitions |
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

## Requiem integration state (2026-09-26)

The canonical Requiem image has its own verified static `RenderWorld`,
`UpdateRenderList`, `DrawAll`, native body update/move and player movement
boundaries in the Requiem manifest. The shared ALUT path now dispatches by
exact host hash to a Requiem probe. That probe consumes common OpenVR session,
camera/eye math, HPL camera adapter, GL eye targets, scissor mapping and stereo
render policy for an initial rotational stereo path. RQ-01 confirmed ALUT/probe
startup, a submitted stereo world and a scene visible in PSVR2. Turning the
head exposed missing geometry until native mouse yaw changed: the native
render-list pass still used the game camera before the VR eye pass. The proven
Black Plague HMD visibility/frustum boundary is now adapted to Requiem's own
exact callsite and reuses one compositor pose for visibility and both eyes.
RQ-02 confirmed on PSVR2 that head-only turning no longer exposes missing
geometry, so this specific visibility path is headset-validated. Requiem's
pre-gameplay UI does not traverse `RenderWorld`; the existing SDL swap callback
now captures that 2D framebuffer into the shared `OpenGlMenuFrame`, holds a
stable tracked panel anchor, and submits it only when no stereo world was
submitted in the current frame. A Requiem-local frame gate prevents duplicate
compositor submission. The full Release build and all 43 host tests pass; the
menu path passed RQ-03 on PSVR2: boot, menu navigation and the transition into
gameplay can be completed with the headset on, and the 2D panel hands off to the
stereo world. This presentation path is now headset-validated. Exact Requiem
gameplay evidence now maps `ButtonHandler::Update`, player access, both native
move callsites/state gates and the physical body owner. A Requiem gameplay
bridge consumes shared controller actions and HMD-relative locomotion policy,
preserves native keyboard/controller priority, injects only X/Z at the native
physical owner and leaves Y/gravity/jump native-owned. The exact-build
contract is host-tested. RQ-04 exercised basic Sense movement in the running
game; precise HMD heading, each axis and long-term stability remain
unverified. This remains **partial live integration**, not gameplay parity.

RQ-04 confirmed basic Sense-left movement in the headset and a clean gameplay
mirror on the monitor. Exact HMD-versus-body heading and individual axes were
not isolated by that report. Requiem's own `ChangeMoveState` entry (`0x9CBA0`),
move-state index (`+0x2D4`) and state 4 crouch handler (`0xAF364`) were verified
offline in the initialized executable. The shared Hybrid crouch policy now
drives the native state on the game thread with a null-body guard and retains
native keyboard ownership. RQ-05 observed repeated native crouch/stand
transitions without a crash or blocked stand, but physical crouch exposed a
native camera-height jump. Requiem's rotation-only world view had retained
native camera Y; the backend now reads body Y at its existing physical owner
and consumes shared `VrTrackingSpace` for HMD height and button-only posture
offset, as BP does. RQ-06 showed this first vertical correction still dropped
the eye too far: it had used the native capsule **centre** as the tracking
feet anchor. Rework `23c890f` anchors to `GetFeetPosition()`. The exact
Requiem image proves active capsule height at `iCharacterBody+0xC8` via
`GetSize`/`SetActiveSize`; the backend now uses centre minus half that height.
RQ-07 reports physical crouch now appeared correct in the headset; the stick
speed impression remains, and its final log block fell to 55.6 world FPS with
5.9 ms mean RenderStereo time. The exact Requiem physics-world callsite now
brackets native body Update for accepted X/Z motion, while the existing
physical owner injects combined room-scale and stick requests. Shared Rework
reconciliation and filtered render prediction are consumed through Requiem's
own boundary. Eleven exact native gameplay queries now receive Sense actions
after executing their original queries. These additions are host/static-tested
and awaited RQ-08; button interaction does not prove hand grab or spatial
selection. RQ-08 subsequently validated mixed room-scale/stick movement,
physical crouch, wall response and flat floor objects in the headset; it also
exposed an invisible R1 inventory that stopped stick movement, no observed R2
interaction, and a short-feeling jump. The exact Requiem inventory activation
changes native input mode to 5 while the earlier compositor submit omitted the
later 2D draw. The next host-tested probe defers that submit to desktop capture
for active inventory/notebook, maps 59 exact native queries and five menu
pointer calls, requests recenter, and draws controller-anchored Rework hand
meshes. RQ-09 must validate these paths; visible hands do not prove spatial
selection or grabbing. Right-stick turning and the perceived jump height remain
unresolved. The next log records mean native button-update time and Sense
button counters. Three startup dumps show
the same SDL mutex fault signature as BP, with no established cause.

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
