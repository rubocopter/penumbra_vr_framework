# Black Plague frame probe

## Scope

The probe proves a narrow integration path against this exact executable:

```text
SHA-256: FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF
Machine: x86
Build ID: black-plague-steam-observed
```

It is not yet a playable VR backend. Its default attached state only observes and forwards rendering. Bounded experimental commands have submitted native stereo and validated rotation-only tracking. A newer explicit continuous mode keeps OpenVR and adaptive runtime-sized targets active until stopped. Full-resolution PS VR2 sessions have validated its technical lifecycle, keyboard/mouse preservation and HMD-relative visibility. Frame pacing remains open.

## Loading model

Launching this executable directly on the development machine exits voluntarily with code 0 after roughly 380 ms. Launching Steam AppID `22120` creates the persistent game process. For that reason, the verified workflow is currently:

1. Launch Black Plague through Steam.
2. Locate the resulting process and obtain its executable path.
3. Recompute and whitelist-check its SHA-256.
4. Wait for `SDL.dll` and for the exact initialized bytes at the mapped `RenderWorld` call site.
5. Load the probe DLL with a remote `LoadLibraryW` call.
6. Call the exported `PenumbraVR_Initialize` function explicitly.

No substantial work is performed from `DllMain`; it only disables thread attach/detach notifications.

The direct-path launcher mode remains useful for diagnosing non-Steam builds, but it is not the verified route for this Steam installation.

The initialized-code gate is necessary because the executable is protected. One launch exposed `SDL.dll` before RVA `0x000EE010` had been reconstructed; the probe correctly rejected the still-mismatching call instruction, but loading it that early was itself unsafe. The launcher now polls for the exact five manifest bytes before injecting any DLL.

## Frame hook

The game imports `SDL_GL_SwapBuffers` from `SDL.dll`. The probe parses the host PE32 import directory and atomically replaces that one import-address-table slot.

- Function ABI: `void __cdecl SDL_GL_SwapBuffers(void)`
- Patched code bytes: none
- Modified state: one pointer-sized IAT entry in the main executable
- Callback behavior: increment a frame counter and periodically append telemetry
- Forwarding: every intercepted call invokes the original SDL function exactly once

The hook implementation does not assume the preferred image base and does not scan for an instruction pattern.

## RenderWorld call-site probe

The current build also prepares a passive counter at the single `cScene::Render` call to `cRenderer3D::RenderWorld`:

- call-site RVA: `0x000EE010`
- expected initialized bytes: `E8 FB EA 03 00`
- original target RVA: `0x0012CB10`
- replacement ABI: x86 `fastcall` adapter for the original `thiscall` function
- observed values: call count plus renderer, world, camera and frame-time arguments
- forwarding: the adapter invokes the original function exactly once and does not change an argument

Installation fails closed unless all five call bytes match. Other process threads are briefly suspended while the five-byte instruction is replaced or restored, and installation is aborted if a thread is currently executing inside that instruction. This call-site mechanism has host-independent Debug and Release coverage and has completed three live attach/detach cycles for the exact allowlisted research hash.

## Teardown

The current research build also observes the imported `glMatrixMode`, `glLoadMatrixf` and `glOrtho` calls. These hooks collect per-frame counters, the most recent projection matrix, a short projection call stack, and a bounded frequency table of model-view matrices. The table identifies the most frequently loaded model-view matrix without assuming a game address. All OpenGL arguments are forwarded unchanged.

`PenumbraVR_Shutdown` first destroys persistent GL resources through a render-thread request, then restores the SDL import, the `RenderWorld` call and the OpenGL imports. SDL, OpenGL and world adapters retain atomically published original targets and wait for active calls to quiesce.

The launcher deliberately does not invoke `FreeLibrary` after deactivation. A live test exposed a late callback into the unloaded research DLL despite successful pointer restoration. Keeping this small DLL resident until process exit makes late already-dispatched calls safe and matches the eventual mod's normal process-lifetime model. A subsequent `--attach` reinitializes the resident module; rebuilding the DLL requires restarting the game.

