# Black Plague headset validation checklist

This is the current repeatable hardware checklist for the active Black Plague
candidate. Historical session chronology is intentionally omitted. Do not repeat
a closed reverse-engineering milestone unless new evidence contradicts it.

## Before the headset session

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
- approach movable props without pressing Interact and confirm mere proximity
  does not launch them;
- hold/release objects across turning and a representative transition where safe.

Aim/pointer behavior should follow raw tracking while the visible palm remains
collision-resolved. Acquisition assistance must remain bounded.

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
- verify the captured gameplay HUD/subtitle surface appears in both eyes;
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
- camera-facing particles such as tunnel vapor after the per-eye refresh change.

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
exercised that capability on the documented build/current candidate. Positive
results in one gate do not promote unrelated gates, and an old run does not
validate a later host-only fix.
