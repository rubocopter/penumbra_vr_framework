# Experimental Neural companion

The v1.0.15 Setup bundles this x64 helper and the MIT DirectNR forwarder.
After installing VR, use **Maintenance → Import Neural (experimental)** and
select your local `nvngx_dlssnr.dll`. Select installed games first. The NVIDIA
model is not bundled/downloaded. Requirements and source guidance:
https://github.com/lowpoplabs/nr-bridge#prerequisites (consult NVIDIA's terms).
The import preserves the previous INI and does not enable Neural by default.
Use **Remove Neural** to restore that configuration. Full root removal also
removes owned Neural support; modified owned files block removal to avoid loss.
CLI: `tools/Install-PenumbraNeural.ps1 -GamePath <game> -ModelPath <model>`;
use `-Plan` for preflight or `-Restore` without ModelPath for removal.
Missing helper/model or unsupported initialization keeps original stereo and
reports Unavailable. Inference/image quality remains experimental.

Build separately from the x86 Framework:

```powershell
cmake -S tools/neural_post -B build/neural-host -A x64
cmake --build build/neural-host --config Release
```
No NVIDIA binary is included or downloaded. Supply your local NGX runtime in
`NeuralPost.ini` beside each game executable (or `PVR_NEURAL_POST_INI`):

```ini
[Neural]
HostPath=C:\your-game\redist\PenumbraNeuralHost.exe
RuntimeDirectory=C:\your-game\redist\NeuralRuntime
TimeoutMs=20
```

The game-specific VR profile and Neural page own enabled/tuning preferences.
The INI supplies machine-local deployment paths; it is not written by menu edits.
Without valid runtime paths activation is unavailable and original stereo stays
visible. Local NGX currently requires `nvngx_dlssnr.dll` and its matching
`nvngx.dll_nr_bridge.dll` forwarder. The settings page cannot install the runtime.
Open **VR Settings → Neural** in any game to edit all nine controls.
Use the visible **−** and **+** buttons with the controller trigger to decrease
or increase a value; the label also retains mouse left/right editing. Rebuilding
the model temporarily shows original stereo while the status is Starting or
Warming up. Off immediately restores originals; a new enable retries a failed
session. A single missed frame deadline reports **Recovering** and submits the
original pair until late output is safely discarded; a fresh frame can resume
without toggling the setting. An actual worker fault or a two-second stall
still bypasses the session. Raising TimeoutMs can worsen headset latency and
does not improve model quality. BP and Requiem use separate Neural sections in their existing shared
profile. Setup's optional import owns deployment; a source build alone is not
automatically copied to an installed game.
Developer manual deployment may also use a game-local helper/runtime and an
explicit NeuralPost.ini. Verify configuration and status from inside the game;
standalone GPU probes do not establish headset performance or image quality.

Internal height is the transport proxy, not the headset render resolution.
Processing scale further reduces that proxy: the new default height 896 at 60%
evaluates an approximately 538-pixel-high central tier when the central region
is 100%.
Increasing either height or scale adds cost; additional passes run the model
again and do not necessarily improve the result. The default 90% intensity is
aggressive; reduce it if the model overwhelms original detail. This is a
generative appearance edit with
estimated camera rotation and constant depth, so neither maximum settings nor
successful inference establish better image quality in a particular scene.

For explicit host verification, build `pvr_neural_post_roundtrip` in the x86
Framework and run its executable with an absolute INI path and mode `live`,
`model`, `zero`, `identity`, `detail`, `detail_srgb`, `detail_edit`, `detail_srgb_edit`, `aspect`, `timeout`
or `glfailure`. Identity/detail/aspect require
`DiagnosticMode=3`; model/zero use 0 (zero intensity for zero); timeout uses a
100 ms diagnostic delay with `DiagnosticDelayCount=1` and a 50 ms frame deadline.
`detail_edit` and `detail_srgb_edit` use diagnostic 4. Detail cases use a 64-pixel stage and a
256-pixel full eye with one-pixel bars: identity must retain every original
byte, while an actual edit must retain edge contrast and source alpha and match
DirectNR's UNORM residual (no additional gamma-space reinterpretation). The
edited sRGB case also enables framebuffer conversion and checks encoded
transport bytes and restoration of that inherited GL state. The
bridge follows DirectNR's residual resolve onto the original full-resolution
eye; it does not enlarge the inference proxy as a replacement for that eye.
The model-zero case verifies original pixels at intensity 0. A one-shot delayed
worker verifies bounded waiting, immutable pending input, discarded late output
and automatic recovery. Live tests exercise a settings change while warmup is pending, actual option
delivery, disable/re-enable and worker/IPC retirement. GPU probes require a
working hardware OpenGL/D3D adapter and are not unconditional CTests.
`tests/neural_post/run.ps1 -HostPath <helper.exe> -RuntimeDirectory <local-runtime>
-WithModel` creates disposable test INIs and exercises all cases. Individual
`live` runs use `DiagnosticWarmupDelayMs=100` to make an outstanding warmup
deterministic even when the diagnostic worker finishes in less than one frame.

DirectNR is pinned to LowPopLabs/nr-bridge revision
`0bb6bf86e13ac19a85d7693062c1f74cfcd5f77f`, Git blob
`391982151d89e4cba10f3cd4637a0add76f8556f`, under its included MIT license.
NVIDIA definitions are fetched from a pinned revision with SHA256 verification.
The adaptation, stereo protocol and test harness are versioned here; no ignored
research file is a build input. This remains an experiment, with synthetic GPU
and limited Requiem live-menu evidence. Gameplay quality/headset validation is
pending. No visual improvement or official NVIDIA integration is claimed.
Forwarder source: same revision, `src/nr_forwarder.cpp`, SHA256
`ADFA3D29A87483F7261CE386EF0A238DF54F8D284271C6603B8337189DD7C0BD`.