## Verification performed

On 2026-09-10, the read-only body/collision hooks ran during PID 30896 while
the user performed free locomotion, wall/obstacle contact and physical lean.
They identified the active player cylinder as `0.70 x 1.65 m` (radius `0.35 m`)
at `dt=1/60`, and recorded free accepted motion plus native slide, total block
and partial collision rejection. HMD/body horizontal divergence reached
`0.436 m` while positional translation stayed disabled. This makes the exact
body/collision mapping `live-tested`; it does not validate room-scale,
locomotion replacement, jump, crouch or camera/footstep bob ownership.

## Live-tested action ownership; camera path still pending

The next exact-build read-only capture must correlate six `cButtonHandler`
action callsites (jump, jump hold, sprint begin/end and crouch pressed/release-or-not-held) with
the existing `D460A` body tick. It must also observe the conditional
gravity-disabled calls in `iCharacterBody::Update`, `D790C -> D5F00` and `D7913 -> D6120`, and
the final render camera. Each wrapper must forward its original arguments and
return unchanged, validate its exact `E8` bytes/signature first, and publish a
monotonic sequence only. This is required to attribute state-machine effects
and camera offsets without changing locomotion, bob, jump or crouch behavior.

PID 25776 did **not** provide this capture. Its host SHA was the supported
`FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`, and the
body/collision probe installed, but the ownership probe rejected its manifest
before patching any of the eight calls. The fault was local to the probe map:
the held-jump map used `0x52CD`, whose exact-build bytes are `6A 01` (`push 1`),
instead of the following `E8` at `0x52CF` targeting `0x9A890`.

The corrected probe first checks all eight direct calls and their 16-byte target
signatures, then installs all hooks. A mismatch now names the concern and call
RVA and prints expected/actual five-byte instructions and decoded targets; a
target-signature mismatch reports both call and target RVAs and both signatures.
No installed probe owns these eight callsites: the input bridge owns the nearby
input-query calls (`5299`, `52C1`, `52EB`, `5313`, `533B`, `536A`), while the
ownership probe observes their later action dispatches. Player/body/camera
pointers are sampled on every callback rather than retained across a body/world
replacement. The observed gameplay-to-menu-to-gameplay transition is consistent
with recreation or level loading, but the log does not establish which.
PID 24780 is the valid follow-up capture: both probes installed on the same
allowlisted SHA. It observed sprint begin/end once each, one jump and 172 held
jump updates, and both crouch calls once for each press/release interval. The
active body changed from size `0.70 x 1.65 x 0.70`, centre Y `0.8297`, to
`0.70 x 0.95 x 0.70`, centre Y `0.4797`, with feet Y `0.0047` preserved; its
active physics-body pointer changed for crouch and returned when standing.
This is `live-tested` native action/body ownership, not physical crouch or
room-scale validation.

Static initialized-image decoding corrects two labels: `5347 -> 9CFA0` is the
pressed dispatch and `538A -> 9CFD0` is release/not-held dispatch. With toggle
crouch enabled both legitimately occur in one interval; neither count alone is
a crouch-state transition. `D790C/D7913` run only when body gravity is disabled.
The gravity-active player branches past them to later unconditional body sync,
so their zero counts do not identify the final player camera path. The probe now
refreshes `player -> +274 body -> +110 character camera` on every action
callback, rather than filling those fields only from the conditional calls.

### Live-characterized native jump body contract

The existing `D460A -> D6E00` observation wrapper accepts a burst request
only after the observed `5299 -> 9CEA0` dispatch has returned. It retains the
following 240 player-body updates (approximately four seconds at the proven
60 Hz cadence) and emits them only once complete. Every retained tick contains
the body-update sequence, `dt`, player/body pointers, body and feet position,
existing requested/solver/accepted deltas, and the derived vertical speed from
accepted delta. It also reads only the already-evidenced raw player fields
`+268`, `+26C`, `+1FC`, `+200`, `+204`, `+2D0` and its current state-object
pointer. No presumed native vertical-velocity offset is read and no game memory
or action behavior changes.

