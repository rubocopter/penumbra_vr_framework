# Binary research log

Use this log for conclusions that have been reproduced against an exact executable hash. Do not record guesses as resolved addresses.

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
- Runtime correlation: the previously observed matrix at the OpenGL boundary is the exact 70-degree, 16:9, near-0.05 infinite projection generated by this getter's source-matching calculation.
- Result: confirmed static active-camera/projection chain. A binary MVP can receive the camera as a `RenderWorld` argument and replace only the projection sent to OpenGL, avoiding an unsupported global camera singleton assumption.

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
- Result: real-runtime dynamic initialization, HMD render-size discovery and explicit shutdown are confirmed in the standalone path. Creation of targets at those dimensions inside Black Plague remains a separate live test.
