# Trilogy parity plan

This is the capability ledger for proving that Penumbra VR is one framework
rather than three unrelated ports. Rework `23c890f` is the proven Overture
behavioral reference. Black Plague is the second-consumer proof and Requiem is
the active third consumer.

## What counts as ported

A capability counts as ported only when:

1. the proven reference behavior is identified;
2. reusable policy/data is shared where reuse is real;
3. the target backend consumes it, or documents a technically incompatible
   native equivalent; and
4. the target reaches its own required validation level.

Extraction alone is not parity. Validation uses the common ladder:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

## Capability ledger

| Capability family | Shared/framework state | Black Plague | Requiem | Remaining evidence/work |
| --- | --- | --- | --- | --- |
| Stereo/compositor/tracking | Shared OpenVR session, view and tracking policy | Consumed; substantial headset evidence | Consumed; boot/menu/gameplay stereo and head tracking exercised | Current-candidate presentation/focus/transition regression per game |
| Locomotion/body/crouch | Shared metric locomotion, accepted-motion, play-mode and crouch policy | Consumed through native body boundary | Consumed through Requiem-owned body/input boundary | BP comfort regression; Requiem representative progression/pacing |
| Recenter/turn/yaw | Shared yaw/settings/input policy and yaw epoch | Consumed | Consumed; held Grab body remained attached through one headset snap turn | Transition/tracking-loss and BP held-object continuity regression |
| Controller actions/bindings | 42 actions, 6 sets, 8 bindings | Consumed; freshness guard host-tested | Sense gameplay/UI actions consumed | Hardware/per-device regression before support claims |
| Hands/fingers | Shared pose/conditioning; target profiles own mesh/socket data; attached-tool curl follows Rework's fixed radius pose | BP keeps richer skeletal channels; latest held-tool correction host-tested | Resolved-palm tool/hand motion headset-validated in flashlight/glowstick/flare clip; installed-asset flashlight socket correction host-tested | BP finger/material pass; Requiem static finger/light alignment headset check |
| Palm/contact | Shared resolver/contact policy | Collision-resolved palm and held-body ownership consumed | Requiem-owned exact-build palm adapter consumes the same proven lifecycle | Headset contact stability and turn continuity |
| Free-body Grab/Move/throw | Shared pose, anchor and throw policy | Fresh bounded surface-contact Grab ported from Requiem path, host-tested; stale/native picks retain origin fallback | Representative `Grab=6` acquisition, carry, snap turn, changed contact and release headset-validated; free-body `Move=2` and throw lack headset evidence | BP surface regrab regression, Requiem throwable prop and distinct Move path when encountered |
| Native mechanisms / Push | Shared math only where the native mechanism proves compatible | Slider/hinge/Object adapters keep native lifecycle ownership | Representative Push cube and jointed `Move=2` monolith puzzle headset-tested; monolith rings showed some resistance/springback | Requiem mechanism comfort investigation; BP mechanism regression |
| Tracked UI | Shared panel/input policy | Inventory/notebook/context/UseItem routes host-tested on latest candidate | Menu, inventory and notebook visible; shared spatial UI consumed | Headset layout/action regression, subtitles/legibility |
| VR settings/config | Shared schema/editor policy plus per-game data | Native settings consumer implemented | No validated persisted Requiem VR block yet | Headset settings regression; Requiem user-facing profile later |
| Haptics/audio | Shared event, HRTF and reverb policy | Haptics/HRTF/reverb consumers implemented | Only applicable shared startup/runtime pieces consumed so far | Audio-device validation; map safe BP low-pass boundary |
| Enhanced visuals/effects | Shared final eye-stage calibration where proven | BP final stage remains disabled; exact-build full-eye refraction port is host-tested with separate particle/copy telemetry | Requiem's exact-build full-eye refraction copy removed the portal's large rectangular/striped artifact in headset with Refractions enabled | Subtle motion ghosting remains unclassified; BP refractive particles need target visor evidence, and ordinary smoke is a separate family |
| Deployment/package ownership | Shared deployment/settings/localization manifests | Existing shared-redist transaction extended for exact Requiem probe/localization; direct fixture host-tested | Runtime backend has headset evidence; shared deployment transaction host-tested only | Combined deterministic package/selector and GUI discovery/control creation host-tested; final user acceptance pending |

## Black Plague release-regression state

Black Plague is the established second backend and has real headset evidence,
including gameplay VR from Steam's ordinary **Play** path. It is not supported
yet. Current-candidate regression still covers presentation/focus, mixed
physical motion, crouch recovery, hands, free-body/mechanism interaction, UI,
audio, visual effects, transitions and shutdown. Exact work order belongs in
`ROADMAP.md`; the hardware procedure belongs in
`VR_HEADSET_TEST_CHECKLIST.md`.

The SDL mutex crash remains historical and unattributed. Existing dumps do not
justify assigning a cause. Collect a new discriminating dump/log only if it
reappears during normal testing.

## Requiem integration state

Requiem has its own allowlisted executable, initialized-image manifest and
backend. It consumes the common OpenVR session, stereo presentation, tracking,
input, locomotion, crouch, accepted physical motion, hands, free-body
interaction math, yaw continuity and spatial UI through Requiem-owned renderer,
player, physics and input boundaries.