PID 29672 produced the complete 240-tick burst. State index `3` is the
source-correlated `cPlayerMoveState_Jump`: its `EnterState` vtable slot is
called by `9AB30` during `9CEA0`'s transition and establishes the vertical
force before the next `D6E00`. The first body tick accepted `+0.09222 m`
(`+5.53333 m/s` derived) and the apex reached `~0.95 m` over the `0.82970 m`
baseline. The known horizontal solver remained zero in Y throughout, proving
that `D6E00` applies the vertical state pipeline after its horizontal request /
solver phase rather than through that solver.

`9A890` sets `+1FC`; while held it accumulates `+200` from the native update
time. It does **not** clamp at `+204`: the capture reaches `2.233332`, and
release resets `+200` to the `+204` value of `0.300000`. In the Jump state's
source-correlated `OnUpdate` vtable slot `+8`, `+200 < +204` is the hold-force
threshold; beyond it the state follows its non-hold upward branch. This matches
the observed regime change around `0.3`, but the effective per-tick vertical
changes are not labelled gravity constants.

`+268` is now live-characterized as the 25-tick ground-grace counter: it falls
25→0 after take-off and resets to 25 on the tick after landing. It is not the
instantaneous body-grounded flag. `+26C` stayed zero and only acts as an
alternate branch in `9CEA0`'s eligibility test; its semantic name remains
unproven. Landing is resolved by `D6E00` at tick 10464; the subsequent move-
state update observes the landed body and transitions index `3 -> 0`, visible
at the next body tick. The final camera path remains outside this investigation.

On 2026-09-03, a Release build was attached and detached three times in the same Steam-launched process. Each cycle:

- revalidated the executable hash inside the game
- observed at least one real swap
- restored the original import
- appeared to unload the probe without an immediate fault
- left the game process alive for the next cycle

The cycles observed 1, 31 and 73 swaps respectively. Separate Debug and Release tests use small fake SDL/OpenGL libraries to verify IAT interception, forwarding, telemetry reset and restoration. A third test verifies direct `rel32` call interception, exact-byte rejection, forwarding and byte-for-byte restoration.

A later 122-frame capture in the main menu observed repeated matrix-mode changes and two `glOrtho` calls per steady-state frame, but no `glLoadMatrixf` call. This is consistent with a 2D menu.

An actual 3D gameplay capture then observed 702 consecutive frames. Every logged sample contained one perspective projection load, 64 model-view loads and three orthographic calls. The projection remained:

```text
[0.803333 0        0         0]
[0        1.428148 0         0]
[0        0       -1        -1]
[0        0       -0.100000  0]
```

Under OpenGL's column-major convention, this is a symmetric infinite-far perspective projection with a 70-degree vertical field of view, 16:9 aspect ratio and 0.05 near plane. It matches the released HPL1 camera projection formula.

A subsequent 1,560-frame capture grouped the model-view loads by exact matrix value. In a representative frame, 56 loads reduced to 20 unique matrices and the dominant matrix appeared 10 times. While the player moved and looked around, this dominant matrix changed coherently in both rotation and translation while the projection remained fixed. Representative samples were:

```text
initial rotation/translation:
[ 0.987300  0.007912  0.158671  0]
[-0.000000  0.998759 -0.049801  0]
[-0.158868  0.049168  0.986075  0]
[ 4.199785 -1.447538  2.489121  1]

after movement:
[-0.844894  0.063100 -0.531199  0]
[-0.000000  0.993019  0.117959  0]
[ 0.534933  0.099663 -0.838996  0]
[-0.726727 -0.725464 -7.200001  1]
```

This confirms a reliable API-level view-matrix signal. The later camera mapping ties it to `cCamera3D::GetViewMatrix`, which returns the matrix at `camera+0x44`. A version-gated remote read then confirmed that this field, after the normal `SetMatrix` transpose, exactly matches the dominant `glLoadMatrixf` value. The projection at `camera+0x84` matched in the same way; the read did not mutate game memory.

