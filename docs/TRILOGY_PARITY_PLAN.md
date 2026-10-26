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

On 2026-10-10 the maintainer accepted the newly installed trilogy checklist.
`releases/1.0.7-headset-acceptance.json` pins the exact local runtime hashes and
closes the new main/pause menu, BP mixed-obstacle and input/focus recovery gates.
Overture cadence/edge visibility/reach and Requiem menu/calibration also have
scoped acceptance. Quest 3/Link 72 Hz and optional Requiem Move/throw cases were
conditional and have no separately identified evidence in that confirmation.

| Capability family | Shared/framework state | Black Plague | Requiem | Remaining evidence/work |
| --- | --- | --- | --- | --- |
| Stereo/compositor/tracking | Shared OpenVR session, view and tracking policy | Consumed; substantial headset evidence | Consumed; boot/menu/gameplay stereo and head tracking exercised | Current-release presentation/focus/transition regression per game |
| Locomotion/body/crouch | Shared metric locomotion, accepted-motion, play-mode and crouch policy | Consumed through native body boundary | Consumed through Requiem-owned body/input boundary | BP comfort regression; Requiem representative progression/pacing |
| Recenter/turn/yaw | Shared yaw/settings/input policy and yaw epoch | Consumed; held-turn continuity accepted on local checklist | Consumed; held Grab body remained attached through one headset snap turn | Broader transitions/tracking-loss regression |
| Controller actions/bindings | 42 actions, 6 sets, 8 graphs; all three consumers and package/install/repair distribution host-verified | Consumed; freshness/release guards host-tested; input/focus recovery accepted | Shared release/dead-zone hardening consumed; recovery accepted | Other-device hardware regression; Vive compatibility and WMR holster omissions remain |
| Hands/fingers | Shared pose/conditioning; target profiles own mesh/socket data; attached-tool curl follows Rework's fixed radius pose | BP keeps richer skeletal channels; latest held-tool correction host-tested | Tool/hand motion and final native-size flashlight beam/model alignment headset-validated | BP finger/material pass; slight finger/contact imperfections remain |
| Palm/contact | Shared resolver/contact policy | Collision-resolved palm and held-body ownership consumed | Requiem-owned exact-build palm adapter consumes the same proven lifecycle | Headset contact stability and turn continuity |
| Free-body Grab/Move/throw | Shared pose, anchor and throw policy | Surface grabs, hold/turn, release and throw accepted on installed local checklist; stale/native picks retain origin fallback | Representative `Grab=6` acquisition, carry, snap turn, changed contact and release headset-validated; free-body `Move=2` and throw lack separately identified headset evidence | Broader BP shapes/mechanisms; Requiem throwable prop and distinct Move path when encountered |
| Native mechanisms / Push | Shared math only where the native mechanism proves compatible | Slider/hinge/Object adapters keep native lifecycle ownership; native `Push=1` tracked-palm adapter host-tested for the tutorial heavy crate; native Move Wheel hinge and dynamic-over-frame acquisition headset-accepted on the feedback build | Representative Push cube and jointed `Move=2` monolith puzzle headset-tested; monolith rings showed some resistance/springback | Broader BP mechanism regression; Requiem mechanism comfort investigation |
| Tracked UI | Shared panel/input policy | Inventory/notebook/context/UseItem accepted on installed local checklist | Menu, inventory and notebook visible; shared spatial UI consumed | Headset layout/action regression, subtitles/legibility |
| VR settings/config | Shared schema/editor policy plus per-game data | First-level settings, shared extraction and calibration accepted on installed local build | First-level native page, live edits, persistence and calibration accepted on installed local build | Broader device/profile coverage; exact new Setup acceptance separate |
| Haptics/audio | Shared event, HRTF/reverb and tracked-listener pose policy | Haptics/HRTF/reverb implemented; new head-listener consumer host-tested | New head-listener consumer host-tested through native OpenAL imports | Per-game head-rotation/audio-device validation; map safe BP low-pass boundary |
| Enhanced visuals/effects | Shared final eye-stage calibration where proven | BP final stage remains disabled; observed special effects now render correctly in headset with native full-eye copy/resize telemetry | Requiem's exact-build full-eye refraction copy removed the portal's large rectangular/striped artifact in headset with Refractions enabled | Coverage of all smoke/material variants and subtle ghosting remains unestablished |
| Deployment/package ownership | Shared deployment/settings/localization manifests | Existing shared-redist transaction extended for exact Requiem probe/localization; direct fixture host-tested | Runtime backend has headset evidence; shared deployment transaction host-tested only | v1.0.0 package/selector/GUI host-tested with maintainer real-install, uninstall and game-entry acceptance; independent clean-PC breadth remains open |

