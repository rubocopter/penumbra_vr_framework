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
| Hands/fingers | Shared pose/conditioning; target profiles own mesh/socket data | BP keeps richer skeletal channels | Visible tracked hands consumed | BP finger/material pass; Requiem tool alignment |
| Palm/contact | Shared resolver/contact policy | Collision-resolved palm and held-body ownership consumed | Requiem-owned exact-build palm adapter consumes the same proven lifecycle | Headset contact stability and turn continuity |
| Free-body Grab/Move/throw | Shared pose, anchor and throw policy | Consumed through BP Grab/Move boundaries; latest changes host-tested | Representative `Grab=6` acquisition, carry, snap turn, changed contact and release headset-validated; free-body `Move=2` and throw lack headset evidence | BP regression, Requiem throwable prop and distinct Move path when encountered |
| Native mechanisms / Push | Shared math only where the native mechanism proves compatible | Slider/hinge/Object adapters keep native lifecycle ownership | Representative Push cube and jointed `Move=2` monolith puzzle headset-tested; monolith rings showed some resistance/springback | Requiem mechanism comfort investigation; BP mechanism regression |
| Tracked UI | Shared panel/input policy | Inventory/notebook/context/UseItem routes host-tested on latest candidate | Menu, inventory and notebook visible; shared spatial UI consumed | Headset layout/action regression, subtitles/legibility |
| VR settings/config | Shared schema/editor policy plus per-game data | Native settings consumer implemented | No validated persisted Requiem VR block yet | Headset settings regression; Requiem user-facing profile later |
| Haptics/audio | Shared event, HRTF and reverb policy | Haptics/HRTF/reverb consumers implemented | Only applicable shared startup/runtime pieces consumed so far | Audio-device validation; map safe BP low-pass boundary |
| Enhanced visuals/effects | Shared final eye-stage calibration where proven | BP final stage exists but remains disabled pending target lighting parity; particle telemetry exists | Refraction-on/off headset comparison isolated the portal's large artifact; exact-build full-eye copy candidate is host-tested only | Validate Requiem candidate and residual ghosting before assessing BP's distinct particle scenes |
| Deployment/package ownership | Shared deployment/settings/localization manifests | Development deploy and two-game candidate consume them | Development backend only; production transaction absent | Unified three-game transactional installer |

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
`Move=2` and native mechanisms have no equivalent headset proof. Requiem's
successful surface-contact grip is a useful comparison for BP's currently
host-tested fixed-origin grip. BP needs its own headset evidence before changing
that adapter.

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
mismatch is implicated by the next headset recording: the rectangular/striped
defect appeared with Refractions enabled and disappeared when disabled, while
the portal ring remained visible. A small motion-related ghosting impression
remained with Refractions disabled. A Requiem-only candidate now captures the
whole bound VR eye into HPL's rectangle screen texture at the two exact
renderer copy sites; the exact image, GL operation and fail-closed gate are
host-tested. Its visual result and pacing still need visor evidence. The
transition/progression was exercised, and HPL logged a successful exit after
the comparison on the prior candidate. Portal presentation and tool alignment
remain open on the new one.

**Current gate:** validate the exact-build Requiem refraction candidate while
keeping the interaction rough edges visible. Magnetic acquisition remains limited to
the proven Rework/BP inventory-item policy and must not be enabled in Requiem
until its exact body-list, bounds and item-subtype boundaries are demonstrated.

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
