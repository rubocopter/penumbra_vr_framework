# Trilogy headset regression checklist

This is the trilogy release-regression checklist; the detailed gates below
provide broader Black Plague coverage. The current work
order is recorded in `ROADMAP.md`; the capability/evidence boundary is in
`TRILOGY_PARITY_PLAN.md`. Historical session chronology is intentionally
omitted. Do not repeat a closed reverse-engineering milestone unless new
evidence contradicts it.

## Before the headset session

### Consolidated feedback retest across the trilogy

The maintainer accepted the installed local trilogy checklist on 2026-10-10.
The exact hashes and case scope are in `releases/1.0.7-headset-acceptance.json`.
Do not repeat these cases as unresolved release gates:

- Overture: main/pause bilingual VR Settings and persistence, tested cadence,
  left-eye edge visibility, crouched hatch/door reach and input/focus recovery.
- Black Plague: mixed stick/HMD obstacles, input/focus recovery and stick
  response, VR settings/calibration, held grabs/turns/release/throw, inventory/
  notebook/context/UseItem and wall/step/crouch regression.
- Requiem: native bilingual VR menu, live edits/calibration/persistence,
  input/focus recovery, successful reported starts and height/crouch/slope.

Keep Quest 3/Link 72 Hz, optional Requiem Move/throw/new mechanisms, other
controller families and exact new-Setup acceptance separate. The confirmation
did not identify new hardware or independently document conditional cases.
Use the detailed gates for new regressions or broader coverage, not as a request
to repeat accepted focused tests without new evidence.

The BP distance/occlusion low-pass consumer and intermittent Requiem startup
crash remain unresolved implementation/investigation items. A successful session
does not close either without evidence specific to that issue.

1. Build and run host tests:

   ```powershell
   cmake --build --preset release
   ctest --preset release --output-on-failure
   .\tools\Test-PenumbraVrMetadata.ps1
   ```

2. Validate the target executable with `tools\Get-PenumbraBuildInfo.ps1`. Use
   only the allowlisted Black Plague build from `SUPPORTED_BUILDS.md`.
3. Install the current normal-Steam candidate with
   `Install-Black-Plague-VR.cmd` / `tools\Install-BlackPlagueSteamBootstrap.ps1`.
4. Confirm the deploy contains the payload declared in
   `assets/deployment/manifest.json`: bootstrap/probe DLLs, `openvr_api.dll`,
   action/binding assets, Rework hand texture, Spanish localization and the
   managed HRTF configuration boundary.
5. Start SteamVR and launch Black Plague from Steam's normal **Play** button.

Use focused validation helpers only when their extra telemetry is required.
Normal gameplay validation should exercise the production path.

## Gate 1 — presentation first

### Focused dream-ship progression retest

Use a save immediately before the ship transition after the sewers and record
the installed mod/build and controller family. This new report does not reopen
the accepted tutorial/chair checklist.

- Advance the arrival message with a controller trigger/Examine button, confirm
  the native completion restores control, and observe whether the world remains
  stereo during the message. Pausing/overlays must still work.
- While the message has disabled native player control, try stick movement,
  turning, crouch and physical translation; none may move the native body or
  change its scripted stance. Confirm normal control resumes on dismissal.
- Load Mental Realm and grab the yellow bucket under the table, lift it by
  the container and handle, turn the wrist, and release it. Confirm the internal
  hinge remains intact and neither body springs away. Bucket acquisition,
  throwing/reacquisition, chest opening and egg acquisition have local acceptance.
- Place the egg on the egg picture on the table near the coffin (`mr_table` /
  `stickegg`), not on the hand pedestal. Confirm native attachment and the
  subsequent spider/fire sequence. This sequence and dog appearance now have
  local acceptance. The earlier hand-altar instruction was wrong.
- With the articulated Grab candidate, acquire the dog at trunk, neck/head
  collision region and distal physical limbs. Lift, drag, rotate, release,
  throw and reacquire different bones. Watch for stretching, violent rotation
  or intermittent rejected contacts; include a held snap turn and tracking
  recovery. The visible mesh does not give every anatomical region its own body.