Overture's post-v1.0 physical-interaction correction has maintainer headset
acceptance on the exact 1.0.1 installer: the maintainer played beyond the initial
drawer, repeated crank/mechanism interactions and accepted the result. The
installed executable matches the candidate build hash. This validates the
observed acquisition/gameplay sequence, including visibility-before-ranking;
it does not certify every Overture mechanism or Quest 3 pointer alignment.

## Input and lifecycle audit corrections — v1.0.11

Overture's actual source consumers have host regressions for stable interaction
hand ownership, individual binding releases, a shared UI pointer/button hand and
current-pose recenter in menu/pause containers. The reference/difference contract
is recorded in `REWORK_PORTING_PLAN.md`.

Requiem's physical hand-ray ranking now retains native occlusion independently
per ray: a foreground solid blocks its ray without discarding an unobstructed
finger ray. Both hit orders are covered by actual-consumer tests. Control-thread
shutdown retains the native Leave owners until pending/committed Grab, Move and
Push interactions drain on the game thread. Palm-shape destruction likewise
requires its native owner; timeout or destroy failure retains a retryable partial
state and the servicing gameplay hooks. Menu/player-less update service and
explicit reinstallation have host regressions. Turn reset is requested atomically
and consumed only by the native update owner.

These corrections ship in v1.0.11 and are implemented and host-tested. They do not transfer the
previously installed artifacts' live/headset acceptance to this candidate or
close the progression, water, aiming, contact or startup-crash gates below.

## Requiem progression feedback — v1.0.10 host evidence

Caelus reports on Quest 3 that walking/jumping now work correctly; audio seems
correct but was only briefly exercised. The exact installed version/hash was
not supplied, so this is scoped user feedback, not artifact-pinned acceptance.
The new report identifies a level03 explosive ketchup-bottle throw blocker and
hard-to-find Examine/ladder cues. It does not close ascent/cabinet gates.

Requiem now consumes Rework's independent head Examine query through its own
mapped native callback; physical hand selection remains separate. Native
solid/area arbitration, occlusion, Examine distances/description-once policy
and Ladder's own admission/activation guards remain authoritative. Examine
refreshes its head target at action time; distant described props cannot become
remote VR grabs. The Normal Update/ray owners preserve the native callback.

Native Grab Examine (default Touch B in the right-handed layout) originally
unprojected the camera/cursor after leaving Grab. Its direction now uses the
current aim of the hand holding the body, with grip fallback, captured before
Leave clears ownership. Native CanBeThrown, restored mass, impulse profile,
central/point application and state/script lifecycle remain unchanged. Gesture
release keeps Rework's tracked velocity gain/caps; invalid focus, UI, stale
reference or mismatched tracking epoch produces no release momentum. The
unused never-fed velocity-history fallback was removed.

Actual `render_world` consumer tests reproduce the previous selection/aim
failures and cover independent head/hand targets, native range/once policy,
occlusion, head rotation, ladder guards, action refresh and hook restoration.
Throw tests execute production hooks with characterized native boundaries and
the shipped Newton DLL, preserving native transition/reset/impulse ordering,
mass profiles, CanBeThrown and off-center impulse behavior. Reaching the
level03 trigger and perceived aiming/cue usability remain headset gates.
Exact mappings and evidence windows are in Requiem's manifest.

### Requiem level04/05 progression feedback — 2026-10-25

Caelus reports that crouching in level04 sewer water can leave the player
walking on the bottom, unable to rise by standing or jumping; level05
generator/hatch/door script buttons have very narrow or missing interaction
targets, even when a cue appears; filing-cabinet sliders resist opening.
This is reported Quest 3 behavior without a pinned installed artifact/hash.

