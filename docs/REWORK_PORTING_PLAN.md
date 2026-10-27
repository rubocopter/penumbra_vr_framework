# Penumbra VR Rework porting contract

This document defines how proven Overture/Rework behavior moves into the shared
Framework. Cross-game completion is tracked separately in
`TRILOGY_PARITY_PLAN.md`.

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral
reference. The Framework-owned Overture product is the forward integration line;
Rework is not a build/runtime dependency.

## Porting rule

For a demonstrated Rework capability:

1. locate the exact implementation and tests/evidence;
2. identify HPL/game/model-specific dependencies;
3. extract only genuinely game-neutral policy;
4. expose the narrowest backend/profile boundary needed by the target;
5. preserve proven behavior/constants unless target evidence demonstrates a
   necessary difference;
6. validate the target independently.

A replacement algorithm is justified only when Rework has no equivalent, the
target interface is incompatible, or evidence shows direct adaptation is unsafe
or incorrect. Record the reason in durable architecture/parity documentation.

## Bidirectional reuse

Rework is the baseline, not the ceiling. When Black Plague or the Requiem
backend demonstrates stronger game-neutral behavior, that behavior may become the
shared implementation after a real cross-consumer boundary is proven. The
Framework-owned Overture product should then consume the shared improvement too.

Game-specific mechanics, binary layouts, tool/model sockets and rig details stay
with their owner even when the surrounding policy becomes shared.

## Current portability boundary

The 2026-10-23 feedback corrections deliberately adapt three incompatible native
boundaries. Rework bounds physical HMD displacement separately from stick
motion; the binary hosts' combined queue incorrectly imposed the same 5 cm cap
on both. Its Scene explicitly derives the listener from the current head pose;
the binary consumers instead require a named OpenAL import adapter because their
temporary render camera overrides do not provide that contract. Rework's accepted
step placement uses the measured surface height, whereas the independently
decoded BP/Requiem success blocks advance by `dt * climbSpeed`, overshooting
shallow rises. The binary adapter bounds that accepted advance without changing
native collision or physics-update ownership.

BP's native slider callback also cancels the ported Rework velocity servo in an
authored sticky endpoint zone. The held-child Move adapter suspends only that
native rest policy during VR ownership, retaining joints, limits and script
callbacks and restoring endpoint data on release. A real Newton consumer test
demonstrates the difference. These are host-tested adaptations, not new headset
acceptance. The maintainer explicitly requested common normal trilogy movement
speeds rather than retaining Rework's slower comfort defaults; the shared normal
walk/run values are now `3/4.5 m/s`.

Normal jump parity also requires a source-host adaptation. Rework multiplies
Overture's authored launch by `1.7` and uses custom gravity `-10`; the independently
decoded BP and Requiem pipelines use the original `350`, mass `80`, gravity
`-18`, a `0.3 s` held counter and a native `60 Hz` logic/physics timestep.
Copying those one-tick forces directly into Overture's HMD-rate logic changes
height with cadence. Its backend therefore samples that native 60 Hz normal
vertical envelope and submits the required acceleration through `AddForce`.
This is an Overture-owned interface adaptation, not shared horizontal movement
policy or a new binary-host vertical hook. Native collision/state transitions
remain authoritative; external force/velocity changes, altered profile values
or disabled gravity stop sampling and retain the inherited scripted pipeline.
The extracted production Jump state has host cadence and ownership regressions;
actual solver contacts and the reported long-jump platform still need headset
acceptance. Exact native window hashes and profile values live in each binary
manifest's `normalJumpProfile`.

Overture's input boundary must also merge legacy keyboard and VR Jump before
publishing the native held counter. The inherited later keyboard-only release
erased a valid VR hold; separate true publications can increment that counter
twice. ButtonHandler now publishes the combined input through its existing
Framework owner and retains inactive-state cancellation. The native airborne
`Jump()` counter reset remains observable by the sampler, renewing held ascent
without applying a second launch.

| Capability | Shared destination | Target-owned part |
| --- | --- | --- |
| OpenVR poses/compositor lifecycle | runtime | frame/render entry point |
| Tracking/world yaw/recenter/play mode | runtime | native body/camera application |
| Metric locomotion and accepted displacement | runtime | native movement/collision boundary |
| VR footstep cadence | runtime | native surface/material footstep call |
| Logical input/actions/haptics | runtime + shared OpenVR assets | native intents and per-game action lifecycle |
| Settings/ranges/editor policy | runtime | storage/UI/backend capability map |
| Per-eye target/render policy | runtime | renderer/GL integration |
| Hand conditioning/articulation output | runtime | rig/bone/profile mapping |
| Palm sweep/recovery/contact math | interaction policy | native shape/contact query adapter |
| Grab/throw/nudge math | interaction policy | entity/body ownership and mechanism lifecycle |
| Slider/hinge servo behavior | interaction policy when compatible | native joint selection and special mechanisms |
| Tool/light grip behavior | shared behavior + profile | model geometry, sockets, inventory semantics |
| Tracked panel/menu policy | runtime | game UI/draw owner |
| HRTF/reverb policy | audio runtime | audio startup/effect hook/world query |
| Distance/occlusion low-pass | audio runtime | target world-query/audio boundary |
| Enhanced Visuals final eye treatment | visual runtime | renderer-specific material/light path |
| LAA requirement | installer policy | allowlisted executable transform/hash |
| Backup/repair/rollback | installer | per-game payload/build manifest |

