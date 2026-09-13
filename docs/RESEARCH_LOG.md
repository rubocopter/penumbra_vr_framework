# Binary research log

Use this log for conclusions that have been reproduced against an exact executable hash. Do not record guesses as resolved addresses.

## Current checkpoint — 2026-09-06

Real OpenVR actions, native Black Plague intents, tracked menus, procedural
gloves and free-body palm-relative grab/throw are integrated and code-tested.
One-step Steam startup is implemented; only its read-only preflight has been
executed in this batch. The launcher checks the loaded probe path and relocates
forwarded Windows exports through their actual owner. No new game/SteamVR
session was started, and the separate Overture Rework repository is unchanged.

All 22 tests pass locally in Release, Debug and SDK-less Release. The capture
verifier checks native input and spatial boundaries without modifying a process.
Proprietary captures, dependencies and build outputs are excluded from Git.
Hosted CI runs the 21 non-WGL-driver tests per configuration; that result is
separate from local GPU tests and from headset validation.

The third gameplay gate remains partial: palm collision, articulated mechanisms,
tool/light attachment and full Rework hand assets are not finished. See
`VR_STARTUP_AND_CONTROLLERS.md` for behavior/limits and
`BLACK_PLAGUE_SPATIAL_NOTES.md` for exact RVAs and the next integration boundary.
Earlier dated entries below describe historical milestones, not physical
validation of these newly implemented systems.

### 2026-09-06 — Persistent Black Plague input and comfort profile

- The probe now loads normalized handedness, analog move scale/dead-zone and
  turn mode/angle/speed/dead-zone from the framework INI before installing the
  native input hook. Preflight validates the same file.
- The dominant hand selects the corresponding OpenVR gameplay/UI set, tracked
  menu pointer and interaction owner. The opposite hand carries the installed
  glowstick/flashlight transform, matching the Rework ownership model.
- Snap turn retains neutral re-arming with configurable angle. Smooth turn uses
  the native update `dt`, a radial dead-zone remap and configured degrees/second;
  disabled mode emits no yaw input.
- The local right-handed test profile uses move scale `0.85`; this changes only
  the controller axis and is not evidence that game timing is corrected.
- The profile also drives adaptive per-eye `RenderScale` and the menu's
  `UiDistance`/`UiScale`. Drawing and controller-ray projection share the same
  geometry so comfort changes cannot introduce pointer drift.

### 2026-09-06 — Black Plague held-body character collision filter

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Question: can a palm-tracked free body be prevented from resolving contacts
  against the player and launching the character out of the map?
- Evidence: `iPhysicsBody` initializes byte `+0x3C8` to true at RVA `0xCD877`.
  The character-aware world query reads it at `0xD4952`, the character ray
  callback at `0xD4E0E`, and the registered Newton begin-contact callback in
  both body orderings at `0x19D2D0` and `0x19D2E4`.
- Registration evidence: `NewtonMaterialSetCollisionCallback` receives begin
  callback RVA `0x19D250` at `0x19DAA8`; that callback obtains both HPL bodies
  through `NewtonBodyGetUserData` before applying the field checks.
- Implementation: snapshot `+0x3C8`, set false only for an acquired free body,
  then restore the snapshot after native Grab Leave. Installation remains
  fail-closed unless every instruction signature matches.
- Release safety: the latest five finite controller velocities use a component
  median, require two samples, and are bounded to 9 m/s linear and 6 rad/s
  angular. A palm discontinuity above 0.35 m releases with zero momentum.
- Negative test: a synthetic body whose map-authored value starts false remains
  false after release; a true value is restored true. Parented and jointed
  bodies remain excluded from palm tracking.
- Result: exact-build boundary confirmed and code-tested; cautious headset/game
  validation remains required. The binary has no separate Rework-era
  `CollidePlayer`, so this temporarily filters all characters.

## Entry template

### YYYY-MM-DD — Short description

- Game/build SHA-256:
- Researcher/tool version:
- Question:
- Evidence:
- Module-relative RVA(s):
- Signature and mask, if applicable:
- Calling convention/register assumptions:
- Reproduction steps:
- Negative tests or alternate explanations:
- Result: hypothesis / confirmed / rejected

## Initial audit

- The observed Black Plague and Requiem executables are 32-bit PE images and import broadly similar HPL1-era dependencies, including SDL, OpenAL, OpenGL and Newton.
- Twenty-three provisional Black Plague patterns and one provisional Requiem pattern from the abandoned scaffold were scanned against the observed Steam executables.
- None matched. Those patterns are not carried into this repository.
- The Overture source and working VR Rework remain useful behavioral and structural references, but do not establish binary equivalence with either closed game.

### 2026-09-04 — Large Address Aware state of the binary games

- Builds inspected: Black Plague `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`; Requiem `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2`.
- Question: do the installed 32-bit executables already opt into an address
  space larger than the original 2 GB user-mode range?
- Evidence: both files have the PE32 optional-header magic `0x010B`, x86 machine
  type and COFF characteristics `0x010F`. Bit `0x0020`
  (`IMAGE_FILE_LARGE_ADDRESS_AWARE`) is clear in both images.
- Reference: Overture VR Rework explicitly enables the Visual Studio
  `LargeAddressAware` linker setting for its rebuilt game executable.
- Result: confirmed absent in these exact Black Plague and Requiem files. The
  unified installer should enable the one-bit flag only after exact-hash
  matching and verified backup; it must record the transformed hash and support
  byte-exact rollback.