On 2026-09-04, the `RenderWorld` call-site probe completed three more attach/detach cycles in one live gameplay process, observing 1,676, 336 and 324 frames. Every sampled steady-state frame contained exactly one forwarded `RenderWorld` call. Renderer, world and camera pointers remained stable across the cycles, and frame time tracked the observed 60 Hz cadence at roughly 0.016–0.018 seconds.

The projection call-stack capture independently returned `0x00560212` followed by `0x004EE015`: the return from the mapped `SetMatrix` implementation and the instruction immediately after the mapped `cScene::Render` call site. Each detach restored the original five bytes and left the game responding immediately afterwards. Result: the call site is a verified live hook boundary for this exact executable hash; this evidence did not prove that unloading the containing DLL was safe.

A 362-frame gameplay capture queried the OpenGL state at entry to that boundary without changing it. After tightening the capability gate to require all ten FBO operations, including depth/stencil attachment, a separate 304-frame capture reproduced the result. A current OpenGL context was present on every sampled call. The NVIDIA driver exposed OpenGL `4.6.0 NVIDIA 616.56` and the complete required core framebuffer-object API. The observed state and implementation limits were:

```text
viewport:                  [0, 0, 2560, 1440]
framebuffer binding:       0
maximum texture size:      32768
maximum renderbuffer size: 32768
maximum viewport:          [32768, 32768]
```

These limits describe the test machine, not minimum requirements for Penumbra VR. The relevant conclusion is that `RenderWorld` runs with a current context, enters through the default framebuffer on this build, and provides the framebuffer API needed for reversible off-screen eye targets. The probe did not create or bind an FBO during these captures.

The separate `--validate-eye-targets` command then requested work from the injected DLL while leaving all OpenGL calls on the game's render thread. In one 172-frame attach/deactivate cycle, it created two complete RGBA8 plus depth24/stencil8 targets at `512x512`, bound and restored the left target, transactionally replaced both targets at `640x480`, bound and restored the right target, and destroyed every object. Framebuffer, renderbuffer, 2D texture and viewport state matched their incoming values afterwards. This validated the GL operations, but predated the corrected resident-DLL policy.

This deliberately transient test proves the GL object lifecycle in the real context. It does not retain targets between frames, duplicate `RenderWorld`, or submit an image to a headset.

A second explicit command, `--hold-eye-targets`, created a persistent `512x512` pair on the render thread, left it allocated for 363 `RenderWorld` calls, and reported it still active at frame 300. `PenumbraVR_Shutdown` posted a destroy request while the world hook remained installed and restored the GL state before removing hooks. The game initially remained responsive, but Windows Error Reporting recorded a `0xC0000005` execution fault in `PenumbraVR.BlackPlague.Probe.dll_unloaded` roughly three seconds later. This disproved safe live unloading even though GL destruction itself completed.

The corrected policy first completed two 122-frame menu attach/deactivate cycles against one resident DLL. It then completed two gameplay cycles that kept persistent targets alive for 420 and 422 `RenderWorld` calls respectively. Both cycles destroyed the targets on the render thread, restored incoming GL state, deactivated all hooks, reused the resident module and survived a 12-second post-deactivation observation window with no new Penumbra WER event. This closes the diagnostic persistent-target lifecycle for the tested policy; the targets are still not used for world rendering or compositor submission.

With SteamVR and the PSVR2 active, the injected OpenVR path subsequently reported a per-eye recommendation of `3400x3468`. It created two targets at that exact size, kept them active for 423 world frames, destroyed them on the render thread, shut down OpenVR and deactivated all hooks. The game remained responsive through a 12-second observation window and produced no matching WER event. A standalone session had earlier reported `4164x4244`; the runtime results are recorded independently rather than assuming one fixed headset size.