## Extraction state

Already demonstrated as reusable and consumed by Overture plus Black Plague in
some form:

- tracking transforms, play mode and world-yaw policy;
- settings schema/editor semantics;
- logical OpenVR actions and controller binding graph;
- metric locomotion and accepted-motion policy;
- hand pose conditioning, contact math and free-body interaction policy;
- haptic event policy;
- stereo/render-target policy and visual calibration pieces;
- HRTF configuration and environmental-audio parameter policy.

Still incomplete at the cross-game level:

- final mechanism coverage and physical-interaction headset parity;
- distance/occlusion audio consumption in Black Plague;
- complete reusable UI/subtitle behavior;
- full Enhanced Visuals parity beyond the transferable eye stage;
- controller-family hardware validation;
- final installer user acceptance and release audit.

Some Rework behavior is intentionally product-specific unless future evidence
proves otherwise, including exact HPL material/light response, model/weapon
statistics, authored rig/socket data and source-game UI/gameplay mechanics.

### Release-blocking native boundary corrections

Rework `23c890f` generates Index (`knuckles`) bindings from the generic
Touch-style `/input/joystick` component. Framework inherited that graph in
both payload roots. SteamVR's installed
`indexcontroller/resources/input/index_controller_profile.json` instead
declares `/input/thumbstick` with joystick type, position and click; it has
no `/input/joystick` component. Retaining the reference path is incorrect at
this controller-profile boundary. The generator now gives Index its own
physical input map, preserving the logical vector2 actions and joystick mode.
Both dominant-hand layouts and their UI stick clicks consume that correction;
other controller families retain their own paths. The regression generates
fresh graphs and checks the two distributed Index graphs against that physical
contract.

