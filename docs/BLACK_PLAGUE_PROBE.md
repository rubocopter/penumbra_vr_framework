# Black Plague frame probe

## Scope

The probe proves a narrow integration path against this exact executable:

```text
SHA-256: FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF
Machine: x86
Build ID: black-plague-steam-observed
```

It is not a VR backend. It neither changes rendering nor initializes OpenVR.

## Loading model

Launching this executable directly on the development machine exits voluntarily with code 0 after roughly 380 ms. Launching Steam AppID `22120` creates the persistent game process. For that reason, the verified workflow is currently:

1. Launch Black Plague through Steam.
2. Locate the resulting process and obtain its executable path.
3. Recompute and whitelist-check its SHA-256.
4. Load the probe DLL with a remote `LoadLibraryW` call.
5. Call the exported `PenumbraVR_Initialize` function explicitly.

No substantial work is performed from `DllMain`; it only disables thread attach/detach notifications.

The direct-path launcher mode remains useful for diagnosing non-Steam builds, but it is not the verified route for this Steam installation.

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

Installation fails closed unless all five call bytes match. Other process threads are briefly suspended while the five-byte instruction is replaced or restored, and installation is aborted if a thread is currently executing inside that instruction. This call-site mechanism has host-independent Debug and Release coverage and has completed three live attach/detach cycles for the exact supported hash.

## Teardown

The current research build also observes the imported `glMatrixMode`, `glLoadMatrixf` and `glOrtho` calls. These hooks collect per-frame counters, the most recent projection matrix, a short projection call stack, and a bounded frequency table of model-view matrices. The table identifies the most frequently loaded model-view matrix without assuming a game address. All OpenGL arguments are forwarded unchanged.

`PenumbraVR_Shutdown` restores the SDL import first, restores the `RenderWorld` call, waits for any already-active adapter invocation to finish, and then restores the OpenGL imports before closing the log. The launcher waits for shutdown to finish and only then invokes `FreeLibrary` in the target process. If any expected pointer or instruction differs, or an active call does not quiesce, the DLL is not unloaded.

## Verification performed

On 2026-09-03, a Release build was attached and detached three times in the same Steam-launched process. Each cycle:

- revalidated the executable hash inside the game
- observed at least one real swap
- restored the original import
- unloaded the probe
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

This confirms a reliable API-level view-matrix signal. It does not yet establish the address or layout of the owning HPL camera object.

On 2026-09-04, the `RenderWorld` call-site probe completed three more attach/detach cycles in one live gameplay process, observing 1,676, 336 and 324 frames. Every sampled steady-state frame contained exactly one forwarded `RenderWorld` call. Renderer, world and camera pointers remained stable across the cycles, and frame time tracked the observed 60 Hz cadence at roughly 0.016–0.018 seconds.

The projection call-stack capture independently returned `0x00560212` followed by `0x004EE015`: the return from the mapped `SetMatrix` implementation and the instruction immediately after the mapped `cScene::Render` call site. Each detach restored the original five bytes, unloaded the probe and left the game responding. Result: the call site is a verified live hook boundary for this exact executable hash.

A 362-frame gameplay capture queried the OpenGL state at entry to that boundary without changing it. After tightening the capability gate to require all ten FBO operations, including depth/stencil attachment, a separate 304-frame capture reproduced the result. A current OpenGL context was present on every sampled call. The NVIDIA driver exposed OpenGL `4.6.0 NVIDIA 616.56` and the complete required core framebuffer-object API. The observed state and implementation limits were:

```text
viewport:                  [0, 0, 2560, 1440]
framebuffer binding:       0
maximum texture size:      32768
maximum renderbuffer size: 32768
maximum viewport:          [32768, 32768]
```

These limits describe the test machine, not minimum requirements for Penumbra VR. The relevant conclusion is that `RenderWorld` runs with a current context, enters through the default framebuffer on this build, and provides the framebuffer API needed for reversible off-screen eye targets. The probe did not create or bind an FBO during these captures.

The test game process did not exit in response to a normal window-close request after verification and was therefore explicitly stopped. This does not count as successful launch/play/exit validation, which remains open on the roadmap.

## Commands

```powershell
# Attach to a Steam-launched process
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --attach <pid>

# Restore the import and unload the probe
.\build\bin\Release\PenumbraVR.ProbeLauncher.exe --detach <pid>
```

Logs are stored under `%LOCALAPPDATA%\PenumbraVR\logs` and include the host path, SHA-256, build ID, frame telemetry and shutdown count.

## Next question

Static analysis maps `cRenderer3D::RenderWorld` to RVA `0x0012CB10` and its sole direct call in `cScene::Render` to RVA `0x000EE010`. The call passes `mpActiveCamera` from `cScene+0x64`; `RenderWorld` forwards it to `BeginRendering` at RVA `0x0012A7F0`, which obtains the projection through `cCamera3D::GetProjectionMatrix` at RVA `0x00113D20`. Live validation confirms this boundary and a usable current-context FBO path. The next step is to implement a state-preserving eye-target abstraction, test it outside the game, and only then use this single intercepted call to issue controlled world passes while leaving later UI work outside the duplicated region.
