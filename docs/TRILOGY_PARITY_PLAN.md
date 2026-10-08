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

The maintainer accepted the requested BP/Requiem feedback retest on 2026-10-08.
The [scoped acceptance record](releases/1.0.6-feedback-acceptance.json) pins the
preceding installed binaries: BP vice/locker acquisition, first-level settings
and height calibration, contact mini-hop correction, and Requiem intro height,
crouch, fullscreen UI, slope stability and persisted controls. Overture was not
included in that retest. The new shared native-menu extraction, Overture/Requiem
first-level routes and BP mixed stick/HMD obstacle correction are host-tested;
they do not inherit headset acceptance from those preceding binaries.

| Capability family | Shared/framework state | Black Plague | Requiem | Remaining evidence/work |
| --- | --- | --- | --- | --- |
| Stereo/compositor/tracking | Shared OpenVR session, view and tracking policy | Consumed; substantial headset evidence | Consumed; boot/menu/gameplay stereo and head tracking exercised | Current-release presentation/focus/transition regression per game |
| Locomotion/body/crouch | Shared metric locomotion, accepted-motion, play-mode and crouch policy | Consumed through native body boundary | Consumed through Requiem-owned body/input boundary | BP comfort regression; Requiem representative progression/pacing |
| Recenter/turn/yaw | Shared yaw/settings/input policy and yaw epoch | Consumed | Consumed; held Grab body remained attached through one headset snap turn | Transition/tracking-loss and BP held-object continuity regression |
| Controller actions/bindings | 42 actions, 6 sets, 8 graphs; all three consumers and package/install/repair distribution host-verified | Consumed; freshness guard host-tested | Sense gameplay/UI actions consumed | Device hardware regression; Vive compatibility and WMR holster omissions remain |
| Hands/fingers | Shared pose/conditioning; target profiles own mesh/socket data; attached-tool curl follows Rework's fixed radius pose | BP keeps richer skeletal channels; latest held-tool correction host-tested | Tool/hand motion and final native-size flashlight beam/model alignment headset-validated | BP finger/material pass; slight finger/contact imperfections remain |
| Palm/contact | Shared resolver/contact policy | Collision-resolved palm and held-body ownership consumed | Requiem-owned exact-build palm adapter consumes the same proven lifecycle | Headset contact stability and turn continuity |
| Free-body Grab/Move/throw | Shared pose, anchor and throw policy | Surface-contact Grab port has headset evidence from acquiring a chair at different points; stale/native picks retain origin fallback | Representative `Grab=6` acquisition, carry, snap turn, changed contact and release headset-validated; free-body `Move=2` and throw lack headset evidence | BP held turn/throw/mechanism regression; Requiem throwable prop and distinct Move path when encountered |
| Native mechanisms / Push | Shared math only where the native mechanism proves compatible | Slider/hinge/Object adapters keep native lifecycle ownership; native `Push=1` tracked-palm adapter host-tested for the tutorial heavy crate; native Move Wheel hinge and dynamic-over-frame acquisition headset-accepted on the feedback build | Representative Push cube and jointed `Move=2` monolith puzzle headset-tested; monolith rings showed some resistance/springback | Broader BP mechanism regression; Requiem mechanism comfort investigation |
| Tracked UI | Shared panel/input policy | Inventory/notebook/context/UseItem routes host-tested on latest candidate | Menu, inventory and notebook visible; shared spatial UI consumed | Headset layout/action regression, subtitles/legibility |
| VR settings/config | Shared schema/editor policy plus per-game data | First-level native settings and height calibration headset-accepted; shared extraction host-tested | Persisted turn/crouch and play-mode/height consumed; nine-row native first-level VR page, live refresh and calibration host-tested | New Overture/Requiem menu routes and shared extraction headset confirmation; broader profile coverage |
| Haptics/audio | Shared event, HRTF and reverb policy | Haptics/HRTF/reverb consumers implemented | Only applicable shared startup/runtime pieces consumed so far | Audio-device validation; map safe BP low-pass boundary |
| Enhanced visuals/effects | Shared final eye-stage calibration where proven | BP final stage remains disabled; observed special effects now render correctly in headset with native full-eye copy/resize telemetry | Requiem's exact-build full-eye refraction copy removed the portal's large rectangular/striped artifact in headset with Refractions enabled | Coverage of all smoke/material variants and subtle ghosting remains unestablished |
| Deployment/package ownership | Shared deployment/settings/localization manifests | Existing shared-redist transaction extended for exact Requiem probe/localization; direct fixture host-tested | Runtime backend has headset evidence; shared deployment transaction host-tested only | v1.0.0 package/selector/GUI host-tested with maintainer real-install, uninstall and game-entry acceptance; independent clean-PC breadth remains open |

