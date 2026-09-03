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

## Teardown

`PenumbraVR_Shutdown` first restores the original IAT entry, then closes the log. The launcher waits for that call to finish and only then invokes `FreeLibrary` in the target process. If restoring the expected pointer fails, the DLL is not unloaded.

## Verification performed

On 2026-09-03, a Release build was attached and detached three times in the same Steam-launched process. Each cycle:

- revalidated the executable hash inside the game
- observed at least one real swap
- restored the original import
- unloaded the probe
- left the game process alive for the next cycle

The cycles observed 1, 31 and 73 swaps respectively. Separate Debug and Release tests use a small fake `SDL.dll` to verify interception, forwarding and restoration of the import.

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

The next probe should observe, without changing, the OpenGL projection/model-view setup leading to each swap. This evidence will determine whether an API-level matrix hook is sufficient or whether the backend must hook a higher HPL1 camera/render entry point.