The v1.0.11 Requiem adapter keeps its native `A5000` clear,
`A5D50` solid/area resolve, original ray eligibility/occlusion and native
action callbacks when selecting level05 script areas. The hand-ray selection
also checks native script areas before an overlapping physical palm contact
can short-circuit the query. Host consumer tests exercise foreground solids,
nearer/farther areas and the overlap-priority decision. Level05 callbacks
in the installed `level05.hps` target `genbutton`, `hatchbutton` and
`doorbutton`. Their numeric Requiem entity subtype is still unverified;
the synthetic fixture models callback area routing explicitly.

For Requiem filing cabinets the host candidate temporarily suspends the
native slider's `+E8/+EC` sticky endpoint distances only while a VR-owned
`Move=2` child of a static parent is held, then restores unchanged zeroed
values on release if the original joint is still linked. Native limits,
constraint ownership and script overrides remain authoritative. Slider
host tests cover ownership, dynamic parents, override, stale joints and
restoration before native `Move::Leave` can modify its joint links.

The water issue remains an **unresolved investigation**, not a fixed feature.
Rework's liquid force depends on the submerged character-body cylinder and
its current capsule height. Requiem's `9CBA0` move-state switch writes the
requested state index before calling `9AE00` for the transition; checking
the index alone does not establish that the physics capsule has expanded or
that buoyancy is sufficient. The level04 script changes water height and
configures `water01` through `SetupLiquidArea`. Neither a vertical VR impulse
nor altered gravity is justified by existing evidence. Level04 rise/stand,
level05 repeated button activation and drawer endpoints remain independent
headset validation gates for v1.0.11. The existing native
crouch-transition log now also samples the active capsule height (`capsule_y`,
exact-build `character_size_y_offset`) to distinguish a changed state index
from an actual physical resize during that focused run.

## Previous feedback corrections — v1.0.9 host evidence

Framework v1.0.9 publishes corrections for the concrete Quest 3 report;
these changes have no new live/headset acceptance. Historical accepted binaries
and their scoped results remain unchanged.

- Normal metric walk/run policy is shared at `3/4.5 m/s`. Binary-host stick
  requests no longer inherit the physical HMD request's 5 cm cap; room-scale
  translation retains that bound. Requiem owns both independently mapped
  held-jump queries, including the final native release check. Consumer tests
  cover VR-only hold/release and motion at different update cadences, including
  mixed physical/stick requests and collision rejection.
- Normal jump targets the independently decoded BP/Requiem 60 Hz profile:
  authored launch `350`, mass `80`, custom gravity `-18` and `0.3 s` held input.
  Overture replaces Rework's `1.7` launch/`-10` gravity defaults for this normal
  profile and samples the native vertical envelope through forces at its HMD
  cadence. Extracted production-state tests cover tap/held ascent at
  30/60/72/90/120 Hz, crouch, release and native script/collision handoff. The
  Overture input owner combines keyboard and VR before publishing the held
  counter once; a later keyboard release cannot erase VR hold. Production
  input-chain tests cover either/both devices and cancellation. A fresh native
  Jump press while airborne resets the held counter without relaunching; the
  sampler preserves that native renewal and its test executes the real setter
  and Jump method. The binary hosts retain their native vertical pipeline. These are host boundary
  tests; actual ceiling/landing contacts and the reported long-jump platform
  remain independent headset gates.
- Both binary hosts consume Rework's tracked head-listener convention through
  their own verified named OpenAL imports. A real OpenAL Soft null-device test
  exercises physical-only head rotation, position, recenter, native overwrites,
  fallback, context switching/recreation and restoration. It does not establish
  perceived direction, headset HRTF quality or every listener-relative sound.
- The independently mapped native step-success blocks advanced by climb speed
  times timestep even when the measured rise was smaller. A CPU execution
  regression reproduces 5.17 cm of ascent for a 1 cm surface at 60 Hz. The shared
  bounded advance and exact-build adapters limit VR ascent to that measured rise,
  recovered from the surviving ray-height/distance data and accepted ray index;
  the old height local is already reused by native shape construction. This keeps
  native collision/step selection and one physics update. Host coverage
  includes shallow/tall rises, nonpositive hits, cadence, native fallback and
  instruction restoration. The reported Requiem uphill surfaces and BP cave/snow
  surfaces still need independent headset reproduction.
- BP's filing-cabinet joints declare sticky endpoint distances absent from the
  inspected desks. The independently mapped native callback zeros drawer
  velocity in those regions. The committed VR `Move=2` consumer suspends only
  the held child's endpoint rest distances and restores them on release,
  preserving native joints, limits and nonzero script changes. A real Newton
  consumer regression reproduces the stalled cabinet before correction and
  covers desks, masses, both stops, obstruction/recovery and cancellation.

