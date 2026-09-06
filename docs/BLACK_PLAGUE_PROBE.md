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

# Read known cCamera3D fields without injecting or writing memory
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --inspect-camera <pid> <camera-address>
```

`--vr-mirror-on` and `--vr-mirror-off` persist the successful choice in
`%LOCALAPPDATA%\PenumbraVR\settings.ini`. `--start-vr` applies that value before
starting presentation; a missing file or key safely defaults to mirror off.

The OpenVR commands require a build configured with `PENUMBRA_VR_OPENVR_SDK`. The controlled duplication command uses a `512x512` diagnostic target, preserves the normal desktop pass, passes zero frame time to each extra call and performs no camera mutation or compositor submission. The stereo-matrix command uses the same diagnostic size, applies the OpenVR per-eye projection and IPD only around each extra pass, verifies byte-exact camera restoration, and likewise performs no compositor submission. The static submission variant additionally acquires a compositor pose to delimit each frame, submits both OpenGL color textures and flushes GL, but deliberately ignores that pose for camera transforms. The tracked variant follows the Overture VR Rework tracking boundary: it aligns only the first valid pose's horizontal heading with the game camera and thereafter preserves the raw runtime pitch and roll. A near-vertical initial HMD orientation is rejected rather than used as a full 3D anchor. Six live cycles confirmed correct world orientation and horizontal/vertical response. Translation remains forced to zero. The continuous mode retains that transform, starts from SteamVR's recommended size at scale 1.0 and falls back proportionally if the x86 process cannot allocate the requested pair. Its Rework-derived mirror policy defaults off: the first eye receives the game frame time and the original desktop world pass is skipped, reducing three world renders to two. `--vr-mirror-on` retains the separately timed desktop pass and `--vr-mirror-off` restores the two-pass path. Bounded diagnostics always keep their prior desktop pass. If stereo fails after the first timed eye, the fallback desktop pass receives zero frame time to avoid a double update. Pass ownership and counts are exposed in telemetry. Both continuous schedules have executed with the expected live pass counts and no stereo or camera-restoration errors; an explicit visual check of the desktop mirror and a comparable frame-pacing measurement remain pending. Black Plague and Requiem are not Large Address Aware in their installed state, so the future installer must apply that known-build-gated change before the much heavier Enhanced visuals buffers are enabled.

Logs are stored under `%LOCALAPPDATA%\PenumbraVR\logs` and include the host path, SHA-256, build ID, frame telemetry and shutdown count.

## Current boundary and next validation

Static analysis maps `cRenderer3D::RenderWorld` to RVA `0x0012CB10` and its sole direct call in `cScene::Render` to RVA `0x000EE010`. The call passes `mpActiveCamera` from `cScene+0x64`; `RenderWorld` forwards it to `BeginRendering` at RVA `0x0012A7F0`. The separate render-list update target is RVA `0x0012A8F0`, called from `cScene::UpdateRenderList` at RVA `0x000EDF84` before `cScene::Render`. The mapped camera getters return the view matrix at `camera+0x44` and projection at `camera+0x84`. Live validation covers these boundaries, stereo projection/IPD, yaw-aligned rotation tracking, HMD-aware visibility, keyboard/mouse input and continuous start/stop. Both two-pass and monitor-mirror schedules have executed with the expected live pass counts. The latest cap-disabled session has sampled intervals near 90 submissions per second; this does not establish compositor reprojection or worst-case timing.

The next physical boundary is the medium-distance lamp dropout. A reversible
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
[spatial adapter evidence](BLACK_PLAGUE_SPATIAL_NOTES.md). Positional tracking,
palm collision, articulated mechanisms, tool/light attachment and Enhanced
visuals renderer hooks remain separate work.