The next controlled command created a `512x512` diagnostic pair and issued 120 additional direct `RenderWorld` calls with zero frame time before each normal desktop pass. During steady duplication, projection loads rose from one to two per frame and model-view loads from 35 to 68. The command restored the framebuffer and viewport after every extra pass, destroyed the targets after 121 active frames and left the normal render cadence, process and delayed WER check healthy. It did not modify the camera or submit the diagnostic texture to OpenVR.

The per-eye matrix command then initialized OpenVR, read the live PSVR2 optics and derived HPL-compatible asymmetric infinite projections. It reported horizontal projection terms `[0.717113, -0.320757]` for the left eye and `[0.717113, 0.320757]` for the right, with eye-to-head X translations of `-0.0315 m` and `+0.0315 m`. For 60 consecutive frames it rendered both eyes into separate `512x512` targets, temporarily replacing `camera+0x44` and `camera+0x84` only around each extra pass. Projection loads rose from one to three per frame and representative model-view loads from 27 to 77. All 120 eye passes restored the captured camera bytes, framebuffer and viewport; the normal pass then returned immediately to one projection load. OpenVR and both targets shut down cleanly. The process remained responsive through 12-second pre-deactivation and post-deactivation windows, with no matching Windows Error Reporting event. These images were still not submitted to the compositor, so the headset continued to show SteamVR's cinema presentation rather than native stereo.

The compositor command completed three consecutive 300-frame runs in a later process. Every one of the 900 frames acquired a valid connected HMD pose, rendered both eye targets, submitted both OpenGL textures without a compositor error, flushed GL and restored the camera before the normal desktop pass. The user directly observed native full-view stereo instead of the cinema screen; resolution was visibly low as expected from the deliberate `512x512` diagnostic targets. The pose was used only to delimit compositor frames, so moving the headset did not control the camera. The process remained responsive through the delayed checks and deactivation, with no matching Windows Error Reporting event.

The test game process did not exit in response to a normal window-close request after verification and was therefore explicitly stopped. This does not count as successful launch/play/exit validation, which remains open on the roadmap.

On 2026-09-04, continuous presentation was started in a fresh Steam-launched process and retained SteamVR's full `3400x3468` recommendation without allocation fallback. It submitted 6,847 yaw-aligned rotation-tracked stereo frames during 120.6 seconds and crossed two observed changes of the `RenderWorld` world pointer. Every sampled frame reported a valid HMD pose, captured tracking anchor, two completed eye passes, restored camera and an empty stereo error. Explicit stop destroyed the eye targets on the render thread with GL state restored; detach then removed every hook after 6,990 observed frames. The game remained responsive for more than 23 seconds afterwards, its later process exit was observed, and the Application log contained no matching Application Error or Windows Error Reporting event from launch through exit.

SteamVR 2.17.8 recorded 10,839 presents, two dropped frames and 3,467 reprojected frames for PID 9448: 31.99% reprojection at a 90 Hz target. Its cumulative application times were 16.160 ms CPU and 14.721 ms GPU, while the probe commonly observed an approximately 60 Hz game cadence. This is evidence of a working continuous lifecycle, not acceptable final frame pacing. The startup-only SRV and unrelated dashboard/input-manifest warnings predated the game connection.

The user reported that keyboard and mouse appeared functional and that the image generally looked good. However, walls and other geometry were sometimes absent when looking towards them with the HMD and appeared after moving the game camera towards the same area; some nearby objects also appeared and disappeared. Static analysis then mapped `cScene::UpdateRenderList` at RVA `0x000EDF50`, its call at RVA `0x000EDF84`, and `cRenderer3D::UpdateRenderList` at RVA `0x0012A8F0`. This confirms the source-matching order: HPL1 prepares its portal/frustum render list before the intercepted `RenderWorld` call, while the original backend applied tracked per-eye matrices only inside that later call.

The correction hooks that earlier call while continuous stereo is active. After the first valid tracked frame, it derives the next visibility view from the latest valid HMD pose and current game-camera view, builds one symmetric frustum containing both OpenVR eye frusta plus a five-degree angular guard, and invokes the original update once. View/projection matrices, FOV, aspect and dirty flags are restored byte-exactly afterwards. It falls back to the untouched game-camera update on any validation error and exposes per-frame success, failure and restoration telemetry. A 110.3-second follow-up run exercised it without a technical failure, and the user confirmed that all geometry looked correct: neither missing walls on HMD turns nor nearby-object popping recurred.