Exact-build data and evidence provenance belong in each game's manifest.
The new runtime candidate does not inherit support or headset acceptance from
v1.0.8; the reported long platform jump, ascent comfort and cabinet gesture
remain live/headset gates.

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

The v1.0.6 follow-up aligns BP's native Push axis-force dispatch
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

Framework v1.0.6 keeps Overture's logic cadence aligned to the
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
dynamic-obstacle rejection; the maintainer accepted the mixed-obstacle checklist for this changed
combination on 2026-10-10. Broader collision comfort remains separate.

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

### Dream-ship progression feedback

An external report identifies a progression blocker in the dream ship reached
after the sewers: the yellow bucket under a table and the dead animal required
on the hand altar could not be grabbed. The same report describes mouse-only
message advancement on arrival, rare Examine cues and an unusable kitchen save
point near the computer. Quest 3 is reported; the mod is presumed current but
its installed version, controller binding and exact save are not yet verified.
Prior tutorial/chair acceptance does not close these cases.

The inactive Message input exception is implemented and host-tested. BP's
native handler accepts Interact/Examine in state `5` even when the player is
inactive; the previous Framework UI gate instead removed those VR actions and
selected full-screen presentation. The adapter now preserves that native route
and world-rendering eligibility while real overlays retain priority. VR turn,
crouch and stick ownership still require an active player; queued physical and
stick displacement is rejected at the existing body injection boundary when
native gameplay becomes inactive. Both actual-adapter regressions failed before
their fixes and pass afterwards; the initialized-image input verifier passes.
The installed feedback candidate received maintainer headset acceptance for
controller arrival-message dismissal and the kitchen save point on 2026-10-15.
Examine cues worked consistently in the tested saves on that setup; Quest 3
coverage remains separate. The scene script confirms inactive Message ownership.

The code-only follow-up corrects three independently reproduced adapter defects:
inert Object contacts no longer mask a usable palm candidate; physical Object
distance consumers use a fresh matching contact within the existing 0.40 m
reach instead of measuring from the native body centre; and a separate tracked
head query restores Examine selection/cues and nonphysical area/SaveArea
fallback. The scoped headset observations above do not validate every branch.

Head selection retains native solid/area arbitration and occlusion. Explicit
Examine refreshes the gaze target while Interact retains the hand target;
described props cannot become distant gaze grabs. SaveArea is independently
mapped as type `10`, with a non-colliding body and its native 2.1 m save guard.
Its callback point and native distance are preserved. Physical distance changes
are limited to four Object calls and the verified crosshair distance caller;
native Examine, SaveArea, UseItem and generic interaction-script distance
consumers retain their original policy. Stale targets, changed points, UI/focus
loss and out-of-reach contacts fall back to native distance.

The reporter saves reproduced the remaining yellow-bucket failure in visor:
chest opening and egg acquisition worked, but the bucket could
only be pushed with the egg. `boat_bucket01.ent` defines native Grab and two
dynamic bodies (3 kg bucket, 2.2 kg handle); its DAE links them with an internal
hinge. Framework's blanket joint rejection differed from Rework's jointed Grab.
The new host-tested adaptation accepts only entirely dynamic, same-Object body
assemblies with all joint endpoints internal. Native Grab must already own the
transition; world anchors, foreign bodies and mechanism entity types are rejected.
Rework's 4/6 body speed caps and 3 m/s carried velocity apply to these assemblies;
release remains bounded to 3 m/s and restores native saved settings. Newton owns
the joint solver and only the selected body receives the tracked transform.
The maintainer's next headset test accepted bucket acquisition by both handle
and container. The supplied 58-second clip shows lifting/rotation in the egg
room. This closes the reported ungrabbable-bucket case on that local setup;
The following maintainer test also accepted throwing and reacquiring the bucket;
broader assembly regression remains separate.
The later dog puzzle was not exercised; egg placement must not be recorded as
dog acceptance. Placing the egg on the hand pedestal produced no event; earlier
test instructions incorrectly equated that pedestal with `stickegg`. Map/save
evidence places `stickegg` on the `mr_table` table near the coffin, while
`stickdog` is at `altar01` (`mr_hand_altar`), almost 2.8 m away horizontally.
The egg belongs on the table's egg picture. Native egg attachment triggers a
spider encounter and enables the dog after the subsequent fire sequence; dog
Hips/Spine2 attachment then belongs at the hand pedestal's hook.
The maintainer then confirmed correct-table egg attachment, the fire sequence
and dog appearance. Dog acquisition still failed. Its native Grab model
`character_dog.ent` has 14 dynamic ragdoll bones, while the save enumerates 28
bodies alternating those bones and mass-zero animation colliders. Exact Rework
`23c890f` creates these companions inactive, non-character-colliding and separate
from the ragdoll joint graph; native entity reactivation can wake all enumerated
bodies. BP's assembly guard previously rejected every zero-mass sibling.
The host-tested correction ignores only unjointed, non-character-colliding
mass-zero companions of the same Object; they are not valid joint endpoints
and their native activation is never changed by Grab. Character-blocking static
bodies and world/foreign anchors remain rejected.
The actual adapter regression failed before this change and passes afterwards;
a 28-body ragdoll fixture covers the graph, and both inactive and reactivated
companion fixtures preserve native lifecycle state.
The following headset report acquired the dog but found limited anatomical
acquisition and extreme skeletal stretching. The dog altar was not tested:
the maintainer tried the wall instead. The altar event is untested, not a
confirmed failure.