Overture's post-v1.0 physical-interaction correction has maintainer headset
acceptance on the exact 1.0.1 installer: the maintainer played beyond the initial
drawer, repeated crank/mechanism interactions and accepted the result. The
installed executable matches the candidate build hash. This validates the
observed acquisition/gameplay sequence, including visibility-before-ranking;
it does not certify every Overture mechanism or Quest 3 pointer alignment.

## Controller profiles and distribution

All three products consume the same 42 logical actions and six action sets,
including the mirrored dominant-hand layouts. The bundled defaults are:

| Family | SteamVR controller type | Bundled action coverage |
| --- | --- | --- |
| PS VR2 Sense | `playstation_vr2_sense` | All 42 actions |
| HTC Vive | `vive_controller` | Compatibility layout: no turn, button crouch, holster, pause or skeletons |
| Valve Index | `knuckles` | All 42 actions |
| Oculus Touch | `oculus_touch` | 41 actions; recenter deliberately unassigned |
| Pico 4 | `pico4_controller` | 41 actions; recenter deliberately unassigned |
| Pico Neo 3 | `pico_neo3_controller` | 41 actions; recenter deliberately unassigned |
| Windows Mixed Reality | `microsoft/motion_controller` | No holster or skeletons |
| Holographic/WMR | `holographic_controller` | No holster or skeletons |

Coverage here means declared binding outputs, not hardware validation. Sense
has game-specific headset evidence; the other families still require physical
device validation. Vive's omitted actions matter to the binary backends, which
do not supply a raw-input fallback. Missing skeletons use the existing hand-pose
fallback. SteamVR permits custom remapping.

The inherited Index component-path correction is host-tested in the generator
and both payload roots, including movement, turn, sprint/crouch clicks and UI
drag for both handedness layouts. The external Index tester later reports Black
Plague and Requiem working as intended after the v1.0.3 fixes. Overture now
crashes immediately before the main menu, and the Black Plague tutorial heavy
crate's native Push interaction is unreliable in VR. These are scoped reports
without exact installed-build hashes and do not establish full controller or
game acceptance.

The offline follow-up host-tests independent Overture analog fallback when
only move or turn is inactive, preserving an active neutral custom binding and
excluding legacy stick-press bits. Black Plague's renderer reproduced dropping
the calibrated height when a yaw epoch changes before the next body tick; it now
keeps the fresh same-origin feet/world anchor and height, while suppressing old
horizontal prediction. Origin discontinuities, invalid/stale body samples and
excessive physical deltas still reject placement. This is a host-tested camera
continuity correction. The subsequent external retest reports Black Plague
working as intended, closing that reported turning-height symptom for the tested
path without promoting broader hardware support.

Index defaults now separate offhand interaction from quick light, use explicit
force clicks for grip/trackpad and put pause on the spare right trackpad. The
pointing sources use SteamVR's declared tip poses. Touch/Pico no longer bind
recenter to a duplicate/nonexistent face-button click; WMR's mirrored pause no
longer also recenters. The generated [binding reference](CONTROLLER_BINDINGS.md)
and Black Plague's shortcut to SteamVR's active binding editor are host-tested.
Overture's v1.0.3 shortcut is removed in v1.0.4 after the pre-menu crash report;
it uses the SteamVR dashboard editor again. New mapping/pointer/dashboard
headset acceptance remains pending. Vive/WMR
compatibility exceptions are retained and documented. These source changes
follow the published v1.0.2 and do not retroactively change that artifact.

The unreleased offline follow-up aligns BP's native Push axis-force dispatch
with HMD-relative body intent using the projection already consumed by Requiem.
BP-owned captured axes and callback slots are exact-image verified; actual
dispatch, native rejection and non-Push behavior are host-tested. The representative tutorial crate received local headset acceptance; broader
Push coverage remains separate. Overture's actual action/legacy
consumer is now host-tested for immediate focus release, held-button recovery
and fresh grip fallback after auxiliary aim loss. These changes do not identify
or validate a fix for the reported pre-menu crash.

The BP vice model is `Type="Wheel"`, and `level01_cells.hps` reads its native
angle and owns its `SetWheelCallback` progression. The exact BP 2.2.0.0
`cGameWheel` constructor (RVA `0x5A340`) sets entity type `0x13`. The palm
target filter admits that type. The latest physical hinge consumer also accepts
Wheel after its proven native `Move=2` transition; the earlier consumer rejected
it even after targeting succeeded. Fixed frames with active dynamic siblings
no longer win palm/ray selection for the vice or locker. Native acceptance,
wheel physics, joint limits and level callbacks remain authoritative. The probe
build and focused host regression tests pass. Wheel acquisition and actual
rotation received maintainer headset acceptance on the feedback candidate;
broader mechanism coverage remains separate.