## Commands

```powershell
# Attach to a Steam-launched process
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --attach <pid>

# Restore hooks and deactivate; the DLL remains resident until game exit
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <pid>

# With the probe attached, exercise transient eye targets on the render thread
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-eye-targets <pid>

# Keep a diagnostic pair alive until --detach performs render-thread teardown
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-eye-targets <pid>

# Experimental: initialize OpenVR and use its recommended per-eye dimensions
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --hold-openvr-eye-targets <pid>

# Experimental: issue 120 extra world passes into a diagnostic FBO
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-world-duplication <pid>

# Experimental: render 60 reversible stereo pairs into hidden targets
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-matrices <pid>

# Experimental: submit 300 static stereo frames
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-stereo-submission <pid>

# Experimental: submit 300 frames with recentered rotation-only tracking
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --validate-tracked-stereo-submission <pid>

# Experimental: start/stop continuous tracked presentation at adaptive resolution
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --start-vr <pid>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --vr-mirror-on <pid>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --vr-mirror-off <pid>
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --stop-vr <pid>

# Change the persisted mirror without a running game
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --set-vr-mirror on
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --set-vr-mirror off

# Read known cCamera3D fields without injecting or writing memory
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --inspect-camera <pid> <camera-address>
```

`--vr-mirror-on` and `--vr-mirror-off` persist the successful live choice in
`%LOCALAPPDATA%\PenumbraVR\settings.ini`. `--set-vr-mirror on|off` changes
the same setting offline, without a PID or an in-game VR settings page.
`--start-vr` applies that value before starting presentation; a missing file
or key safely defaults to mirror off.

The OpenVR commands require a build configured with `PENUMBRA_VR_OPENVR_SDK`. The controlled duplication command uses a `512x512` diagnostic target, preserves the normal desktop pass, passes zero frame time to each extra call and performs no camera mutation or compositor submission. The stereo-matrix command uses the same diagnostic size, applies the OpenVR per-eye projection and IPD only around each extra pass, verifies byte-exact camera restoration, and likewise performs no compositor submission. The static submission variant additionally acquires a compositor pose to delimit each frame, submits both OpenGL color textures and flushes GL, but deliberately ignores that pose for camera transforms. The tracked variant follows the Overture VR Rework tracking boundary: it aligns only the first valid pose's horizontal heading with the game camera and thereafter preserves the raw runtime pitch and roll. A near-vertical initial HMD orientation is rejected rather than used as a full 3D anchor. Six live cycles confirmed correct world orientation and horizontal/vertical response. The bounded tracked diagnostic keeps translation at zero. The continuous mode retains that transform and can consume the separate default-off room-scale validation sample, starts from SteamVR's recommended size at scale 1.0 and falls back proportionally if the x86 process cannot allocate the requested pair. Its Rework-derived scheduling gives the first eye the game frame time and always renders two native eye world passes. Mirror-on copies the left-eye texture to the desktop at swap; mirror-off clears the desktop backbuffer to black. Neither mode adds a third native world pass. Bounded diagnostics always keep their prior desktop pass. If stereo fails after the first timed eye, the fallback desktop pass receives zero frame time to avoid a double update. Pass ownership and counts are exposed in telemetry. Both continuous schedules have executed with the expected live pass counts and no stereo or camera-restoration errors. PID 26144 visually confirmed that mirror-off leaves the desktop black without the prior growing white-point artifact; a comparable frame-pacing measurement remains pending. Black Plague and Requiem are not Large Address Aware in their currently installed canonical state. The repository now recognizes host-verified exact LAA variants for both builds, but installed-file transformation still belongs to the future known-build-gated transactional installer before heavier Enhanced visuals buffers are enabled.