- While holding an articulated prop, lift/pull and rotate the wrist during
  sustained smooth turning. Physical motion must continue to drive the held
  bone. After a half-turn of the wrist, stop and check that a lagging bone
  converges instead of remaining inverted. These corrections are host-tested;
  their headset gate remains open.
- Exercise other reachable free articulated props: worm, human corpses,
  trash/food bags, pillow and mattress. Repeat endpoint and middle grabs,
  sustained pulling and release. Host topology/solver tests are not headset
  acceptance. Keep static/world-anchored cords and hanging props on their
  existing native ownership boundary.
- Repeat opening/closing a door, coffin/chest and locker; confirm hinges,
  stops, locks and script events still behave normally. Recheck the accepted
  bucket by handle and container, including throw/reacquisition.
- Test the later dog puzzle separately when reached: attach the trunk to the
  hand pedestal's hook (`altar01` / `stickdog`). Egg acquisition does not
  establish either attachment callback or dog progression.
- Compare reaching a floor prop in open space and beside a blocking prop,
  without joystick input. Lean forward/down, hold, then retreat; record any
  backward/lateral world correction and whether reaching becomes easier with
  the physical-only carry candidate. Repeat with real stick input separately.
  A passing attribution test does not close broader native body-block severity.
- Revisit the mattress in the egg/dog room with the fresh step-normal candidate.
  Approach with joystick and physically in separate trials, then retreat. Record
  traversal and world correction; do not infer headset acceptance from the
  synthetic step-ray tests or increase native step height to mask a failure.
- Examine nearby described objects and activate the kitchen save point near
  the computer. Record selected entity/body and native state on failure.

Before interpreting body or interaction behavior:

- enter gameplay from the menu;
- remain still for roughly ten seconds, then rotate the head for another
  20–30 seconds;
- confirm continuous stereo with no black frame, desktop-only fallback pattern or
  obvious alternating stale frame;
- confirm both controllers remain tracked;
- confirm the monitor-mirror preference behaves as configured;
- check the resulting log for compositor errors and repeated stale/reused
  presentation sequences.

Stop the session if this gate fails; body/interaction observations from a broken
presentation run are not promotion evidence.

## Gate 2 — body, room-scale and locomotion

Exercise the current production body path:

- stationary head motion and short physical X/Z movement;
- gentle wall pressure and sliding along a wall/corner;
- crouched tunnel/low-ceiling pressure;
- open-space room-scale walking;
- stick walk and sprint at several HMD headings;
- brief mixed stick + room-scale motion near a low/dynamic prop;
- one ordinary stair/ledge with pure stick locomotion.

Expected behavior:

- no repeated vertical mini-jump from wall/low-prop contact;
- collision correction must not drag the tracking anchor sideways/backwards as if
  it were accepted physical HMD movement;
- pure stick movement keeps native stair/ledge stepping;
- walk/sprint remains HMD-relative and uses the shared metric policy;
- no body/lifecycle access after death/menu invalidates the active player body.

## Gate 3 — crouch and calibration

- physical crouch down/up;
- button crouch down/up;
- Hybrid: release physical crouch while the button latch remains active;
- blocked stand beneath low geometry, then recovery in open space;
- standing/seated switch, height calibration and explicit recenter.

Confirm tracked Y, native stance and camera height remain coherent throughout.

## Gate 4 — turning, tracking epochs and transitions

- snap and/or smooth turning as configured;
- turn while both hands are visible and near geometry;
- explicit recenter;
- tracking loss/recovery if it can be exercised safely;
- representative door/map transition.

The user's world heading should survive authored map-start yaw. Hands must not
jump because collision history was computed in an older yaw epoch.

## Gate 5 — hands and direct interaction

With production palm collision enabled:

- inspect both hand textures, scale and orientation;
- articulate all five finger channels on each available controller;
- press/slide palms against wall/table/locker geometry;
- acquire several ordinary props naturally;
- exercise free-body grab, move, release and throw;
- specifically repeat with several `Grab=6` props of different shapes and hold
  each for a few seconds while moving/turning the hand; confirm the body follows
  continuously instead of flying to a fixed point until release;
- repeat acquisition with the hand close to/partly inside prop collision and
  confirm selection does not jump to a behind-ray candidate;
- approach movable props without pressing Interact and confirm mere proximity
  does not launch them;
- hold/release objects across turning and a representative transition where safe.

Aim/pointer behavior should follow raw tracking while the visible palm remains
collision-resolved. Acquisition assistance must remain bounded.

For the reported Index finger mismatch, test bare hands away from geometry with
no tool attached. On each hand, start open and curl thumb, index, middle, ring
and pinky separately, then reopen between gestures. Record which visible finger
responds to each physical gesture. Repeat the same sequence in SteamVR's own
skeletal-hand display and record the controller type and game/mod version.
This distinguishes device/runtime curl data from the game's rig mapping;
an attached tool intentionally overrides measured curl with its handle pose.

## Gate 6 — constrained mechanisms and tools

Exercise representative mapped mechanisms:

- drawer/slider;
- hinged object or door;
- swing door where available;
- a recognized jointed `Object` path;
- flashlight and glowstick placement/toggle.

Native mechanism lifecycle remains authoritative. Unknown joint families should
not be forced through free-body grab behavior.

## Gate 7 — UI and settings

- open inventory and notebook in VR;
- from the floor beside Swanson's desk, look at the computer and activate it
  within its native range without climbing onto furniture; move the screen
  cursor with the tracked controller and accept the initial incoming-call popup
  by pressing/releasing the mapped select trigger on its Answer button;
- with flashlight/glowstick equipped, open pause/menu, inventory, notebook and
  computer UI; verify the held model is hidden, then resumes attachment when
  gameplay returns without changing which light is equipped;
- hover an inventory item, press the mapped context-action/Circle input and
  confirm the native item-action popup appears; press it again while the popup
  is active and confirm the native close path still works;
- validate at least one contextual action and one `UseItem` target, including
  the controller-aim red/green usability beam;
- verify the captured gameplay HUD/subtitle surface appears in both eyes;
- examine short and multiline object descriptions: verify the complete text
  block is centered, its letters are about twice their previous size, wrapping
  stays readable and native fading/dismissal still works;
- open the native `VR Settings` page;
- test left/right value changes, Spanish/English labels as available, save and
  persistence;
- verify restart-required settings are labelled appropriately.

Do not promote `SubtitleScale` for Black Plague until subtitles are actually
observed and validated in-eye through the mapped HUD surface.

## Gate 8 — audio, haptics and visuals

When the scene provides suitable coverage:

- HRTF on the actual headset/audio device;
- environmental reverb/bus trim in a representative space;
- mapped pickup/drop/UI/direct-contact haptics;
- light toggle, damage and real-contact melee haptics where practical;
- Enhanced Visuals on/off startup and representative image/performance behavior;
- revisit the messhall window/light-ray scene and confirm the
  `_bb_blue_lightray_halo` billboards no longer stretch into square cards or long
  rays after the per-eye native camera-position transaction;
- camera-facing particles such as tunnel vapor after the per-eye refresh change.

For a scene that reproduces a smoke/vapor artifact, inspect the same log window
for `particle_updates`, `particle_eye_refreshes` and `particle_refresh_misses`.
Nonzero eye refreshes prove that effect activity reached the Rework-derived
particle path; persistent artifacts with no matching refresh activity should be
classified as another render family before changing particle policy again.

The missing distance/occlusion low-pass consumer remains implementation work and
is not part of a passing audio gate yet.

## Gate 9 — focus, mirror and shutdown

