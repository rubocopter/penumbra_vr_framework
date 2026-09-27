# Trilogy parity plan

This is the capability ledger for proving that Penumbra VR is a framework rather
than three unrelated ports. Rework `23c890f` is the Overture behavioral
reference; Black Plague is the second-consumer proof; Requiem is the active
third consumer with its own validation gates.

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

## Framework-readiness gate before Requiem gameplay (passed for Hito 2)

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

These remaining Black Plague items are release/regression gates. They did not
block the start of Requiem gameplay integration:

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

The detailed current work order is in `ROADMAP.md`; the repeatable hardware
procedure is in `VR_HEADSET_TEST_CHECKLIST.md`.

## Requiem integration state (2026-09-27)

The allowlisted Steam executable has its own initialized-image manifest and
backend. Requiem consumes the common OpenVR session, stereo presentation,
tracking, input, locomotion, crouch, accepted physical body motion, hand
visuals, free-body grab/contact math, snap continuity and spatial UI through
Requiem-specific renderer, player, physics and input boundaries. These are
implemented/host-tested capabilities, not a blanket runtime support claim.

Headset reports confirm boot/menu to gameplay, stereo head tracking without
the former head-only culling gap, basic Sense-left motion, a usable gameplay
monitor mirror, room-scale motion mixed with stick, wall collision, physical
crouch, visible hands and accessible inventory/notebook. Jump felt better
after the Requiem-local adjustment. Tool alignment can still drift during stick
motion; lateral speed feels different without a measurement. Some sessions
showed a late cadence drop of unknown ownership.

**Current blocker: headset validation of physical interaction parity.** RQ-13
reported difficult stone/note pickup and an apparent forward stone impulse when
stepping back. Its log had nearby VR ray candidates, including mass-26 entities,
but zero unconditional native Grab/Move entries, so that impulse did not come
from the VR hold path. Exact-image inspection then mapped native accepted-pick
ownership and the Requiem palm collision ABI.

The current host-tested batch ports Black Plague's stronger reusable hand
ownership pattern into the Requiem backend. A backend-owned palm box is resolved
against the exact Requiem physics world on the existing character-body update
owner; palm overlap is attempted before ranked ray fallback while preserving
native eligibility. Grab and Move exclude the held body before a synchronous
fresh palm generation, use the resolved palm and gameplay yaw epoch while held,
and clear ownership on failed acquisition/leave. Move also reuses a fresh
selected surface point instead of forcing a centre anchor. Release compilation
and **44/44** host tests pass for this batch. It remains unvalidated in the
headset, so no support claim follows from it.

The inventory-item magnetic acquisition boundary remains unported. Rework and
BP apply magnetic range only to items, with subtype-specific reach and hand/head
solid sight, never to stones, doors or arbitrary mechanisms. Requiem
item-list/bounds/subtype evidence must be completed before consuming that
policy.

Requiem is **not supported**. See `CLOSURE_STATUS.md` for the single current
manual gate and `SUPPORTED_BUILDS.md` for validation terms.

## Requiem implementation contract

The current integration follows this contract:

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