PID 19192 refines the earlier mirror-off observation: gameplay suppresses its
monitor world pass and appears black, while native 2D menus remain visible
because they do not call `RenderWorld`. This is consistent with the current
pass ownership. Mirror-on and comparable frame-pacing evidence remain pending.

Logs are stored under `%LOCALAPPDATA%\PenumbraVR\logs` and include the host path, SHA-256, build ID, frame telemetry and shutdown count.
Initialization publishes a capability bitmap for matrix telemetry, render/frame
ownership, native input, body/collision, body adapter, movement ownership and
spatial interaction. The launcher displays the optional subsystem states after
attach; a partial research probe is therefore distinguishable from a complete
gameplay stack.

## Render/body evidence and pending validation

Static analysis maps `cRenderer3D::RenderWorld` to RVA `0x0012CB10` and its sole direct call in `cScene::Render` to RVA `0x000EE010`. The call passes `mpActiveCamera` from `cScene+0x64`; `RenderWorld` forwards it to `BeginRendering` at RVA `0x0012A7F0`. The separate render-list update target is RVA `0x0012A8F0`, called from `cScene::UpdateRenderList` at RVA `0x000EDF84` before `cScene::Render`. The mapped camera getters return the view matrix at `camera+0x44` and projection at `camera+0x84`. Live validation covers these boundaries, stereo projection/IPD, yaw-aligned rotation tracking, HMD-aware visibility, keyboard/mouse input and continuous start/stop. Both two-pass and monitor-mirror schedules have executed with the expected live pass counts. The latest cap-disabled session has sampled intervals near 90 submissions per second; this does not establish compositor reprojection or worst-case timing.

For the lighting subtrack, the remaining physical check is the medium-distance
lamp dropout. A reversible
`glScissor` IAT hook now adapts desktop-pixel rectangles to each eye's viewport
only during stereo world rendering. It checks the current context, framebuffer
and viewport, leaving other destinations untouched. Eye bindings save/restore
scissor enable and box state, clearing inherited clipping before each eye.
`eye_scissor_remapped` and `eye_scissor_bypassed` expose per-frame activity in
the standard log. Host tests cover actual GL pixel coverage and hook teardown;
the [lighting checklist](VR_LIGHTING_VALIDATION.md) remains pending in game.
As of 2026-09-06, real VR input, native intents, tracked menus, provisional gloves
and free-body grab/throw are implemented and code-tested, not headset-validated.
See [startup/controller status](VR_STARTUP_AND_CONTROLLERS.md) and
[spatial adapter evidence](BLACK_PLAGUE_SPATIAL_NOTES.md). The exact-build
player link (`cPlayer+0x274`), native character-body tick (`D460A -> D6E00`),
active body/size/position fields and initial horizontal collision request
(`D7312 -> D4830`) are now statically mapped. Read-only telemetry for body/feet,
requested/solver/final displacement, physics timestep and unapplied HMD/body
divergence is implemented, host-tested and live-tested in PID 30896. Positional tracking,
palm collision, articulated mechanisms, tool/light attachment and Enhanced
visuals renderer hooks remain separate work.

PID 8628 completed that narrow adapter validation. It installed the native
input bridge, body/collision probe, adapter and ownership probe together. Free
movement, total block and slide/partial acceptance all preserved the measured
requested/solver/accepted boundary. `character_body` changed from `1E841A98`
to `1A4BE610` without losing observation, proving dynamic resolution rather
than a cached prior body. At `dt=0.016667` the native update rate remained
about 60 Hz: **native body update remains exactly once per native physics
tick**. This is not validation of room-scale, positional HMD translation,
tracking/body reconciliation, VR speeds, physical crouch, jump VR or bob;
`positional_translation_enabled=0` throughout.

The first attempt failed before movement because the adapter was installed after
the native input bridge had legitimately replaced the two movement calls, but
still expected pristine bytes. Installation is now ordered after both existing
owners: `NativeInputBridge` owns the movement callsites and
`BodyCollisionProbe` owns `D460A`; the adapter only registers fan-out through
their verified live status. A future diagnostic names the failing concern, RVA,
pristine/live five-byte instructions, decoded targets and owner state.