The follow-up compares the exact reference graphs with installed SteamVR Index,
Sense, Touch and Vive profiles. `pose/tip` is the declared pointing source;
Index grip/trackpad expose force rather than physical grip clicks. Index light
was assigned to the offhand interaction trigger, and Touch/Pico recenter named
a wrong-hand X and duplicated a gameplay click. These profile boundaries cannot
retain the reference verbatim. Index now follows Sense's grip/light division,
uses force clicks following [Valve's Moondust example](https://github.com/ValveSoftware/Moondust/blob/master/bindings_knuckles.json),
and puts pause on the spare right trackpad. Touch/Pico recenter remains
deliberately unassigned; WMR's mirrored pause/recenter collision is removed.
Vive/WMR's other compatibility limitations are documented rather than broadly
redesigning their layouts. Physical component semantics follow
[Valve's input-profile contract](https://github.com/ValveSoftware/openvr/wiki/Input-Profiles).

Overture's inherited fallback only entered when the entire action context was
inactive. A valid turn action therefore prevented legacy movement recovery when
move alone was missing. The source consumer now applies the existing legacy
analog/dead-zone behavior per inactive stick, preserves any active action
(including neutral custom bindings), and never imports legacy button bits.
SteamVR's Index legacy graph can synthesize a pad press from stick position, so
using that press for crouch would be an incorrect adaptation of this boundary.

Rework retains its calibrated tracking height when world yaw changes. BP's
renderer instead rejected the whole room-scale placement on a yaw-epoch
mismatch and fell back to native camera height. The actual camera-composition
host regression reproduced that loss. BP now keeps the fresh same-origin
world/feet anchor and height while rejecting prediction from the old yaw basis.
Tracking-origin discontinuities retain the original rejection; the body adapter
still owns sample freshness/generation and the one native physics update.
The composition lives in a BP-owned, game-call-free source module so that its
real consumer math is testable offline. This does not prove the reporter's low
baseline or native crouch state, and headset acceptance remains open.

The Overture door-nudge report was compared against Rework `23c890f`:
Framework retained the same contact-speed limits and `delta-v * mass` call.
The bundled Newton 1.x header declares `NewtonAddBodyImpulse` with
`pointDeltaVeloc`, and the HPL wrapper forwards its vector unchanged. A host
test against the actual shipped Newton DLL demonstrates that a 0.36 m/s request
became 1.08 m/s for a 3 kg body. Direct adaptation is incorrect at this boundary.
Overture now passes bounded delta velocity for recognized constrained hinges
and sliders, retaining the native joint lifecycle and existing contact-speed
policy. Loose-prop tuning remains the reference behavior. The real-DLL test
covers masses 1, 3, 25 and 100 kg. Door contact and crouched hatch reach received
scoped installed-local acceptance in `releases/1.0.7-headset-acceptance.json`;
broader mechanisms remain separate.

Investigation prompted by the BP tutorial shelf report independently exposed
the same incompatible boundary for loose-prop hand nudges. The clarified clip
and tutorial script establish that the shelf launch on chemical use is an
intentional native `AddBodyForce` callback, not a reproduction of the nudge
defect; see `TRILOGY_PARITY_PLAN.md` for that scoped evidence.
BP's verified `0x19C3D0` wrapper forwards its vector unchanged to
its own Newton import at IAT `0x2724E4`; the installed Newton DLL SHA-256 is
`69E6F1BA9D02E7FEBEFCB6DF9BFC6A5A8D8ED78FA659E86B06CA301BFAC894EE`,
identical to the real-DLL test dependency. A 0.10 m/s bounded request became
0.30 m/s at 3 kg. BP now sends point delta velocity directly for all hand
nudges, keeping its existing mass/size policy, contact direction, speed caps,
selection protection and native joints. Actual BP consumer math feeding the
real DLL passes at 1, 3, 20 and 100 kg. This evidence justifies adapting the
inherited `delta-v * mass` formula rather than preserving that incompatible
unit conversion. Character collision response is a separate unresolved gate;
the fixed native body boundary and solver remain unchanged.

Rework's source camera consumes native `GetViewMatrix`, which resolves dirty
position/rotation caches. Requiem previously copied the matrix cache before
native visibility resolved it, then fixed a per-body height calibration from
that possibly stale view. Opening inventory changes the tracked-body generation
and permits a new calibration, matching the reported recovery. Requiem now calls
its own manifest-confirmed lazy view/projection getters before both visibility
and stereo snapshots. Entry signatures are validated before hooks are installed.
A host dirty-camera regression demonstrates the stale floor-height capture and
its correction. The later intro-height report exposes a second difference:
Rework `Player.cpp` anchors Y to `GetFeetPosition()` and `Init.cpp` takes height
calibration from the user profile, whereas Requiem derived a permanent offset
from the native scripted camera once per body. Its intro script forces crouch
and LookAt before restoring control, so that camera is not a standing reference.
Requiem now composes feet, tracked HMD, configured height offset, seated policy
and configured button-crouch depth through the existing runtime transform.
Host cases cover standing, physical/button crouch, seated mode and unchanged
height after an inventory/body reset. No automatic native-camera calibration
or guessed offset remains; the maintainer accepted intro height and physical/
button crouch on the feedback candidate.

BP's vice exposes native `cGameWheel::OnPlayerInteract` through vtable
`0x2793E8+0x28`, targeting `0x3B8E0`, which commits native `Move=2` at
`0x3B9D4`. Palm targeting alone was insufficient: the committed hinge adapter
rejected Wheel. It now accepts Wheel only after that native transition and
retains the joint limits, native angle update and script callbacks. Rework's
interaction rejects non-interactive bodies before ranking; BP now excludes a
fixed Object/Wheel frame when the same entity has an active dynamic body, for
both palm and ranked-ray acquisition. Real adapter regressions reproduce the
wheel rejection and fixed-frame selection before the fixes. Physical vice
progression and locker acquisition were subsequently accepted by the maintainer
on the feedback candidate.

Rework `23c890f` copies refraction from a framebuffer whose dimensions match
its native screen texture. Requiem's binary renderer retains that copy path,
but the Framework draws each VR eye into a larger target; the shipped shader
samples the copy in eye viewport pixels. The refraction-on/off headset
comparison exposes this incompatible renderer boundary. A Requiem exact-build
adapter now captures the full eye into the bound screen texture only at the two
verified refraction copy callsites. The headset clip with Refractions enabled
shows the portal ring without the prior rectangular/striped defect; the runtime
copy counters match the intercepted calls, with normal pacing and exit.
Black Plague's initialized image independently maps the same renderer copy
contract at its own vtable and two callsites; its installed refraction shaders
also sample in viewport pixels. Both adapters now consume one OpenGL full-eye
capture implementation, with per-game caller allowlists and rollback ownership.
A real-driver host test verifies expansion, full-corner refresh and preservation
of GL binding/viewport state. BP's own headset run confirms correct special
effects in the observed scene, with native full-eye copy/resize telemetry.
This port does not establish that every smoke effect uses refraction or that
every material variant is validated.

Rework `23c890f` fixes the visible hand curl around an attached tool according
to its grip radius, overriding sensor curl while attached. Black Plague and
Requiem consume the shared hold-pose policy. Requiem's tool clips exposed
finger intersection: its visible hand first lacked the attachment weight, and
the shared articulation then allowed sensor curl to exceed that weight. The
adapter now supplies the weight only after validating the live attachment,
and shared articulation fixes the curl to that weight. Host tests protect that
policy; slight finger intersection remains a headset limitation. Tool light
geometry is target-owned: Rework's modified DAE positions its ray billboard
differently from Requiem's installed asset.
The subsequent Requiem trial showed that the fixed curl alone did not keep the
native tool attached to the visible hand during stick movement. Unlike Black
Plague's tool path, Requiem used a raw grip for the tool while drawing the hand
from the collision-resolved palm. Its adapter now uses the same resolved palm
for both, with a raw fallback across missing publications and snap-turn epochs.
The latest headset clip and log validate motion coherence for all three tested
tools. The installed BP/Requiem flashlight DAE places spotlight/ray nodes on
model -Y. A target-owned measured-grip socket has headset evidence for beam/
model alignment, but copying BP's rotation mapped that axis backwards in
Requiem. Reversing the target socket has headset evidence for forward direction,
but restoring Rework's 1.6 parent scale reintroduced beam separation. HPL's
`BillBoard.cpp::GetModelMatrix` normalizes the axis billboard basis; its installed
0.210 height stays fixed while the centre at model Y=-0.203767 inherits scale.
Against the emitter at Y=-0.103966, 1.6 predicts a 0.05468 gap instead of the
native 0.00520 overlap. The target profile therefore keeps a rigid native-size
parent and 0.020 grip radius with the validated direction. This combined
correction has host tests and the user's final headset acceptance/image; a larger
tool requires separately proving the target billboard-size boundary. Earlier slight finger
intersection remains a known limitation.

Black Plague's VR-origin free `Grab=6` uses Rework's point-in-palm transform.
It previously anchored the body's local origin, independent of the selected
surface point.
Rework's variable surface contact and conditional `mbPickAtPoint` mode produced
different grip placements for the same Black Plague prop. In the 2026-09-24
headset clip, repeated native selections still needed many attempts despite a
valid nearby VR target: the extra palm-box contact test rejected them. A fresh
VR winner now uses the shared 0.40 m reach bound, while stale/native-only picks
retain the tighter box guard. The Requiem headset sequence later demonstrated
stable reacquisition of one prop from different surface points. BP now anchors
a fresh, accepted VR surface contact within the shared reach bound and retains
its origin fallback for stale/native-only picks. BP's own headset trial confirms
chair acquisition from different contact points. Held snap turns and throwing
were subsequently accepted on the installed local checklist; exact hashes are
in `releases/1.0.7-headset-acceptance.json`. Native-origin grabs and jointed
`Move=2` mechanisms retain their native behavior. A published world-yaw epoch
also distinguishes snap turning from physical palm jumps: free bodies follow
that rebase without a one-frame force impulse, while mechanisms rebase their
hand offset without teleporting the joint.

## Overture acquisition correction after Rework

Rework `23c890f` and Framework's Overture `PlayerState_Misc_VR.cpp` reduce
physical overlaps to one nearest candidate per priority class before testing
visibility. The Framework multi-body sight correction already excludes target
siblings as occluders while retaining unrelated solid barriers. A remaining
ranking defect exists in both implementations: if the nearest overlap is hidden
by unrelated geometry, a farther reachable contact in the same volume is never
considered. This can prevent acquisition around drawer fronts/frames.

Overture now checks the established head/palm sight rule before accepting a
nearer candidate into its class. An occluded overlap cannot replace a visible
winner, in either portal iteration order. Existing item priority, physical reach,
contact skin, shape dimensions and native Move lifecycle stay unchanged; no
universal reach or collision retuning was introduced. The bounded adapter
correction is justified by the inherited defect, rather than an invented
replacement for Rework's contact behavior. The host regression protects both
orders and avoids sight queries for candidates farther than an accepted winner.
The maintainer accepted the initial drawer and repeated crank/mechanism use in
an Overture headset session installed with the exact 1.0.1 candidate. Broader
progression remains a separate regression gate.

## Native Push axis projection and Overture input validity

Rework `23c890f` `PlayerState_Interact_VR.cpp` retains side-effecting native
Push movement callbacks: forward/right forces use the camera axes captured by
Push Enter. Framework's binary Black Plague adapter previously sent raw stick
components to those callbacks while its body owner moved in HMD world axes.
A rotated-head host reproduction proved perpendicular force/body intent.

The correction promotes Requiem's existing neutral horizontal projection into
`runtime::ProjectNativePushAxes`, now consumed by both backends. Black Plague
independently supplies its Push vtable `0x27CFF8`, forward/sideways slots
`0x4C/0x50` (`0xACCC0/0xAB290`) and Enter-captured axes at `+0x14/+0x20`.
Runtime contains none of these binary values. Both primary Push callbacks and
the secondary locomotion permission gate retain native ownership. Host tests
exercise actual BP dispatch, native rejection and unchanged non-Push behavior;
the exact-image verifier pins the captured stores and force consumers. The
maintainer accepted the representative tutorial-crate feedback retest.

Rework and Framework Overture's `TrackedController::BeginPoseFrame` inherited
an auxiliary aim-validity omission. The maintained product now clears aim
validity per frame, on lost grip and on inactive/failed aim reads, preserving
compositor grip fallback. Its action and legacy consumers also check OpenVR
input availability before sampling or using binding-refresh grace, emit held
release edges once, and latch held digital controls through focus recovery.
This follows the focus boundary already consumed by BP/Requiem rather than
changing controller profiles. Tests compile the actual Overture input sources
against substituted external OpenVR interfaces; no physical-device or startup
crash acceptance follows from those tests.

The BP tutorial follow-up found a second difference from Rework's kinematic
Grab: BP ejected a valid hold solely for `dt > 0.25`, although that path writes
a rigid tracked transform and does not integrate the timestep. This cutoff is
removed while finite/positive-time and physical-discontinuity guards remain.
BP's `ControllerWorldPose` also requires a render-published world reference
younger than 250 ms. An expired reference now defers Grab transform writes
while the controller is still tracked and the interaction remains held; the
backend retains collision exclusion and clears residual velocity during that
deferral. It never drives from stale world coordinates or suppresses actual
release, focus loss, tracking loss or disconnection. This adaptation is needed
because BP's independently published binary presentation boundary differs from
Rework's synchronous source hand-pose consumption. Actual-adapter tests cover
the boundary; renewed headset acceptance remains separate.

## Shared action release and dead-zone hardening

Rework `23c890f` `SteamVRInput.cpp` reconstructs interaction release from the
preceding held state, while its other digital actions require an active binding
and `bChanged`. An individually inactive binding therefore cannot report its
own release. The shared router extends the preceding-state rule to digital
actions within an unchanged context and hand; context/hand entry keeps the
reference edge suppression. BP/Requiem consume this shared router through
`VrActionInput`. Native lifecycle and context selection remain backend-owned.

Transient focus loss, read failure and complete binding inactivity previously
called the router reset that also discarded the configured movement dead zone.
`VrActionInput` now preserves that setting across these transient resets. An
explicit reset still restores defaults. The public dead-zone helper also
rejects non-finite axes, normalizes non-finite settings to the existing default,
and bounds finite axes before radial rescaling, matching the action reader's
physical axis range. Frozen-sample checks reuse already validated grip flags.

These changes pass the Framework host regressions for individual binding
release, context/hand suppression, transient setting preservation and invalid
analog data. The maintainer accepted input/focus recovery on the installed
BP/Requiem follow-up; the exact hashes and acceptance scope are recorded in
`releases/1.0.7-headset-acceptance.json`. Version 1.0.7 publishes this hardening;
synthetic invalid-data tests and other devices remain separate evidence.

## Overture input consumer follow-through

The v1.0.11 Overture consumer reconstructs individually inactive digital
bindings' releases from the preceding held state, matching the shared-router
improvement above. Rework's interaction is an OR of the two triggers; a second
trigger previously changed the input owner while Grab retained its acquisition
hand. The consumer now retains that hand until the logical hold ends, so pose
loss and cancellation monitor the same owner as native interaction.

Rework's `VRHelper::TraceUIPointer` can use the other tracked hand when the
preferred hand is unavailable. Input and geometry now consume one physical-hand
selector, including mirrored UI action sets and legacy buttons. A hand change
ends the old pointer's hold and requires release before the new hand can press;
focus/context suppression remains authoritative. This is an adaptation of
Overture's OpenVR source interface, not a new shared action or binding profile.

Rework's ButtonHandler recenters orientation directly even outside gameplay.
The source integration retains that body-independent path and publishes the
latest head pose before input when a player update is paused. It does not consume
the motion baseline during ordinary input synchronization. Host tests compile
the actual input, pointer and source-integration consumers and cover both hands,
binding loss, focus recovery, bodyless/pause recenter and subsequent movement.
New headset acceptance is still required for this candidate.

## Native menu and mixed-step adaptation

Rework `23c890f` owns its VR options in source-native menu widgets. Framework
Overture retains that page and moves its entry/Back route to Start. BP's accepted
native-widget injector provides the binary equivalent. Requiem independently
maps the same constructor/list/state interfaces in its canonical initialized
image; the shared HPL adapter now contains the widget lifecycle/localization,
while each backend keeps exact addresses/layouts and its consumed setting set.
This is an evidenced second consumer, not a universal binary HPL contract.
Requiem observes its existing input-update owner for initial menu injection;
it does not add a second hook to a claimed callsite.

Rework Player.cpp performs physical motion with `vr_stepstaticonly=true`, then
clears that flag for its separate joystick body update. BP must retain one
native body update and therefore injects both requests through `0xD7281`.
Applying physical static-only eligibility to this mixed update rejected a
movable step candidate even when joystick movement was injected. The actual
adapter host regression reproduces that difference. BP now admits dynamic
candidates in the mixed case while retaining finite, upward-normal and positive
height guards for physical contact. Pure physical motion remains static-only;
pure native stick movement is unchanged. No additional physics update, guessed
collider, scale, Y clamp or movement multiplier is introduced. The mixed-case
correction is host-tested and the maintainer accepted the mixed-obstacle
headset checklist on the installed local build; see the scoped 1.0.7 acceptance
record. Broader collision comfort remains a regression topic.

## Black Plague inactive Message adaptation

Rework `23c890f` `ButtonHandler.cpp` keeps gameplay actions available for
Message even when the player is inactive; `PlayerState_Misc_VR.cpp` advances
the native message from Interact or Examine. BP independently demonstrates the
same exception at `0x505D/0x506A`, reaching the existing action owners at
`0x542A/0x5496`. Its Message constructor at `0xAD5B0` stores state `5`, and
the `0xAD5E0` handler forwards through `init+0x160` to native advancement at
`0x3F8A0`.

The Framework's former inactive-player UI gate prevented this adaptation.
The BP input adapter now preserves Message's gameplay context with overlay
precedence, leaving advancement, previous-state restoration and completion
callbacks native. Motion ownership remains gated by the native active byte;
the existing body injection owner also rejects previously queued VR requests
if gameplay becomes inactive. No mouse synthesis, direct advancement call,
second body update or shared binary enumeration is introduced. Exact evidence
lives in BP's manifest and `Test-BlackPlagueInputMap.ps1`; validation state and
scene-specific unknowns belong to `TRILOGY_PARITY_PLAN.md`.

## BP physical selection and independent gaze

Rework `23c890f` rejects non-interactive palm contacts before ranking and runs
a separate head-origin Examine query with interaction priority suppressed.
BP's former type-only overlap filter allowed inert Objects to mask usable
contacts. Its own `SetInteractMode` derives `entity+0x37`, while native callbacks
use `+0x204` (Interact) and `+0x20C` (Pick). The adapter now skips only known
inert Objects before ranking, preserving scripted candidates and native final
acceptance without executing scripts to rank overlaps.

Rework's `GetPickedDist()` returns zero; copying that globally into BP would
bypass SaveArea and unrelated native limits. BP independently calculates distance
from the selected callback point to character position. Only fresh, matching
physical Object contacts within the proven reach adapt its Pick/Grab/Move/Push
calls and the crosshair caller to palm distance. Other consumers remain native.

BP has one normal pick callback, unlike Rework's separate Examine callback.
The backend therefore casts tracked-head gaze into a complete scratch callback,
lets native solid/area arbitration resolve occlusion, and restores the original
player callback after deriving the evidenced Examine-only branch. Normal Update
has one owner and publishes the gaze HUD cue independently of physical picking;
explicit Examine refresh selects the gaze point through the existing input owner.
Interact fallback is restricted to mapped areas and exact SaveArea identity,
retaining native callbacks and the save's 2.1 m guard. This narrow target-boundary
adaptation does not promote parented-body support. Subsequent scene evidence
identified `boat_bucket01` as native Grab with an internal dynamic hinge.
BP now consumes Rework's jointed Grab transform and reduced speed caps only
for closed, entirely dynamic same-Object assemblies after native Grab commitment.
External/world anchors and native Move mechanisms retain their existing owners.
Skeletal Grab props also enumerate inactive mass-zero animation colliders:
Rework's mesh loader creates these separately from dynamic ragdoll bodies/joints.
BP excludes only mass-zero, unjointed, non-character-colliding companions from
assembly membership, including when native entity activation has woken them.
Character-blocking static siblings and any joint to an excluded companion remain
rejected. The VR Grab owner does not change animation-collider activation or pose.
Host fixtures cover acquisition, either hinge endpoint orientation, carry/release
caps, restored settings and rejection of static/foreign/world/mechanism links.
Bucket acquisition by handle/container, lifting, throwing and reacquisition
received scoped local headset acceptance; broader assemblies remain open.

### Articulated Grab boundary

Rework `23c890f` directly places the selected Grab body and publishes tracked
velocities. In BP, doing that to one skeletal body bypasses Newton's joint
solve and can pull the visible skeleton apart; the maintainer reported extreme
dog deformation. This is the evidenced reason for a narrow target adaptation
instead of copying that matrix-write route into articulated Grab.

After native `Grab=6` commitment, the existing same-Object assembly guard
classifies the selected connected component. Two or more dynamic bodies use
bounded point-velocity corrections through BP's verified
`AddImpulseAtPosition(0x19C3D0)` plus bounded angular drive. The impulse argument
is delta velocity, as verified against the shipped Newton DLL; it is not
multiplied by mass. Native Grab lightens only the selected body; driving that
body alone cannot lift the remaining ragdoll against gravity. The same bounded
translation delta therefore also increments each connected dynamic sibling's
native linear velocity. Point torque remains on the selected contact. The
validated joint graph excludes animation companions and disconnected bodies;
no sibling mass, gravity, transform or speed limit is changed. The selected
body-local contact is retained. A world-yaw
rebase preserves the current bone pose while retaining the tick's physical
controller translation and wrist rotation: previous/current raw grip poses are
mapped through the same current world basis before rebasing the palm anchor.
This avoids erasing lift/pull on consecutive smooth-turn yaw epochs. Angular
error uses the shortest axis-angle rotation, including a stable half-turn axis
when the cross-product error vanishes at 180 degrees. Existing angular speed
and per-tick acceleration limits remain in force. Wrist correction consumes the angular
velocity after the point impulse so it cannot erase Newton's contact torque.
Temporary world-reference loss
suspends VR drive without freezing one bone. Single rigid props retain the
Rework matrix route. The earlier two-body bucket exception was removed after
headset evidence of its handle separating: moving one of two constrained bodies
has the same solver-ownership defect as moving one ragdoll bone. Both bucket
members now consume the existing bounded contact/component drive; gravity-bearing
two-body hinge fixtures cover either selected body at the authored 3/2.2 kg masses.
Static/world/foreign endpoints and
native Move mechanisms remain outside this dynamic Grab adaptation.

BP's world-rooted `mr_hand_ragdoll` is also native Grab, not native Move. The
closed-assembly guard left it in the mouse/camera update, so tracked lifting
never reached its joints. Rework's direct selected-body placement is unsuitable
for this constrained skeleton for the same deformation reason above. A distinct
constrained Grab route now accepts verified same-Object dynamic graphs ending
at world or same-Object static roots after native Grab commitment. Acquisition
preserves the initial palm/contact offset as Rework Move does, avoiding an
initial pull before tracked hand movement. It applies
the bounded component contact correction only to connected dynamic members;
it writes no matrices, anchor state, joint limits or controllers. Wrist angular
override and tracked throwing are omitted, preserving Newton/native release
velocity. Foreign endpoints, animation endpoints and other entity/state owners
remain rejected. The mental-realm script owns its Grab-to-Static transition,
joint-limit locking and puzzle order. World/static-rooted three-hinge Newton
fixtures exercise every selected body with gravity, native-like mass reduction,
lift, fixed pivots and limits. Authored-model headset validation remains open.

BP's flashlight socket previously rotated model -Y into attachment-socket -Z,
opposite Rework's `VrRotOffset X=-1.57` inside the finger-cylinder grip frame.
The target-owned profile now rotates -90 degrees about X toward socket +Z,
matching the direction already validated independently in Requiem. BP retains
its measured grip, 0.020 m grip radius, resolved-palm owner and native model
size. Independent installed emitter/grip/ray-node checks cover both hands and
wrist rotation; BP headset acceptance of this correction remains open.

Human ragdolls require 36 enumerated entries (18 physical plus 18 animation
companions). Entity enumeration is bounded independently at 64; the dynamic
graph remains bounded at 32, with an explicit insertion guard. No asset name,
joint type or model-specific grip value selects the controller. Asset coverage
and validation state belong to `TRILOGY_PARITY_PLAN.md`.

### Push release and hinge centre of mass

Rework `23c890f` Push VR `OnStopInteract` only changes to the preceding state.
BP `0xAB1F0` additionally applies a captured-camera-forward impulse after that
transition when CanBeThrown is enabled. The maintainer's barrel-release clip
and the saved `3barrels` native Push mode identify this incompatible target
boundary. The spatial owner now intercepts its virtual release slot for
VR-owned/pending Push only; service, guarded and failed-acquisition cancellation
use the same verified native ChangeState call without the later impulse.
Native entry/leave and existing solver momentum remain intact; native-origin
mouse release still consumes the original handler. Actual-adapter tests
reproduce the post-Leave impulse before the correction.

Rework's hinge servo computes linear velocity at the body matrix origin.
Newton applies linear velocity at the centre of mass. Those differ for BP's
offset-collider wall lever. The application correction reads native
local COM, rotates it by the current body basis and adds omega cross that offset
to the existing plan. Contact radius, angular response, lightness, native
controllers, limits and event thresholds are preserved. This corrects an
evidenced limitation shared with the Rework formula; it is not a transcription
change or a new movement multiplier. A shipped-Newton offset-COM lever and
zero-COM control cross the native 110-degree activation angle within the
-1/130-degree physical stops while preserving the pivot within 1 cm.
Requiem's actual Move consumer independently reproduced the omitted correction:
the same offset-COM fixture drifted about 19 cm at its pivot and did not activate.
The correction now lives in `vr_mechanism_policy::CorrectHingeMassCentreVelocity`,
consumed by BP, Requiem and Framework Overture. Each native owner supplies its
own local COM and current body basis; RVAs remain backend-owned. Target consumer
and Newton tests establish host evidence only. No global lever sign inversion,
smoothing, new physics update or limit/controller change is part of this port.

Requiem's ScriptArea path also retains Rework Normal VR's dedicated camera-to-
resolved-hand-centre native query before physical selection. The independent
Examine gaze query and short physical palm rays serve different purposes and
cannot substitute for that area query. Native Clear/CastRay/CalculateResults,
occlusion, range and action callbacks remain authoritative. The short-ray
fallback compares area/contact points from one common origin; native distances
from a rear-offset ray cannot be compared with palm-origin overlap distances.
The existing central-ray/press-time auxiliary physical assistance is retained;
the dedicated area query runs identically during passive and action refresh.

### Fixed computer activation and desktop cursor

Rework `23c890f` Normal VR separates physical palm targets from the native
head/area ray, retaining original result arbitration and native handlers.
BP's fixed computer is a collidable Lamp (exact type 12/vtable `0x2763E8`),
not an HPL interaction area. Applying only the palm path forces the player
against its desk. The BP boundary now admits verified zero-mass Lamps to the
existing occluded head activation result. It preserves each Lamp's native
MaxInteractDist and LampLit/script lifecycle; dynamic Lamps and other props do
not gain distant physical acquisition. This is an exact target classification
adaptation, not an increase in shared physical reach or Rework's blanket
GetPickedDist override.

Overture has no equivalent computer desktop, but Rework's HPL cGui implementation
demonstrates absolute/relative SendMousePos messages to the focused GuiSet and
native mouse-down/up delivery. BP's initialized image independently proves that
same interface at the ScreenGui gameplay input branch. The adapter reuses shared
tracked-menu UV smoothing, reads the focused set's cursor and forwards both
position and delta through native SendMousePos. Native popup attention, hover,
cursor artwork, button callbacks and physical mouse priority remain intact.
The separate state-8 relative mouse path belongs to Schmup and must not be
classified as the terminal. Actual pointer and mapped press/release regressions
exercise a focused popup; headset cursor/Accept use remains a separate gate.

Rework's main/pause menu disables native scene drawing. BP keeps the world for
inventory/notebook and can render a desktop world while fullscreen ScreenGui
is active. Merely stopping tool attachment leaves native equipped geometry
frozen in that world. A BP render transaction now masks the verified mesh and
attached-billboard admission flags before list construction and drawing, then
restores their original values. It does not invoke native SetRendered, whose
submesh propagation would overwrite authored hidden states, or alter native
equip/light state. Existing render owners call this transaction directly;
no additional hook is stacked over those callsites.

### Examine message readability

Rework `23c890f` GameMessage draws at y=300 and applies SubtitleScale to font
size/line height. BP's equivalent uses a 14-pixel font, 16-pixel line height and
left alignment from (30,300). The maintainer requested centered text at roughly
double the previous size. The BP render owner now adapts only the six native
GameMessage WordWrap callers (black text or white plus shadows), forwarding to
the original native font renderer with 2x size/spacing and center alignment.
Whole-block vertical centering counts Rework's existing word-wrap breaks using
BP's native GetLength metrics. It copies borrowed VC7 wide-string contents into
modern scratch storage without constructing/destructing foreign CRT objects.
Native fade, duration, color, shadow pixel offsets, depth and text payload remain
unchanged. This is an explicit BP readability preference beyond the baseline;
Radio subtitles, menu callers and shared settings defaults are outside it.
Actual x86 callsite tests establish host evidence, not headset readability.

## Physical-only collision carry

Rework `23c890f`'s player update projects physical travel along the requested
direction, corrects rejected motion, and carries accepted movement in a separate
joystick branch. BP previously performed the same physical projection but then
carried the remainder of total body travel as locomotion even without stick
intent. The exact BP horizontal-solver observation now separates that residual
from later native travel for a physical-only solve with no native pre-injection
horizontal movement and no accepted native step. The adapter supplies explicit non-physical carry to its
reconciliation consumer, including observed native pre-injection movement.
Mixed/native movement keeps its existing attribution because BP exposes a
combined solve rather than Rework's independently sequenced updates. Native
movement after the horizontal solve and ticks without VR requests are retained.
No new physics update, collider, tracking offset or velocity multiplier is added.

Rework's step ray requests the surface normal that its upward gate consumes.
The older BP native step caller instead supplies `calcNormal=false`; the Newton
callback then leaves its global normal storage unchanged. The BP adapter's
normal gate therefore needed an additional exact-call boundary: `D7741`
requests normals for the current physical-step owner before the existing native
CastRay. The six-byte displaced callback MOV is replayed and execution resumes
at `D7747`. Native and unrelated-body queries retain their flags. This supplies
the missing input to the proven eligibility rule rather than relaxing it or
changing native step geometry, ray count, climb speed or gravity ownership.
Full step traversal parity is still distinct: Rework retries 0.05 m farther
forward and directly commits a cleared step pose; BP retains its native angular
ray search and gradual vertical climb with collision-resolved horizontal travel.
Those differences are not ported by supplying the missing normal, and must be
investigated independently if the focused mattress retest remains blocked.

## Requiem native button-throw adaptation

Rework `23c890f` Grab/Move/Push `OnStartExamine` examines the gaze target;
ordinary release transitions, with Grab publishing tracking-space velocities
rotated into world space. Requiem additionally owns native Examine launches:
Grab applies a mass-profile impulse, Move applies a mass-bounded force along
camera/cursor unprojection, and Push applies a mass-bounded planar force along
camera forward captured on Enter. Copying Rework Examine wholesale would remove
that existing Requiem gameplay action. The narrow target adaptation instead
keeps each native launch's force/impulse, mass, CanBeThrown and transition order,
using the existing owning-controller local -Z convention for VR aiming. Push
remains planar; constrained Move remains native. The Push cancellation boundary
uses native StopInteract, matching Rework's transition-only release. Exact RVAs,
native window hashes and original sphere asset facts stay in Requiem's manifest;
candidate validation and unresolved puzzle evidence stay in the trilogy ledger.

## Validation rule

Extraction, target implementation and target validation are different facts.
Use the common evidence ladder and never promote a target because Overture proved
the reference behavior.

For regressions, inspect in this order:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Current per-capability state belongs in `TRILOGY_PARITY_PLAN.md`; executable and
support identity belongs in `SUPPORTED_BUILDS.md`; active tasks belong in the
root `ROADMAP.md`.