The next unreleased feedback pass keeps Overture's logic cadence aligned to the
active HMD display frequency reported by OpenVR, with the proven 90 Hz behavior
as fallback when that property is unavailable. Its shared visibility pass now
uses the union of both asymmetric eye projections before exact per-eye rendering,
targeting the reported left-eye edge disappearance without widening either
rendered eye. Both policies are host-tested and the Overture Release build passes
its existing VR regression suite; Quest 3/Link 72 Hz headset confirmation is
still pending. Black Plague also exposes the Overture-style height calibration
action through its native VR Settings page. The subsequent PSVR2 report
confirmed calibration still failed: full-screen menu presentation invalidates
world tracking every frame. The follow-up caches raw HMD tracking height from
both world and menu poses independently of the world anchor, accepts only a
finite sample no older than 500 ms and clears it on session restart. Values are
still normalized through the shared settings schema and persistence errors roll
back. Build and settings-policy tests pass; the maintainer accepted the subsequent
feedback calibration retest.
The native settings injector now binds from the exact BP 2.2.0.0
`cMainMenu::SetState` boundary (RVA `0x7A680`). Its ten verified direct
callsites cover both front-end and in-game menu routes. VR Settings now lives
in the first-level Start state `0`, without entering Options. Injection runs
after native state activation and rebinds when the owner changes or its widget
list is rebuilt. A recycled widget address is rejected unless its live button
vtable and injected-root sentinel still match; the address-reuse case fails
before the identity check and passes after it in host tests.
The maintainer accepted the requested first-level main-menu/in-game settings
and persisted-height feedback retest. This is scoped to the preceding BP build.

The accepted feedback build also filters physical step candidates by Rework's
positive/upward/static eligibility at the native ray-result boundary. The new
mixed-input correction lets the native stick update consider dynamic obstacles
when bounded joystick intent was actually injected, while retaining the
physical positive/upward guards. A host regression reproduced the former
dynamic-obstacle rejection; the new excessive-blocking report remains a device
confirmation gate for this changed combination.

The maintainer subsequently accepted startup/basic use of the locally installed
`1c3f834` Overture build, with no apparent issues; this is scoped acceptance,
not an attribution of the earlier external startup crash. An earlier local BP
tutorial run reported stalls, body blocking and drops during freezes/turns; a
later PSVR2 retest grabbed the small tutorial crate repeatedly without issue, so
the external small-crate reach report remains unreproduced locally. The earlier
matching log contains 280–470 ms presentation gaps,
stale palm samples and guarded Grab releases. Production presentation success
counts triggered per-frame matrix/body logs, with synchronous disk durability
flushes on the presentation thread. The follow-up reduces healthy telemetry
to periodic samples and moves durability flushes to close. Actual-adapter host
regressions also preserve Grab on a long tick and defer world-pose writes while
only the presentation reference is expired, retaining held-body collision
exclusion. Release/focus/tracking/disconnection guards remain covered. The
complete cause of the observed stalls and the updated tutorial behavior still
require a headset retest; unheld crate collision has not been retuned.

The maintainer subsequently completed the BP tutorial on local `30a1e78`
without performance problems. The matching log has no 250–1000 ms gameplay
presentation gaps or guarded Grab releases. This closes the reported pacing/
freeze-drop symptom for that tested route, not general collision comfort.
The maintainer clarified with a headset clip that the bottle shelf launches
when using the combined tutorial chemical. The installed game's
`maps/level00_tutorial.hps` registers `chemC` on `shelf01` to `UseItem`, which
explicitly calls `AddBodyForce("shelf01","world",-20000,10000,0)`, flashes,
and hides the shelf after 2.5 seconds before starting the sneak tutorial.
The clip shows that sequence; this shelf launch is native scripted gameplay,
not evidence of a VR hand-contact regression. Preserve the native callback.

Separately, BP's hand-nudge vector multiplied
bounded delta velocity by mass before a verified unchanged forward to
`NewtonAddBodyImpulse`. The installed Newton DLL matches the tested product
DLL exactly; real-DLL tests reproduce the excess speed and now pass at 1, 3,
20 and 100 kg with the mass multiplier removed. That correction is host-tested.
The log also records an approximately 8 mm requested horizontal step resolving
to 21 cm through native collision; its cause and updated hand-contact feel
remain open headset gates. No solver clamp or body-size retuning was applied.