The physical-body boundary is now live-tested on the supported initialized
executable. At RVA `0xD7281`, immediately before the
native horizontal X/Z comparison, a five-byte `JMP rel32` gateway can inject one
bounded physical request for the current character body, replay the original
`fld [edi]` / `fld [esi+54h]`, and resume at `0xD7286`. X/Z is finite and clamped
to the shared `0.05 m` maximum physical step, Y is forced to zero, body/owner
mismatch fails closed, and the gateway never invokes `D6E00`.

The dedicated validation mode is default-off and is activated with
`PVR_BP_PHYSICAL_DISPLACEMENT_VALIDATION=1` or the transient
`Local\\PenumbraVR.BlackPlague.PhysicalDisplacementValidation` mutex. Its
telemetry distinguishes queued, consumed, injected and rejected requests and
measures accepted displacement from the pre-injection position. Local Release
build, **30/30** root CTest tests and `tools/Test-BlackPlagueInputMap.ps1` pass.
PID 26144 live-tested this path while positional HMD translation remained zero.
The session demonstrated queue/injection/matched reconciliation, native
`dt~=1/60` ownership and free/blocked/slide-or-partial outcomes. The original
stationary classifier incorrectly required zero injection, which is incompatible
with Rework `23c890f` reacting to any non-zero HMD tracking delta. Using the
corrected 2 mm significance threshold, the same log contains 12 stationary,
37 free, 2 blocked and 15 slide/partial samples.

The active consumer is implemented behind a separate
`Local\PenumbraVR.BlackPlague.RoomScaleValidation` mutex, accepted only while
physical validation is active. The adapter exposes a fresh reconciled X/Z
camera offset for the current body generation; rendering applies it to the head
view, visibility view and controller game-view basis. Stale or invalid samples
fall back to rotation-only, and Y/jump remain native. Use
`tools/Start-BlackPlagueRoomScaleValidation.ps1`; its final check requires
camera application, a non-zero offset, mirror-on gameplay, the native
`1.65 -> 0.95 -> 1.65 m` crouch shape sequence, a fresh camera sample and
meaningful physical movement after standing again.
The dedicated live launcher additionally fails closed unless the fresh process
log proves a non-zero queued plan, consumed/injected boundary request, matched
reconciliation, non-zero injected body telemetry, the native `dt~=1/60` tick
and sampled stationary/free/block/slide-or-partial physical outcomes. The case
classifier uses the existing pre-injection request/accepted telemetry; it adds
no hook or second body owner. The launcher reports newly captured scenario
classes during the live run, then performs the complete evidence check after the
game exits. PID 18392 verified the negative path: activation alone is rejected.

PID 24956 live-exercised that active path and produced 1159 body summaries: 90
stationary/jitter, 963 free, 10 blocked and 95 slide/partial classifications.
The crouch sequence recovered at 12:02:00.758 and was followed by 414 free and
25 slide/partial samples. The helper's only final complaint was the redundant
absence of another blocked sample after recovery; the global standing-shape
block was already present, so post-recovery now requires meaningful movement
rather than all three collision outcomes again.

The qualitative failure was unintended character walking during in-place head
tilt; stick then added to or opposed that motion. This must remain distinct from
deliberate horizontal head/torso translation. Static comparison also found a
real sequence defect. Rework `23c890f` performs physical reconciliation in one
character update, then carries the head anchor only with a separate accepted
stick update. Black Plague keeps one native `D6E00` and injects the prior
physical request into the tick that also contains native locomotion. The shadow
had passed that combined accepted displacement to locomotion carry after already
reconciling the physical part. It now subtracts the matched physical X/Z
component for carry, preserves actual whole-tick `body_after` for camera space
and logs the result as `locomotion_carry`. The correction is compiled but has not
yet been tested in a headset, so it is not claimed to resolve tilt-induced
walking.