The articulated Grab candidate is host-tested. Closed dynamic components of
at least two physical bodies now use Newton contact/angular drive instead
of assigning one bone's matrix; the native joint solver retains the assembly.
Human corpses previously failed the 32-entry enumeration guard before their
animation companions could be filtered. A regression reproduces that rejection
at 18+18 entries and passes with separate enumeration/dynamic-graph bounds.

The installed base-game asset audit examined 759 `.ent` files and 551 referenced
DAEs. All 15 explicit Grab assets with multiple creatable joints divide into
these groups (counts are authored topology, not live observations):

| Asset stem under `models/` | Physical + animation bodies | Joints | Grab candidate route |
| --- | --- | --- | --- |
| `characters/dog/character_dog` | 14 + 14 | 13 ball | Yes |
| `corpse/corpse_gas_victim_ragdoll`, `corpse/corpse_howard_ragdoll` | 18 + 18 each | 17 each | Yes; old enumeration limit rejected both |
| `corpse/corpse_worm` | 4 + 4 | 3 ball | Yes |
| `cleaning/cleaning_trashbag` | 3 + 3 | 2 hinge | Yes |
| `furniture/furniture_madrass` | 6 + 6 | 7 mixed | Yes |
| `furniture/furniture_pillow01` | 6 + 6 | 6 ball | Yes |
| `kitchen/kitchen_foodbag_flour`, `_rice`, `_sugar` | 3 + 3 each | 2 ball each | Yes |
| `lab/lab_cryo_head` | 3 + 3 | 2 mixed | Constrained contact: static neck endpoint |
| `mental_realm/mr_hand_ragdoll` | 3 + 3 | 3 hinge | Constrained contact: world endpoint |
| `tech/tech_electric_box_cord`, `_static` | 8 + 8 each | 7 ball each | Constrained contact: static endpoints |
| `boat/boat_dynamicparaffin` | 2 + 2 | 2 ball | Constrained contact: world endpoint; one extra declared joint has no child body |

The one-joint `boat_bucket01` now uses the same Newton contact/component drive.
Its earlier acquisition/throw acceptance did not cover strong handle pulls;
the later headset report identifies handle/container separation. The old
selected-body matrix exception is removed and an actual-adapter regression
rejects matrix writes for this two-body assembly. Shipped-Newton hinge fixtures
lift from either member using authored bucket/handle masses; live retest remains
open for that correction.
Coffin/chest variants and lockers use native Move with fixed boxes/frames or
world hinges; their owners and constraints are preserved. Anchored native Grab
props above use the constrained contact route, selected by the live graph rather
than asset name; authored topology does not establish live/headset validation.
The single-joint `kitchen/kitchen_rotten_meat` (one 35 kg body, world ball joint)
and `lab/lab_s_hatch_physics` (9 kg lid, static frame, 0–0.40 m slider) expose
the same native Grab boundary. They are eligible for the constrained route too.
The hatch's native opened/closed controllers remain game-owned; no headset
coverage or controller compatibility is inferred from the multi-joint audit.