The finger mismatch is unresolved and the reporter considers it non-blocking.
Current consumers preserve OpenVR's Thumb/Index/Middle/Ring/Pinky channel order and map
it to the named rig chains; host conditioning/articulation/render tests pass
but do not establish physical sensor-to-visible-finger identity. No channel
reversal has been applied without discriminating hardware evidence.

Oculus Touch aim output now uses SteamVR's `pose/tip` source in both Overture
and the shared binding tree. The generated-binding check and host regression
cover this mapping; Quest 3 pointer alignment still requires a physical-device
retest before the change can advance beyond host-tested.

`products/overture/scripts/generate-bindings.ps1` owns the physical layouts;
`assets/openvr/overture` supplies Overture and `assets/openvr` supplies the
shared Black Plague/Requiem root. Metadata checks enforce functional parity
between those trees, the eight controller types, output coverage and direct
pose/skeleton/haptic types. Each product loads executable/loader-adjacent
`vr/actions.json`; SteamVR selects defaults by controller type. Installation
repairs the owned default files without replacing saved SteamVR custom bindings.

Package/install/repair fixtures verify the manifest and all eight graphs by
source hash for Overture and the shared root, including Requiem selection and
repair of deleted/corrupted graphs. This is host-tested distribution evidence;
it does not establish device compatibility or public support.

## Black Plague release-regression state

Black Plague is the established second backend and has real headset evidence,
including gameplay VR from Steam's ordinary **Play** path. Publication in
v1.0.0 does not promote every capability to `supported`. Current-release
regression still covers presentation/focus, mixed
physical motion, crouch recovery, hands, free-body/mechanism interaction, UI,
audio, visual effects, transitions and shutdown. Exact work order belongs in
`ROADMAP.md`; the hardware procedure belongs in
`VR_HEADSET_TEST_CHECKLIST.md`.

The observed special-effects scene and chair surface-contact acquisition have
headset evidence. The physical-contact mini-hop follow-up was accepted by the
maintainer on the feedback candidate. The exact native step-climb flag at
`cCharacterBody+0x238`, candidate height and hit normal/static telemetry remain
available for scoped regression of the new mixed-input obstacle eligibility.
Broader wall/tunnel comfort and native movement states remain separate gates.

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
The BP port now has its own headset evidence from grabbing a chair at different
contact points; that report does not validate BP held snap turns or throwing.

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
Subtle motion ghosting remains unclassified.

Requiem's tool/hand motion defect came from using raw grip for the tool while
the visible hand consumed the collision-resolved palm. Pre-visibility refresh
and Rework's fixed grip-radius curl alone did not resolve that difference.
Requiem now uses one palm publication for tool and hand at
native update, visibility and eye rendering, with an old-yaw fallback. A new
headset clip confirms that the three tools remain steady relative to the hand
under stick locomotion; its log recorded resolved-palm use and no raw-palm use.
The target-owned flashlight socket preserves forward orientation at native
size. HPL's axis billboard normalizes its rendered basis and keeps its installed
height while its centre inherits scale, so scaling the parent separates the beam.
Requiem now keeps the validated orientation with a rigid native-size attachment
and 0.020 grip radius. Host tests protect the billboard near edge/emitter gap,
direction and fixed grip under wrist rotation. The user accepted the combined
result and supplied a headset image showing the beam at the housing edge.
Earlier slight finger intersection remains a known limitation unless new
headset evidence establishes otherwise.

The unreleased feedback candidate also addresses three Requiem reports without
changing its exact-build ownership boundaries. Native pointer activity now
classifies otherwise-unidentified fullscreen UI so death/continue screens can
use the shared tracked pointer path. Post-character-update publication refreshes
the final native body Y before render-time height composition, matching Rework's
post-physics vertical anchoring and targeting the reported uphill slope shake.
The probe now loads the shared persisted VR settings profile and applies its
turn mode/angles/speed/dead zone and crouch mode/depth instead of hard-coded
backend defaults. The maintainer accepted these requested cases on the preceding feedback build.
Requiem now also consumes the shared native VR page through independently mapped
constructor, widget-list, SetState and vtable boundaries; both binary menus have
host fixtures for startup injection, state return, widget recreation, languages,
calibration, settings persistence failure and partial-allocation cleanup.

The representative Requiem interaction milestone is concluded at the user's
request after the final flashlight validation. Broader
interaction gaps and known limitations remain visible for regression. Magnetic
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