### 2026-09-03 — Verified Black Plague frame and OpenGL telemetry

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`
- Question: can a version-gated DLL attach through Steam and observe a stable frame boundary without inline code patches?
- Evidence: three consecutive attach/deactivate cycles observed 1, 31 and 73 `SDL_GL_SwapBuffers` calls. Every cycle restored the expected IAT pointer and left the game active immediately afterwards. Later testing showed that this did not establish safe DLL unloading; see the delayed-unload entry below.
- Modified address: the main module IAT entry for `SDL.dll!SDL_GL_SwapBuffers`; resolved from PE metadata at runtime.
- Calling convention: `void __cdecl SDL_GL_SwapBuffers(void)`.
- Result: confirmed for this exact build.

OpenGL observation hooks for `glMatrixMode`, `glLoadMatrixf` and `glOrtho` were then verified independently in Debug and Release and run for 122 menu frames. Steady-state frames showed two orthographic calls and no loaded float matrix. Result: telemetry works, but the menu capture does not expose the 3D camera.

### 2026-09-03 — Black Plague gameplay projection and view behavior

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`
- Question: can the 3D projection and camera view be distinguished non-invasively at the imported OpenGL boundary?
- Projection evidence: a 702-frame gameplay capture reported one projection load, 64 model-view loads and three orthographic calls in every logged sample. The projection was stable at `[0.803333, 1.428148, -1, -1, -0.1]` in its non-zero OpenGL column-major elements.
- Projection classification: `fovY = 2*atan(1/1.428148) = 70.0000003 degrees`; `aspect = 1.428148/0.803333 = 1.7777783`; the infinite-far term gives `near = 0.1/2 = 0.05`.
- View evidence: a bounded exact-value histogram was validated in Debug and Release, then attached for 1,560 gameplay frames. A representative frame had 56 model-view loads, 20 unique values, no dropped values, and one matrix repeated 10 times. During player movement the dominant matrix changed coherently in rotation and translation while the projection stayed fixed.
- Modified state: only the existing IAT observation slots; no OpenGL arguments or executable code bytes were changed.
- Negative test: the earlier main-menu capture had orthographic calls but no float projection load, so the gameplay result is not merely a global per-frame UI matrix.
- Result: confirmed API-level perspective projection and moving view-matrix signals for this exact build. The owning HPL camera object and a safe stereo scene-render entry point remain unknown.

### 2026-09-03 — Original Steam executables contain a `.bind` entry layer

- Builds inspected: observed Overture retail, Black Plague and Requiem executables.
- Evidence: all three PE entry points reside in a final `.bind` section rather than `.text`. Straight disassembly of the Black Plague on-disk `.text` begins with instruction-invalid/random-looking data, while its import table remains readable.
- Black Plague entry point: `0x006F62ED`, `.bind` begins at `0x006F6000`.
- Requiem entry point: `0x006F92ED`, `.bind` begins at `0x006F9000`.
- Overture retail entry point: `0x006B82ED`, `.bind` begins at `0x006B8000`.
- Result: confirmed PE layout; likely executable protection/binding. Internal HPL signatures must not be derived from the encrypted-looking on-disk `.text`. The next check is to compare it with initialized process memory before choosing a dumping/disassembly workflow.