Headset testing has reached boot, menu and gameplay; basic Sense locomotion,
room-scale motion, wall collision, crouch, visible hands and inventory/notebook
have been exercised. The interaction work then mapped native pick acceptance,
ported the stronger resolved-palm ownership lifecycle from Black Plague and
added a Requiem-owned Push adapter where puzzle cubes use native `Push=1`
instead of Grab/Move.

The latest Push acquisition correction accounts for Requiem publishing the new
player-state index only after `Push::Enter` returns. In the subsequent headset
session the cube was pushed to its switch, tipped and used to solve the puzzle;
telemetry confirmed VR Push acquisition and applied hand force. That sequence is
headset-validated and does not promote unrelated Grab/Move or mechanism paths.

In the next headset session, a small stone block entered native `Grab=6` after
VR selection and was acquired/released repeatedly with fresh surface contact.
The player carried it with stick movement, made one snap turn while holding it,
changed the grip point and released it without a visible defect. The matching
level-01 stone-block asset declares `CanBeThrown=False`; throwing, free-body
`Move=2` have no equivalent headset proof from this small-block sequence.
Requiem's successful surface-contact grip has been ported into BP's fresh,
bounded VR-origin path while preserving its stale/native origin fallback.
The BP port is host-tested and still needs its own headset evidence.

The next headset pass completed level 01 and loaded the following area. Its
monolith used native `Move=2` for the jointed rings; telemetry recorded 20
native entries, 20 VR acquisitions and 20 releases, and the authored puzzle
advanced. The rings felt somewhat resistant and could spring back. The level
script also clamps their joint limits at puzzle positions, so the VR contribution
to that feel remains unresolved. The user could accidentally displace stacked
blocks while climbing them. In the matching log interval, blocks entered native
`Grab=6` and jumps occurred, but VR Push acquisition and force stayed at zero;
the displacement cannot be attributed to the VR Push force path. Gameplay
pacing returned to roughly 90 FPS after loading, but the portal showed
rectangular/striped visual artifacts in the recording. The native `portal-fx`
warning refers to untextured map control geometry, while the visible portal
uses separate refractive materials. The HPL log records a 2560×1440 screen
buffer; the VR log records 3400×3468 eye targets. Exact Requiem code calls
`glCopyTexSubImage2D` through its native screen-copy path, while the shipped
refraction shader samples `screenMap` in viewport pixel coordinates. This size
mismatch was implicated by a headset comparison: the rectangular/striped defect
appeared with Refractions enabled and disappeared when disabled, while the portal
ring remained visible. A small motion-related ghosting impression remained with
Refractions disabled. Requiem's exact-build copy now captures the whole bound VR
eye into HPL's rectangle screen texture at the two verified renderer copy sites.
With Refractions enabled, the subsequent headset clip showed the authored portal
ring without the large rectangular/striped defect. Runtime telemetry recorded
one texture resize followed by full-eye copies at every intercepted native
refraction call, with roughly 90 FPS in the portal scene; HPL exited normally.
Subtle motion ghosting remains unclassified. An earlier
flashlight/glowstick/flare visor trial showed the native tools moving relative
to the visible hand during stick locomotion. Telemetry confirmed pre-visibility
attachment refresh ran, so timing alone did not explain the separation.
Rework fixes attached-tool finger curl to the grip-radius pose; Framework's
shared articulation now does the same, but the subsequent visor trial showed
no perceptible alignment improvement. The remaining Requiem-specific difference
was positional: the visible hand consumed the collision-resolved palm while
the tool refresh consumed the raw grip. Black Plague's tool path already uses
the resolved palm. Requiem now uses one palm publication for tool and hand at
native update, visibility and eye rendering, with an old-yaw fallback. A new
headset clip confirms that the three tools remain steady relative to the hand
under stick locomotion; its log recorded resolved-palm use and no raw-palm use.
Slight finger intersection and a static flashlight effect gap remain. The
installed flashlight DAE shares BP's -Y spotlight/ray axis, while Requiem had
used Rework's -90-degree socket for its modified asset. The Requiem adapter now
uses a target-owned socket matched to the installed DAE; this last change is
host-tested only.

**Current gate:** validate static flashlight/finger alignment and classify the remaining interaction
rough edges when the corresponding native object is available. Magnetic
acquisition remains limited to the proven Rework/BP inventory-item policy and
must not be enabled in Requiem until its exact body-list, bounds and item-subtype
boundaries are demonstrated.

Requiem is **not supported**. See `ROADMAP.md` for the current work order and
`SUPPORTED_BUILDS.md` for build identity and validation terms.

## Requiem implementation contract

1. Fingerprint and research Requiem independently.
2. Map renderer, player/body, input, UI, interaction and audio boundaries from
   its own executable evidence.
3. Consume existing shared policy first.
4. Keep Requiem layouts, signatures, model values and gameplay state in its own
   backend/profile.
5. Add a shared abstraction only after another real consumer proves the
   boundary.
6. Validate Requiem independently while preserving Overture and Black Plague
   regression gates.