Host regressions exercise every physical member of 2/3/4/6/14/18-body graphs,
and endpoint/interior palm selection through native acceptance and prepared
Grab. Active auxiliary colliders, disabled bones and native callback vetoes
retain their respective selection policies. Real Newton tests pull 2/3/7/14/18/24
body chains by both ends and the middle for 360 ticks, with native-like selected
mass reduction; pivot separation remains within the 4 cm test bound. These
synthetic chains do not prove stability for every authored joint limit/contact.

The subsequent headset report describes only about half a centimetre of dog
motion, as if its weight were infinite. The earlier chain fixture omitted
gravity. Enabling native-like gravity on the non-selected bodies reproduces
insufficient lift: Grab lightens only the selected bone, so its bounded contact
impulse cannot carry the rest. The host-tested correction distributes the same
bounded translation delta through the selected connected dynamic component,
preserving each body's existing velocity and native gravity/joints. Animation
companions and disconnected dynamic bodies remain untouched. Gravity-bearing
chain lift and actual-adapter isolation regressions pass. The next maintainer
headset report confirms lifting the dog and placing it on the altar. It also
reports irregular initial acquisition that recovered after opening/closing the
menu; that recovery's cause remains unclassified.

The same report identifies the following raised-arm puzzle as blocked and the
flashlight attachment as incorrect at 00:49 in the supplied clip. Asset/script
inspection establishes the arm as native Grab with a world-rooted three-hinge
chain; the closed-assembly guard rejected its VR acquisition. A separate
constrained contact route now preserves the native graph, script locking and
limits, applying bounded corrections only to the connected dynamic members.
It does not write static/animation transforms, override wrist angular velocity,
or throw the anchored object. Initial palm/contact separation is preserved so
stationary acquisition does not pull the mechanism. Actual-adapter world/static-root acquisition,
release and isolation tests pass. Shipped-Newton three-hinge fixtures exercise
both anchor types and every selected bone with gravity and native-like mass
reduction: lift reaches within 8 cm of the target, pivots stay within 4 cm and
joint limits within the test's 0.05 rad tolerance. These synthetic fixtures do
not prove the authored puzzle event. The flashlight's target-owned socket now
matches Rework's grip-relative forward direction, preserving the measured
grip and native size; independent node tests pass for both hands and wrist
rotation. The next maintainer headset report confirms moving the articulated
arms to complete the puzzle on installed probe `111F7D…0520`. That scoped
progression gate is accepted. Flashlight confirmation remains separate.

The same session reports barrel slingshot release and a wall lever that moves
but never starts the next puzzle. Saved `3barrels` Objects use native Push;
the original release handler adds a camera-forward impulse after native Leave.
VR-origin release and cancellation now use the transition-only behavior proven
in Rework. Tests reproduce and remove that late impulse without erasing existing
body velocity, and preserve native-origin release semantics.

The `3barrellever` is native Lever/Move with a 2 kg offset-collider body.
Its native update owns the 110-degree maximum state and the `BarrelLever`
callback that enables puzzle rings; physical stops are approximately -1/130
degrees. BP's hinge servo used model-origin linear velocity where Newton needs
COM velocity. The application now adds the rotated native local-COM contribution
to the existing hinge plan. Actual-adapter ABI and shipped-Newton offset/zero-COM
fixtures pass activation-angle, stationary-acquisition, pivot and limit checks.
This preserves the original script/controller lifecycle. The maintainer's
2026-10-16 headset report accepts these previous corrections on installed probe
`AF7F3D…6C8`; this closes the reported barrel, wall-lever and handle-separation
cases on that local setup.

That report identifies the next progression boundary: the fixed computer in
Swanson's room was difficult to activate across its desk, and its screen cursor
required a physical mouse. The authored terminal is a native static Lamp with
2.1 m InteractDist and a LampLit callback opening ScreenGui. Exact Lamp identity
and zero body mass now admit it to the existing occluded head activation query;
native Lamp range checks remain authoritative. Dynamic Lamps and other physical
props retain palm selection. The next maintainer headset report on installed
probe `2AB4F0…CF7D` accepts activating the terminal at a good distance; this
closes that reported reach case. The IncomingChat popup still had no usable
controller cursor and could not be accepted. That report exposed a wrong native
cursor-family attribution: state 8 is the Schmup minigame; the computer instead
uses gameplay ScreenGui and HPL cGui's absolute mouse messages. The adapter now
feeds the existing tracked-menu UV policy into that verified native message
boundary and focused GuiSet, preserving popup attention, cursor artwork,
physical mouse priority and native left-down/up dispatch. The incorrect minigame
hook is removed. Actual-adapter tests reproduce cursor failure before the fix
and exercise popup movement plus Accept on VR release afterward. Cumulative
pointer/press/release diagnostics make the next live route observable.
The next maintainer report on installed probe `1E5861…DE89` accepts the fix;
the reported PC cursor/Accept and reach gates are closed on that local setup.