### 2026-09-03 — Black Plague `.text` unpacking and first HPL symbol

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`
- Initialized `.text` bytes compared: 2,558,295.
- Bytes different from disk: 2,548,280 (99.6085%).
- On-disk entropy: 7.99993 bits/byte.
- Initialized-memory entropy: 6.43571 bits/byte.
- Result: confirmed. Static analysis must use initialized memory for this build. A reconstructed analysis copy is kept locally under the Git-ignored `local/` directory and must not be committed or distributed.

The initialized code contains a unique routine at RVA `0x001601D0` whose behavior matches HPL1 `cLowLevelGraphicsSDL::SetMatrix(eMatrix, const cMatrixf&)`: it selects model-view/projection/texture mode, creates a 64-byte transposed matrix on the stack, calls `glLoadMatrixf`, and returns with `ret 8`. The recorded unpacked-memory signature has exactly one match. Result: confirmed static mapping, not yet hook-validated.

### 2026-09-03 — Requiem initialized-memory comparison

- Game/build SHA-256: `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2`
- Initialized `.text` bytes compared: 2,561,079.
- Bytes different from disk: 2,551,069 (99.6091%).
- On-disk entropy: 7.99993 bits/byte.
- Initialized-memory entropy: 6.43634 bits/byte.
- Result: Requiem uses the same protected-on-disk/normal-in-memory model as Black Plague.

The 37-byte unpacked-memory signature accepted for Black Plague `cLowLevelGraphicsSDL::SetMatrix` also has exactly one Requiem match, at RVA `0x00160970`. The routine is displaced by `0x7A0` rather than sharing a hard-coded RVA. Result: strong confirmation that engine-level symbol definitions can be shared while every executable retains its own version manifest.

### 2026-09-04 — Shared `cRenderer3D::RenderWorld` mapping

- Builds: Black Plague `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`; Requiem `B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2`.
- Question: where does HPL render one complete 3D world pass, excluding the later scene/UI work?
- Source basis: released HPL1 `cRenderer3D::RenderWorld(cWorld3D*, cCamera3D*, float)` adds frame time, configures logging, begins rendering, and executes Z, occlusion, light, diffuse, fog, skybox, transparent and debug passes. Overture VR Rework invokes this function once per eye and passes zero frame time to the extra eye.
- Binary evidence: both initialized images contain the same unique 32-byte function prefix. It loads the float at stack argument three, adds it to `this+0x10`, tests debug flag bit 3 at `this+0x36C`, and updates the two logging flags described by the source.
- Black Plague: function RVA `0x0012CB10`; its only direct call is at RVA `0x000EE010` inside the source-matching `cScene::Render` control flow.
- Requiem: function RVA `0x0012D110`; its only direct call is at RVA `0x000EDE90` inside the equivalent control flow.
- Calling convention: x86 `thiscall`; stack arguments are `cWorld3D*`, `cCamera3D*`, `float`; the function returns with `ret 0x0C`.
- Negative test: no other direct call to either build's function address appears in the initialized `.text` section.
- Result: confirmed static mappings and a strong stereo interception candidate for both manifests. Live hook installation, re-entry and teardown have not yet been validated, so the call site is not yet classified as safe.

### 2026-09-04 — Active camera and projection path

- Builds: the same exact Black Plague and Requiem hashes as the `RenderWorld` mapping above.
- `cScene::Render` loads `mpCurrentWorld3D` from `this+0x50` and `mpActiveCamera` from `this+0x64`, obtains `cRenderer3D` through `mpGraphics` at `this+0x20` then `graphics+0x18`, and passes those values to `RenderWorld`.
- `RenderWorld` passes the same camera pointer to `cRenderer3D::BeginRendering`: Black Plague RVA `0x0012A7F0`; Requiem RVA `0x0012ADD0`.
- `BeginRendering` invokes `cCamera3D::GetProjectionMatrix`: Black Plague RVA `0x00113D20`; Requiem RVA `0x001139C0`. It immediately passes the returned matrix to virtual `iLowLevelGraphics::SetMatrix(eMatrix_Projection, matrix)` at vtable offset `0x6C`.
- The camera getter returns the matrix at `camera+0x84`; its update flag is at `camera+0x8D2` and its infinite-far flag at `camera+0x8D0`. These offsets are evidence for these two builds, not a proposed cross-version ABI.
- Cross-build signatures: `BeginRendering` has one match per initialized image for the same 32-byte prefix; `GetProjectionMatrix` has one match per image for the same 25-byte prefix.
- The adjacent source-matching `cCamera3D::GetViewMatrix` is at Black Plague RVA `0x00113BE0` and Requiem RVA `0x00113880`. Both return `camera+0x44`, clear the dirty byte at `camera+0x8D1` after recomputation, and share a 45-byte prefix with exactly one match in each initialized image.
- Runtime correlation: the previously observed matrix at the OpenGL boundary is the exact 70-degree, 16:9, near-0.05 infinite projection generated by this getter's source-matching calculation.
- Live field correlation: a version-gated read of the Black Plague camera passed to `RenderWorld` returned an affine view matrix at `camera+0x44`, the active perspective matrix at `camera+0x84`, and flag bytes `[infiniteFar=1, viewUpdated=0, projectionUpdated=0]` at `camera+0x8D0`. The two matrices, after the source-matching `SetMatrix` transpose, matched the corresponding `glLoadMatrixf` telemetry element for element.
- Result: confirmed active-camera, view and projection chain, with live field correlation for Black Plague and static cross-build mapping for Requiem. A binary MVP can receive the camera as a `RenderWorld` argument and work from build-manifested matrix fields, avoiding an unsupported global camera singleton assumption.

### 2026-09-04 — Live `RenderWorld` call-site validation

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Hooked instruction: main-module RVA `0x000EE010`, expected bytes `E8 FB EA 03 00`, original target RVA `0x0012CB10`.
- Safety mechanism: exact-byte validation, temporary suspension of other process threads while replacing/restoring five bytes, rejection if a thread instruction pointer overlaps the patch, original-call forwarding and active-call quiescence.
- Host-independent tests: Debug and Release both reject mismatched bytes, intercept and forward two calls, restore the original instruction byte for byte, and verify that later calls bypass the removed hook.
- Live evidence: three attach/detach cycles in one gameplay process observed 1,676, 336 and 324 frames. Steady-state samples contained one `RenderWorld` call per frame with stable renderer/world/camera pointers and frame time around 0.016–0.018 seconds.
- Independent stack evidence: projection loads captured main-module returns `0x00560212` (`SetMatrix` after `glLoadMatrixf`) and `0x004EE015` (`cScene::Render` immediately after the mapped call site).
- Teardown evidence: all three cycles restored the call and left the same game process responsive immediately afterwards. Safe live DLL unloading was not established.
- Result: confirmed safe live hook boundary for this exact Black Plague build. This does not validate stereo re-entry yet.

### 2026-09-04 — OpenGL framebuffer capability at `RenderWorld`

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Question: does the validated world-render boundary execute with a current OpenGL context and a usable framebuffer-object API?
- Method: read-only state queries from the existing `RenderWorld` adapter; no framebuffer, texture or renderbuffer was created or bound.
- Live evidence: a 362-frame gameplay capture reported a current context at every sampled call, OpenGL version `4.6.0 NVIDIA 616.56`, viewport `[0, 0, 2560, 1440]`, and framebuffer binding `0`. A separate 304-frame capture reproduced the result after the gate was tightened to require all ten operations used by an eye target, including `glFramebufferRenderbuffer`.
- Driver limits: maximum texture size `32768`, maximum renderbuffer size `32768`, maximum viewport dimensions `[32768, 32768]`.
- Teardown evidence: the call-site instruction and imported OpenGL/SDL pointers were restored and the game remained responsive at the immediate post-deactivation check.
- Scope: version and numeric limits are observations of the test GPU/driver, not framework requirements. FBO allocation, completeness and GL-state restoration have not yet been tested.
- Result: confirmed that this exact build enters `RenderWorld` with the context and core API needed to attempt off-screen eye targets.

### 2026-09-04 — Host-side OpenGL eye-target lifecycle

- Question: can a reusable component allocate two RGBA8 plus 24-bit-depth/8-bit-stencil FBOs, resize them transactionally, and preserve caller GL state?
- Test context: hidden Win32 window with a real WGL context; the test executable is isolated from the fake `OPENGL32.dll` used by the import-hook test.
- Evidence: creation produced two distinct complete framebuffers; nested left/right bindings selected the expected framebuffer and viewport; unwind restored the preceding framebuffer and viewport at each level.
- State evidence: texture, framebuffer and viewport bindings survived creation, successful resize, rejected resize, rejected destruction and final destruction as specified.
- Failure evidence: zero-sized allocation was rejected without replacing the working targets; resize and destruction were rejected while an owned eye framebuffer was active.
- Configurations: Debug and Release passed with `/W4 /WX`.
- Scope: this proves the component against an ordinary WGL context. Persistent allocation and render-thread teardown inside Black Plague are not yet validated.
- Result: confirmed host-side component; not yet a confirmed game integration.

### 2026-09-04 — Transient eye targets in Black Plague

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Question: does the tested eye-target component behave correctly inside the game's actual render-thread context?
- Control path: an explicit launcher command invokes a probe export; the export posts an atomic request and waits while the existing `RenderWorld` adapter performs every GL operation on the render thread.
- Operations: create two complete RGBA8 plus depth24/stencil8 targets at `512x512`; bind/restore left; transactionally resize both to `640x480`; bind/restore right; destroy all six GL objects.
- State evidence: framebuffer, renderbuffer, active-unit 2D texture and viewport bindings were identical before and after validation.
- Lifecycle evidence: the command succeeded during a 172-frame attach/deactivate cycle, all GL objects and hooks were restored, and the game remained responsive at the immediate check. This predated the corrected resident-DLL policy.
- Negative scope: the targets were not retained across frames, used to render the world, or submitted to OpenVR.
- Result: confirmed transient in-game object lifecycle for this exact build. Persistent render-thread ownership and teardown remain open.

### 2026-09-04 — Persistent eye-target ownership and teardown

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Question: can eye FBOs remain alive across frames and be destroyed on the owning OpenGL thread before hook removal?
- Creation: an explicit request created a `512x512` left/right pair in `RenderWorld`; framebuffer, renderbuffer, texture and viewport state were restored immediately afterwards.
- Lifetime: the pair remained allocated for 363 `RenderWorld` calls. Frame 300 of the 390-frame probe cycle still reported the targets active, with no change to the observed default framebuffer at world entry.
- Teardown order: shutdown posted a destroy request while the world hook remained active, waited for render-thread completion, then restored the SDL swap import, the `RenderWorld` call instruction and the OpenGL telemetry imports.
- Immediate result: all six GL objects were destroyed, incoming GL state was restored, and the game remained responsive at the command's immediate check.
- Negative scope: fixed diagnostic dimensions were used; no world rendering or OpenVR submission targeted these FBOs.
- Result: GL allocation and render-thread destruction confirmed, but the overall lifecycle is not confirmed because a delayed unloaded-DLL crash followed this cycle.

### 2026-09-04 — Delayed crash after live DLL unload

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Observation: Windows Error Reporting recorded the process failure at `04:00:26`, roughly three seconds after the persistent-target detach command had reported success.
- Failure: exception `0xC0000005`, execute violation, fault module `PenumbraVR.BlackPlague.Probe.dll_unloaded`, module-relative offset `0x0001E0F0`.
- Interpretation: an already-dispatched callback could outlive IAT restoration; the callback also read its original target from hook state that removal cleared. Immediate process responsiveness was insufficient teardown evidence.
- Fix: original targets are published to stable atomics before patching; SDL and OpenGL adapters count and quiesce active calls; the `RenderWorld` adapter retains its original target; deactivation restores hooks but intentionally leaves the DLL resident until process exit.
- Regression evidence: the corrected build completed two attach/deactivate cycles of 122 menu frames each in one process, reused the resident module, then remained responsive for a 12-second observation window with zero new Penumbra WER events. It subsequently completed two gameplay cycles with persistent targets alive for 420 and 422 `RenderWorld` calls. Each cycle destroyed both targets on the render thread with GL state restored, deactivated the hooks, survived a further 12-second observation window and produced zero matching WER events. A blocking host-side SDL test independently proves removal waits for an in-flight callback and that the callback still forwards to the original function.
- Scope: persistent gameplay target teardown must be repeated under the corrected policy.
- Result: unsafe live unloading rejected; resident-DLL deactivation and persistent-target teardown confirmed for the tested menu and gameplay cycles.

### 2026-09-04 — OpenVR loader and render-size smoke tests

- Runtime input: the locally installed SteamVR runtime through OpenVR SDK `2.15.6`'s Win32 `openvr_api.dll`.
- Method: a standalone smoke-test path dynamically loaded the API, requested a scene application and would have queried the recommended per-eye render size before shutting down.
- Startup evidence: the first attempt caused SteamVR to start but returned `VRInitError_Init_HmdNotFound` (`108`) before `vrserver` and `vrcompositor` were ready. The test reported the error and exited without retaining an OpenVR session.
- Success evidence: after `vrserver`, `vrcompositor` and `vrmonitor` were active and the PSVR2 was recognized, the same executable initialized successfully and reported a recommended per-eye target of `4164x4244`.
- Optical evidence: the runtime reported left-eye tangents `[-1.84177, 0.947191, -1.32898, 1.32898]`, right-eye tangents `[-0.947191, 1.84177, -1.32898, 1.32898]` and eye-to-head X translations of `-0.032` and `+0.032` metres.
- Tracking evidence: one compositor `WaitGetPoses` sample returned the HMD as connected, pose-valid and `TrackingResult_Running_OK` (`200`).
- Result: real-runtime dynamic initialization, HMD render-size discovery, per-eye optical geometry, live headset pose and explicit shutdown are confirmed through game-neutral data types in the standalone path. Creation of targets at those dimensions inside Black Plague remains a separate live test.

### 2026-09-04 — OpenVR-sized targets inside Black Plague

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Runtime evidence: the OpenVR scene session initialized inside the injected probe and reported `3400x3468` per eye. The earlier standalone `4164x4244` result is retained separately because the runtime recommendation was not assumed to be globally fixed.
- GL evidence: two complete RGBA8 plus depth24/stencil8 targets were created at `3400x3468`, with the incoming framebuffer, renderbuffer, texture and viewport state restored. They remained active for 423 `RenderWorld` calls.
- Teardown evidence: both targets were destroyed on the render thread before OpenVR shutdown and hook restoration. The process remained responsive for 12 seconds and no matching Application Error or WER event appeared.
- Result: runtime-sized in-game target lifecycle confirmed; no world image was rendered into or submitted from these targets.

### 2026-09-04 — Controlled duplicate world rendering

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Method: create a `512x512` diagnostic pair, bind one eye FBO, call the already-validated original `RenderWorld` target with zero frame time, restore framebuffer and viewport, then execute the untouched normal desktop call.
- Evidence: 120 requested extra passes completed. Steady frames retained one intercepted scene call while projection loads rose from 1 to 2 and model-view loads from 35 to 68, independently confirming two renderer executions. The direct extra call also appeared as an additional return address in the projection stack.
- Cleanup: targets were destroyed after 121 active frames with GL state restored; hooks were deactivated under the resident-DLL policy. The game remained responsive after 12 seconds with zero matching WER events.
- Result: a second HPL world pass is viable at the chosen hook boundary without camera mutation. Per-eye matrices and compositor submission remain untested.

### 2026-09-04 — Reversible per-eye matrices and static stereo presentation

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Matrix evidence: a controlled command used OpenVR projection tangents and eye-to-head transforms to render 60 stereo pairs into separate `512x512` targets. The observed horizontal projection terms were `[0.717113, -0.320757]` and `[0.717113, 0.320757]`, with eye translations of `-0.0315 m` and `+0.0315 m`. All 120 extra passes restored the camera bytes, framebuffer and viewport before the normal desktop pass.
- Compositor evidence: a subsequent command completed three 300-frame cycles. All 900 frames acquired a valid HMD pose for frame timing, rendered and submitted both OpenGL textures without compositor errors, and restored the camera after each eye.
- Headset evidence: the user observed native full-view stereo rather than SteamVR's cinema screen. The image was visibly low resolution as expected from the deliberately bounded diagnostic targets; the pose was not yet applied to the view.
- Result: asymmetric per-eye rendering and native compositor presentation confirmed for this build. Head tracking remained separate work at this point.

### 2026-09-04 — Rejected full-orientation tracking anchor

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Method: use the first compositor pose as a complete orientation anchor, apply the inverse relative HMD rotation before the game view, keep translation disabled, and submit two `512x512` eye textures.
- Technical evidence: seven 300-frame cycles completed, for 2,100 tracked stereo frames. Every frame reported a valid anchor and HMD pose; both eyes were submitted, the camera was restored after every eye, no compositor failure occurred, and the process remained responsive.
- Headset evidence: the user reported being positioned as though standing on a wall and leaning forwards. The presentation was therefore visually invalid despite clean telemetry.
- Diagnosis from the Overture reference: `rubocopter/penumbra_vr_rework@23c890f` preserves the raw OpenVR pitch and roll and owns world placement separately; recentering changes only horizontal yaw. A complete pose anchor can incorrectly preserve the orientation of a visor that is being handled while the user changes windows.
- Corrective direction: align only the anchor's horizontal heading with the current game camera, reject a near-vertical anchor, preserve current runtime pitch and roll, and keep translation at zero for the next bounded validation.
- Result: full-orientation-relative anchoring rejected. It is not evidence of working head tracking.

### 2026-09-04 — Yaw-aligned rotational tracking from the Overture reference

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Source behavior: adapted the yaw-only tracking/world alignment from `rubocopter/penumbra_vr_rework@23c890f`, preserving raw OpenVR pitch and roll while keeping game placement separate. Provenance is recorded in `THIRD_PARTY.md`.
- Guard: an initial pose whose forward direction is nearly vertical is rejected, preventing a visor being handled flat from becoming a full 3D world anchor.
- Automated evidence: the new composition tests cover yaw alignment, pitch preservation, rotation-only translation suppression and vertical-anchor rejection. All seven project tests passed in Debug, Release and the no-OpenVR Release configuration.
- Live evidence: six consecutive cycles submitted 1,800 tracked stereo frames. Every frame used a valid pose and anchor; there were no compositor errors or camera-restoration failures, and the game remained responsive.
- Headset evidence: the user confirmed that the world orientation was now correct and that rotational tracking behaved correctly. The image was low resolution by design. It returned briefly between cycles because each bounded 300-frame command shut down and reinitialized OpenVR.
- Result: yaw-aligned rotation-only tracking confirmed for this exact Black Plague build. Positional tracking, continuous session ownership and production resolution remain unvalidated.

### 2026-09-04 — Continuous adaptive presentation implemented

- Implementation: `--start-vr` retains one OpenVR session and yaw-aligned rotational tracking until `--stop-vr` or `--detach`. Eye targets begin at SteamVR's recommended per-eye dimensions at scale 1.0 and retry progressively smaller proportional sizes down to a 512-pixel minimum dimension if allocation fails.
- Host evidence: the OpenVR and no-OpenVR configurations compile with warnings treated as errors, and the component test suite passes. These tests do not simulate the complete injected process lifetime or establish headset behavior.
- Scope: translation remains disabled, the normal desktop pass remains present and the research DLL stays resident after hook deactivation.
- Result: continuous presentation is implemented and build-verified, but runtime-derived resolution, longer gameplay, keyboard/mouse preservation, explicit stop and normal game exit still require one in-headset validation session.

### 2026-09-04 — Continuous full-resolution runtime validation

- Game/build SHA-256: `FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`.
- Runtime: SteamVR 2.17.8 and OpenVR SDK 2.15.6 with a PS VR2 reported `3400x3468` per eye. The first allocation succeeded, so no proportional fallback was used.
- Probe evidence: continuous mode submitted 6,847 yaw-aligned rotation-tracked stereo frames during 120.6 seconds. Every sampled active frame had a valid HMD pose and tracking anchor, two eye passes, restored camera state and no stereo error. Two changes of the observed world pointer exercised more than one renderer-world lifetime.
- Teardown and exit: explicit stop destroyed both targets on the render thread and restored incoming GL state. Detach removed the hooks after 6,990 observed frames while keeping the DLL resident. The process remained responsive for more than 23 seconds, later exited, and produced no matching Application Error or Windows Error Reporting event from launch through exit. Probe log SHA-256: `A996CE132A461EE439D3A0CD8D6F04F4189F37E602121FF0ACF4289C0C3D01F5`.
- Frame pacing: SteamVR reported 10,839 presents, two dropped and 3,467 reprojected frames, or 31.99% reprojection at a 90 Hz target. Cumulative application time was 16.160 ms CPU and 14.721 ms GPU; sampled game frames commonly followed an approximately 60 Hz cadence.
- Result: runtime-sized continuous start, presentation, world changes, stop and teardown are technically confirmed. The reprojection rate requires investigation; user-visible behavior is recorded separately below.

### 2026-09-04 — Continuous-session user feedback

- Input: the user reported that keyboard and mouse appeared functional throughout the test.
- Overall presentation: the full-resolution image generally looked good.
- Visibility defect: walls and other room geometry could be absent when exposed by an HMD turn, then appear after the player moved the game camera towards that area. Some nearby objects also popped in and out.
- Root-cause evidence: source-level HPL1 calls `cScene::UpdateRenderList` separately from and before `cScene::Render`. The initialized Black Plague image contains the matching scene wrapper at RVA `0x000EDF50`, its only renderer update call at RVA `0x000EDF84`, and the source-matching `cRenderer3D::UpdateRenderList` target at RVA `0x0012A8F0`; each selected signature occurs once in `.text`. The target clears and compiles the render list from `camera->GetFrustum()`. The current hook applies tracked per-eye matrices only later at RVA `0x000EE010`, so the submitted eyes consume visibility prepared from the game camera.
- Enhanced visuals: this run used the stock Black Plague lighting path. The imported Rework v4 CPU calibration is test-only; HDR/MSAA targets, final tonemapping, enhanced light/material shaders, sharpening and glowstick-halo integration are not active in the binary backend.
- Implementation: the exact update call is now hooked only during continuous tracked presentation. Starting with the second tracked frame, the original update receives the latest valid HMD orientation applied to the current game-camera view and a symmetric cull frustum containing both asymmetric eyes plus a five-degree guard. A shared transaction restores matrices, FOV, aspect and dirty flags after the call; failures fall back to the untouched update and are reported in frame telemetry.
- Host verification: the conservative-frustum math and byte-exact visibility-camera transaction have positive and rejection coverage. OpenVR Release, OpenVR Debug and no-OpenVR Release builds each pass all 12 tests.
- Result: keyboard/mouse preservation is provisionally validated and the corrective visibility hook is implemented. Continuous visual acceptance remains open pending live hook validation and another headset run.

### 2026-09-04 — HMD-aware visibility runtime validation

- Runtime: the exact Black Plague build ran on PS VR2 at SteamVR's `3400x3468` per-eye recommendation for 110.282 seconds.
- Probe evidence: 6,570 tracked stereo frames were submitted. Every sampled active frame reported one HMD-aware render-list update, zero visibility failures, exact visibility-camera restoration, two completed eye passes, a valid pose and no stereo error.
- Teardown: explicit stop destroyed the targets on the render thread after 6,571 target frames; detach restored both exact-build call sites after 6,745 observed probe frames. The game remained responsive and no matching Application Error or Windows Error Reporting event was present after deactivation.
- Frame pacing: SteamVR reported 9,913 presents, zero dropped and 3,286 reprojected frames, or 33.15% reprojection at a 90 Hz target. Cumulative application time was 16.442 ms CPU and 15.723 ms GPU. Different scene content prevents attributing the change from the prior run solely to the wider visibility set, but the result does not improve the existing pacing concern.
- Probe log SHA-256: `AC7EECBE3E8C5228709BC1E21695821E816B5895D6B96811AACAFEEF3A03C69E`.
- Headset result: the user reported that everything looked correct. Missing walls on HMD turns and nearby-object popping did not recur, so no separate occlusion-query defect was exposed by this run.
- Result: the HMD-aware update hook, fallback telemetry, restoration path and intended visual correction are live-confirmed. Frame pacing remains a separate open problem.

### 2026-09-04 — Rework-derived mirror policy and logical input extraction

- Motivation: the validated continuous path rendered the world once per eye and once again for the desktop while SteamVR measured 33.15% reprojection. Overture Rework already makes that third pass optional through its `RenderToMonitor` setting and advances frame time on the first eye when it is disabled.
- Stereo implementation: `src/runtime/stereo_render_policy.*` now defines frame-time ownership and world-pass count independently of Black Plague. Continuous presentation defaults to two eye passes; the optional mirror retains the original desktop pass. Bounded diagnostics keep their previous three-pass behavior. A partial failure after the timed first eye falls back to the desktop with zero frame time.
- Control and evidence: `--vr-mirror-on` and `--vr-mirror-off` call exported probe controls, and telemetry distinguishes mirrored, suppressed-monitor and eye-timed frames. The policy and both hook configurations compile with warnings as errors.
- Input extraction: `src/runtime/vr_input_state.*` adapts Rework's device-independent intents, radial dead zone, context/handedness edge suppression, pointer/interact pose-loss release and 500 ms all-actions-idle grace period. It deliberately does not poll OpenVR or inject input into Black Plague yet.
- Host verification: all 14 tests pass in OpenVR Release, OpenVR Debug and no-OpenVR Release. The new tests also cover inactive analog cleanup and a non-monotonic supplied clock.
- Validation boundary: the game and SteamVR were closed for this work. No claim is made yet about headset presentation, desktop mirror contents, reprojection improvement or controller behavior.

### 2026-09-04 — Live two-pass schedule, mirror toggle and 60 Hz cap diagnosis

- Runtime: continuous Black Plague presentation started at SteamVR's
  `3400x3468` recommendation with the monitor mirror disabled.
- Two-pass evidence: sampled active frames contained exactly two completed eye
  passes, zero monitor world passes, one suppressed original pass and first-eye
  frame-time ownership. Poses remained valid and camera restoration and stereo
  error counts remained clean.
- Mirror evidence: enabling the mirror at runtime changed the same session to
  two eye passes plus one separately timed monitor world pass. The probe
  reported the mirror flag and expected ownership transition with no hook or
  restoration failure. The user had needed the desktop view to operate options;
  explicit confirmation of the displayed mirror contents was not recorded.
- Frame-cap evidence: although `LimitFPS=false` was written to the game config
  while Black Plague was running, telemetry remained at exactly 300 frames per
  five seconds. The engine does not hot-reload that value and wrote its live
  `true` state back on exit. Overture Rework independently forces
  `SetLimitFPS(false)` during initialization.
- Static follow-up: the captured initialized Black Plague image contains
  `cGame::Run` at RVA `0x000C89F0` and two unique checks of the limit byte at
  object offset `0x28`, beginning at RVAs `0x000C8B8A` and `0x000C8BA2`.
  This is evidence for a future exact-build reversible override, not permission
  to patch the installed executable.
- Result: both scheduling branches are live telemetry-validated. Comparable
  SteamVR timing and an explicit monitor-image check remain open. The game was
  stopped and exited normally before configuration work continued.

### 2026-09-04 — Trilogy VR configuration baseline and shared settings model

- Local configuration: with all games and SteamVR closed, the Overture Rework,
  Black Plague and Requiem `settings.cfg` files were normalized to the documented
  VR baseline. Each original received a same-directory backup before modification.
  Unrelated controls, saves, calibration and game preferences were preserved.
- Comfort and pacing policy: all three profiles now disable the legacy 60 FPS
  cap, desktop VSync, window FSAA, motion blur, depth of field and noise filtering.
  Physics remains at 60 updates per second. Existing high-quality textures and
  atmospheric effects were retained; Overture's Enhanced visuals, render scale,
  HRTF and personal VR calibration were not replaced.
- Shared implementation: `src/runtime/vr_settings.*` adapts Rework's defaults,
  numeric limits, canonical enum text and version-0 smooth-turn migration without
  HPL or OpenVR types. Non-finite numbers and invalid enum states fail to safe
  defaults. The framework adds monitor-mirror state while leaving storage and
  UI ownership to adapters/backends.
- Integration: render-target scale and input dead-zone policy now consume the
  shared setting limits instead of duplicating constants.
- Verification: all 15 host-independent tests pass in OpenVR Release, OpenVR
  Debug and no-OpenVR Release with warnings treated as errors. Metadata also
  passes under PowerShell 7 and Windows PowerShell 5.1.
- Validation boundary: the settings model is not yet loaded by the Black Plague
  or Requiem binary backend, and the adjusted local files have not yet received
  a new headset performance run.

### 2026-09-04 — Persistent Black Plague monitor-mirror preference

- Storage: `src/launcher/vr_settings_store.*` owns a narrowly scoped Windows INI
  adapter at `%LOCALAPPDATA%\PenumbraVR\settings.ini`. Missing storage defaults
  to mirror off; malformed recognized values fail with an explicit error.
- Launcher behavior: a successful `--vr-mirror-on` or `--vr-mirror-off` call
  persists the new state. `--start-vr` loads and applies it before starting the
  continuous presentation, including an explicit reset to the safe off default.
- Local baseline: the current user profile was created with
  `VR/MonitorMirror=false`, matching the two-pass performance default.
- Verification: the new file-store test covers missing files, directory
  creation, true/false round trips, malformed input and the default path. All 16
  host-independent tests pass in OpenVR Release, OpenVR Debug and no-OpenVR
  Release with warnings treated as errors.
- Validation boundary: persistence has not yet been exercised against a live
  injected process; the remaining shared settings still lack storage and
  backend application.

### 2026-09-05 — Lamp scissor correction prepared after the 90 Hz session

- Live evidence before this change: PID 15184 rendered at desktop viewport
  2560x1440 and eye targets 3400x3468 with mirror enabled, two eye world passes,
  one desktop world pass, valid poses and no reported stereo/restoration errors.
  Sampled 300-frame intervals of approximately 3.337 seconds show about 90
  frame submissions per second after the config frame-cap change. This is not
  a measurement of compositor reprojection or proof of stable pacing everywhere.
- User report: near a lamp, head rotation is safe; at medium distance, looking
  up/down removes illumination on the environment while the bulb remains lit.
- Rework comparison at revision `23c890f7dbd06b939be9951d282e6e948d9a6623`:
  `LowLevelGraphicsSDL::GetScreenSize` returns VR dimensions while VR is enabled.
  Point/spot `CreateClipRect` consume that size. `cMath::GetClipRectFromBV`
  bypasses scissoring near/intersecting the light volume. The binary backend
  lacked this dimension adaptation, matching the distance-dependent symptom.
- Binary corroboration from `artifacts/black-plague-22000-live.bin`: the
  `glScissor` IAT slot is VA `0x00672638`; its sole direct IAT call is at RVA
  `0x0015DE3A`, inside the wrapper beginning at RVA `0x0015DE20`. The wrapper
  forwards rectangle X/W/H and computes Y as `[this+8] - rect.y - rect.h - 1`.
  The installed executable also imports `OPENGL32.dll!glScissor`. These offsets
  are research observations, not new runtime patches or layout assumptions.
- Implementation: reversible main-image IAT hook, thread-local per-eye scope,
  actual context/FBO/viewport checks, outwards-rounded and clamped rescaling
  with a one-source-pixel guard. Invalid inputs pass through and empty bounds
  stay empty. Original GL entry remains published for late calls after removal.
- Eye bindings now save/restore the scissor box and enable flag, and disable
  inherited clipping before the eye world clear. Scope ends before restoration
  so desktop coordinates are never scaled on the way back.
- Diagnostics: `eye_scissor_remapped` / `eye_scissor_bypassed` counts per frame.
- Verification: all 17 tests pass in OpenVR Release, OpenVR Debug and no-OpenVR
  Release. Real-WGL tests inspect colored framebuffer pixels, IAT removal and
  reinstallation, nested scopes, intermediate viewport/FBO bypass and exact
  restoration. Metadata validation passes. No game was launched for this batch.
- Boundary: ready for the focused checklist in `VR_LIGHTING_VALIDATION.md`, not
  headset-validated. Scissor query cost, menus, flashlight and both mirror modes
  require the next live session. No new tonemapping/shader integration; Rework
  remains untouched and the installed game executables are unchanged.

### 2026-09-12 — Physical displacement live gate and mirror-off confirmation

- Live evidence: Black Plague PID 26144 activated the transient physical
  displacement validation path with `positional_translation_enabled=0` and
  retained the native `dt=0.016667` body tick.
- Boundary evidence: the fresh log records non-zero plans queued, requests
  consumed/injected at exact-build RVA `0xD7281`, and matched reconciliation for
  the current body/generation. Re-analysis yields 37 free, 2 blocked and 15
  slide/partial samples.
- Stationary classifier correction: the first helper required no physical
  injection at all, but Rework `23c890f` intentionally processes any non-zero
  HMD delta and a real headset continuously produces small tracking jitter. The
  classifier now uses the shared `0.002 m` rejection-significance boundary for
  stationary/jitter and finds 12 valid samples in PID 26144. This changes only
  evidence classification; runtime movement policy is unchanged.
- Presentation evidence: the user visually confirmed that with monitor mirror
  disabled the desktop remained black and the prior growing white-point
  artifact did not return. Mirror-on contents and Alt+Tab/focus-loss menu
  behavior remain separate pending checks.
- Result: the `0xD7281` request boundary advances to `live-tested`. Active
  room-scale/positional HMD translation remains disabled and requires its own
  headset-validation gate through this boundary.

### 2026-09-12 — Active room-scale gate prepared

- Mirror evidence: PID 19192 confirmed that mirror-off suppresses the gameplay
  world to black while native 2D menus remain visible. Telemetry shows
  `monitor_mirror=0`, zero monitor world passes and one suppressed world pass
  during gameplay; menu frames have no `RenderWorld` pass to suppress.
- Implementation: a separate transient room-scale request is accepted only
  while physical-displacement validation is also active. The adapter publishes
  the current body generation plus the reconciled
  `predicted_anchor - body_after` X/Z offset. Invalid, stale (>250 ms),
  mismatched or queue-failed samples are not applied.
- Rendering: the offset is applied to yaw-recentered head view, conservative
  visibility view and controller game-view basis. Raw HMD translation is not
  applied again, Y/jump remain native and the existing body tick remains the
  sole owner.
- Validation tooling: `Start-BlackPlagueRoomScaleValidation.ps1` persists
  mirror on, holds both transient requests and requires active camera offset,
  mirror, the native `1.65 -> 0.95 -> 1.65 m` crouch shape sequence and fresh
  free/block/slide evidence after standing again.
- State: `implemented`. Affected Release targets compile, but no project binary
  or test executable was run during this pass; host/live/headset evidence
  remains pending.

### 2026-09-13 — Active room-scale first headset run and carry correction

- PID 24956 produced 1159 body summaries and captured 90 stationary/jitter, 963
  free, 10 blocked and 95 slide/partial classifications. It also proved the
  native `1.65 -> 0.95 -> 1.65 m` shape sequence, room-scale camera recovery and
  mirror-on gameplay frames.
- The helper failed only because no second blocked sample appeared after standing
  recovery; 414 free and 25 slide/partial samples did. The redundant repetition
  was replaced with a requirement for meaningful post-recovery movement while
  retaining all four global outcome gates.
- The qualitative failure was character walking during in-place head tilt;
  stick then added to or opposed that unintended motion. Comparison with Rework
  `23c890f` found a concrete sequence difference that can amplify it. Rework
  reconciles physical movement in one update and carries the anchor with a later
  stick-only update. Black Plague's one owned tick combines both, and the shadow
  had reused its physical accepted component in locomotion carry after
  reconciliation.
- The adapter now subtracts matched physical X/Z from locomotion carry, preserves
  actual whole-tick `body_after` for camera space and logs `locomotion_carry`.
  Both Release configurations compile. No test executable or new live/headset
  session was run, so the correction remains `implemented` and is not yet
  claimed to resolve tilt-induced walking.

### 2026-09-13 — Active room-scale technical pass and presentation correction

- PID 21548 passed the complete helper after the carry correction:
  `queued=201 consumed=201 injected=201 matched=201`, scenario counts 92
  stationary, 477 free, 1 blocked and 45 slide/partial, mirror on, native
  `1.65 -> 0.95 -> 1.65 m` crouch/stand and post-crouch recovery.
- Headset evidence: in-place head tilt no longer produced character locomotion.
  The technical pass did not pass the visual gate because the world continuously
  shook; wall rejection also remained aggressive.
- Log analysis: 625 room-scale frame summaries were sampled. For 539 stick-zero
  samples, reconciled offset magnitude had a 9.65 mm median and its logged
  sample-to-sample change had a 15.77 mm median; 430 windows reversed at least
  one horizontal direction. Stable-HMD sampled intervals still reached a 60.08
  mm offset jump. Only 4/64 periodic reconciliation summaries contained a
  non-zero physical rejection, so continuous shake is not explained by wall
  rejection alone.
- Rework comparison: revision `23c890f` runs tracking/player placement at 90
  updates/s, renders from its VR player-world anchor and does not retain native
  character-camera smoothing or camera-position additions. The Black Plague
  adapter updated its reconciled offset on the native ~60 Hz body tick and added
  it to the stock camera.
- Correction: each reconciled body sample now retains its observed tracking
  pose. Render computes the new HMD delta since that sample, transforms it with
  the same yaw policy, and places camera X/Z directly at the resulting absolute
  head anchor. Visibility and controller space use the same translation.
  Existing 0.8 m discontinuity handling fails closed, while Y/jump, collision
  reconciliation, hooks and single native tick remain unchanged. Telemetry now
  exposes source offset, render prediction, applied correction and final anchor.
- State: both Release configurations compile. No project test executable, game,
  SteamVR or headset was run for the correction; it remains `implemented`.