- Alt+Tab/focus loss and return;
- monitor mirror on/off if both modes are relevant to the test;
- normal game exit;
- verify no new matching Windows crash report and no unmanaged hook/resource
  teardown failure.

The probe DLL intentionally remains resident until process exit; validation is
about restored hooks/resources and clean process lifetime, not live DLL unload.

## Requiem throw and selection — independent acceptance

Record the installed Framework version/hash, Quest 3 connection and active
bindings. Caelus's positive walking/jumping report and brief favorable audio
observation are not pinned to an exact artifact; they do not validate this
new throw/selection candidate.

- At level03's explosive ketchup bottle, hold the bottle and aim the holding
  controller at the bridge/chain trigger; press Examine (default B with the
  right-handed Touch layout). Repeat with the body/head facing elsewhere and
  with either hand holding it. Confirm a predictable trajectory and puzzle
  progression. Remapped/left-handed bindings may assign Examine differently.
- Compare ordinary gesture release with the assisted button throw. Carry with
  stick movement, turn, release/reacquire and try a nonthrowable prop. Native
  script events and CanBeThrown must remain intact. Lose focus/tracking or open
  UI while holding; there must be no unintended release momentum.
- Look at a described object with the hand pointing elsewhere; its Examine cue
  should be easy to find and the action should examine that head target even
  while the hand is near another object. Check repeatable and once-only text,
  native distance limits and foreground walls. Distant props must not become
  remotely grabbable.
- Approach an accessible ladder while the hand points away. Confirm its native
  cue and climbing action follow the visible head target within native range.
  Repeat outside horizontal/vertical reach and behind foreground geometry;
  native admission and occlusion must still reject those cases.

## Trilogy feedback candidate — independent acceptance

Run these checks on each changed game/build; record the exact candidate hashes,
Quest 3 connection/audio route and active SteamVR bindings. Host tests do not
close these checks.

- On an unobstructed measured route, compare full-stick walk and sprint at the
  same `MoveSpeed` setting. Normal targets are 3 and 4.5 m/s. Repeat while making
  small physical head/body movements; cadence or mixed input must not reduce the
  intended stick speed. Test stopping, wall rejection and a map transition.
- Compare a short jump press and a held jump from standing in all three games;
  include sprint takeoff, crouched takeoff, a low ceiling, landing and releasing
  input/focus during ascent. Reproduce the reported Requiem long platform jump
  using the affected save/map. The supplied non-VR video at 18:31 identifies a
  reference obstacle; it does not validate the VR candidate.
- In BP and Requiem, use a fixed world sound and keep the character/stick still
  while turning only the HMD by 90 and 180 degrees. Repeat with room-scale
  translation and after recenter. Check player-relative sounds separately,
  then open/close UI, lose/regain focus and change maps. Confirm perceived
  direction and HRTF on the actual headset output device.
- Traverse the same slope/stair surface up and down using stick alone, physical
  motion alone and mixed input. Include the reported Requiem area and BP
  cave/snow surfaces. Check for vertical world oscillation, stepping onto low
  obstacles and retaining proper collision at walls/ceilings.
- In BP, compare a closed filing-cabinet drawer with a working desk drawer.
  Keep head/stick still and pull the interacting hand; then repeat while walking
  backward. Check both travel stops, obstruction/recovery, release/focus loss
  and reopening. Native limits and puzzle/script behavior must remain intact.

## What to retain after a run

Keep durable evidence small:

- target executable hash and Framework commit;
- the relevant log or concise automated summary while the issue is unresolved;
- a short written observation for each gate exercised;
- a screenshot/video only when it is needed to diagnose an unresolved visual or
  interaction problem.

Once a capture/video/contact sheet has produced a code fix or a durable finding
in manifests/state documentation, delete the local media. Do not keep large
video evidence in the project tree as historical archive.

## Promotion rule

A capability moves up the evidence ladder only when the session actually
exercised that capability on the documented build/current release. Positive
results in one gate do not promote unrelated gates, and an old run does not
validate a later host-only fix.