The maintainer then requested centered, approximately double-size text when
examining objects. BP GameMessage now uses native center alignment, 2x font
size/line spacing and whole-block vertical centering in the captured HUD plane.
Rework's wrapping algorithm counts lines using BP's native glyph measurement;
the original text, native fades, black-text mode, shadow offsets and depth
layers remain authoritative. Six exact caller hooks cover GameMessage only;
Radio subtitles, menu font calls and the existing SubtitleScale setting retain
their behavior. Synthetic x86 tests exercise all six calls, explicit/word-wrapped
lines, heap/inline text, native fallback and hook restoration. This readability
change subsequently received maintainer headset acceptance on installed probe
`8210CD…44DB`: centered placement and enlarged size are accepted on that local
setup. The scoped record is `releases/1.0.8-headset-acceptance.json`; it does not
promote the separate menu-light, flashlight or physical-step gates.

The same report identifies equipped light geometry frozen over menus. Stopping
tracked tool transforms did not suppress native mesh/billboard rendering.
The render owner now masks verified equipped flashlight/glowstick geometry
before native render-list admission and during drawing, restoring its exact
native flags afterward. Equip and light state remain untouched; the transaction
does not depend on a hands update while UI is active. Menu hide/resume behavior
still requires headset confirmation.

Reliable anatomical acquisition, sustained dragging/rotation/release of these
actual props and broader mechanism regression remain
headset gates. Dog head/jaw/hands have no separate physical bodies; the Neck
body owns compound colliders. That authored topology alone does not establish
the cause of the reported contact failures. Do not enlarge reach or bypass
native selection vetoes without new evidence.

The maintainer also reports backward world correction when leaning down/forward
to reach props and excessive body blocking. Local telemetry confirms a
physical-only solver residual being carried into the camera with no stick
request. Rework carries joystick movement separately after reconciling physical
acceptance. The new host-tested BP correction excludes residual from the initial
horizontal solve only for a physical-only tick with no native pre-injection
horizontal movement, a valid solver observation and no accepted native step.
Explicit backend carry preserves native travel outside the solve, no-request
ticks and existing mixed stick/HMD attribution. Actual probe-to-camera fixtures
failed before the fix and pass afterwards, including lateral rejection,
backward correction, overshoot, missing observation, native-step fallback and
native movement before/after injection. Release build, all 59 host tests and
exact-image/metadata checks pass. Leaning/reach improvement is not yet
headset-validated: the subsequent retest still reports world rejection and
excessive low-obstacle blocking. The correction fixes an attribution defect
but does not close that symptom. Synthetic native-phase tests do not validate real moving
platforms; later native travel cannot identify every platform/correction source.
Broader body-block severity remains unresolved; no collider/offset retuning is
included in this attribution correction.
The maintainer identifies a mattress in the egg/dog room and confirms blocking
with both joystick and physical approach. A subsequent exact-image audit found
that BP's native step CastRay requests distance but not normals, while the VR
positive/upward filter reads the shared ray-normal storage. That storage can
retain a previous query's normal. The host-tested `D7741` gateway now requests
the fresh normal only for the matching physical-step tick (including mixed
stick/HMD), without changing the native ray count, step height or solver.
The actual seven-argument native-call fixture failed before the fix and passes
afterwards for stale/fresh floor and wall normals, scope exclusions, signature
rejection and restoration. Combined Release build, 59 host tests, exact-image
and metadata checks pass; independent review found no blockers. Mattress
traversal and continued backward-world correction remain new headset gates.
Independent final review of the preceding candidate was unavailable due to an
agent usage limit; local review, compilation and host checks completed.

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

The v1.0.6 feedback update also addresses three Requiem reports without
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
