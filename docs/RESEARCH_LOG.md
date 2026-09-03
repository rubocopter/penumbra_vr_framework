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
- Question: can a version-gated DLL attach through Steam, observe a stable frame boundary, and unload without inline code patches?
- Evidence: three consecutive attach/detach cycles observed 1, 31 and 73 `SDL_GL_SwapBuffers` calls. Every detach restored the expected IAT pointer and unloaded the DLL while the game remained active.
- Modified address: the main module IAT entry for `SDL.dll!SDL_GL_SwapBuffers`; resolved from PE metadata at runtime.
- Calling convention: `void __cdecl SDL_GL_SwapBuffers(void)`.
- Result: confirmed for this exact build.

OpenGL observation hooks for `glMatrixMode`, `glLoadMatrixf` and `glOrtho` were then verified independently in Debug and Release and run for 122 menu frames. Steady-state frames showed two orthographic calls and no loaded float matrix. Result: telemetry works, but the menu capture does not expose the 3D camera.

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
