# Codex handoff — Penumbra VR Framework

This file is the operational handoff for future coding sessions. Read it before changing runtime, backend, interaction, tracking or deployment code. `AGENTS.md` is the root engineering contract; `DEBUG_HANDOFF.md` records known problem boundaries and failed symptom-level approaches.

## Non-negotiable source of truth

`rubocopter/penumbra_vr_rework`, revision `23c890f`, is the proven behavioral reference for Overture VR behavior.

Do **not** design a new approximation of a Rework subsystem before inspecting the exact Rework implementation.

For any behavior already demonstrated by Rework:

1. locate the exact implementation and relevant tests;
2. identify which parts are game-specific;
3. port the game-neutral behavior into the Framework;
4. create the narrowest possible per-game adapter for engine/entity/physics/render differences;
5. preserve Rework sequencing, constants and safety behavior unless target-game evidence requires a change;
6. validate the adapted path independently.

A new algorithm requires an explicit technical reason: no Rework equivalent, incompatible target API, or evidence that the original cannot be safely adapted.

## Current checkpoint

The Framework is **pre-alpha**. The latest repository checkpoint is documented by the commits at the tip of `main`; do not assume a clean local working tree without checking it.

### Overture

`pvr_overture_backend` contains a first backend core derived from Rework, now host-tested and functionally validated in a headset through the autonomous Framework product:

- tracking space and calibrated height;
- seated/standing composition;
- world yaw/recenter;
- room-scale displacement rejection/reconciliation;
- fixed displacement steps of `0.05 m`;
- locomotion policy of `1.5 m/s` walk and `2.25 m/s` sprint;
- shared interaction reach of `0.18 m`.

`OvertureBodyAdapter` remains the game-specific boundary for HPL body position, feet height, collision movement and jump. `src/adapters/overture_source` implements that boundary against the real Overture `cPlayer`/`iCharacterBody` types. The Framework-owned consumer in `products/overture` replaces its inline tracking/movement block with the backend call, retains HPL-specific crouch and footstep ownership, and compiles the Framework sources into `Penumbra_vr.exe` through the supplied MSBuild props file.

The minimal coherent Overture/HPL source host, Win32 dependencies, required resources, validators and package scripts now live under `products/overture`; exact Overture bindings live under `assets/openvr/overture`. Neither build nor package reads the Rework checkout. Debug and Release Win32 builds succeed from the Framework. The tested Release package is at `products/overture/build/package/Release/PenumbraVR`; its 3,302,912-byte executable has SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`.

On 2026-09-10 the user deployed that exact package with its included `Install-PenumbraVR.bat` over a valid retail Overture installation, then ran it with SteamVR, a real headset and controllers. The deployed executable hash matched the autonomous Framework artifact above. The first functional headset pass showed perceived behavior equivalent to the previously tested Rework build, with no evident regression reported. This validates the autonomous build/deployment/integration path in a headset; it does **not** prove exhaustive coverage of every tracking, calibration, comfort, locomotion, collision, interaction, visual or hardware case, and it is not yet a `supported` release claim.

Validation on 2026-09-10: the standalone Overture Release pipeline passed all shader, 8,752 visual, 231 texture/decode, metadata, LAA, package and 289 legacy tracking-test checks; the full Debug build also passed its 289 checks after disabling legacy `/Gm` only for the C++20 game project. Root Debug and Release builds include `PenumbraVR.BlackPlague.Probe.dll`, and all 24 CTest tests pass in each configuration. The evaluated Release MSBuild project and all build/package inputs contain no positive Rework checkout path; `git diff --check` passes.

**Milestone state:** autonomous Overture source/build/package integration is complete and has an initial functional headset validation. Preserve the tested hash and behavior. Any future exhaustive Overture checklist or regression report remains a separate evidence gate; do not reopen the source-host migration or mark the product supported without that evidence.

### Black Plague

Validated in headset previously:

- native stereo;
- yaw-aligned rotational HMD tracking;
- keyboard/mouse preservation;
- conservative HMD-aware render-list visibility.

Implemented/code-tested but still requiring focused headset validation:

- OpenVR actions and exact-build native input bridge;
- tracked menus;
- provisional procedural gloves;
- controller picking;
- palm-relative free-body grab/release/throw;
- `0.18 m` direct physical reach fallback;
- tool/light attachment groundwork;
- scissor/light mapping.

Still blocked or incomplete:

- exact body/capsule/collision mapping;
- positional HMD translation;
- Rework-equivalent locomotion through a measured BP body adapter;
- palm collision;
- jointed mechanisms and doors/levers;
- definitive tool/glowstick grip profiles;
- GPU Enhanced visuals path;
- reliable monitor mirror;
- frame-pacing target validation.

**Do not invent a head collider or camera translation to make room-scale appear to work.** Map the exact native body/capsule and collision-resolution path first, add telemetry, then adapt Rework's displacement policy.

The desktop mirror is not a supported gameplay feature at the current checkpoint.

### Requiem

Treat as a future exact-build binary backend. Do not assume Black Plague RVAs, layouts, calling conventions or lifecycle boundaries transfer without binary evidence.

## Engineering boundaries

Runtime may own:

- OpenVR lifecycle;
- tracking transforms and coordinate conversion;
- logical input state;
- shared settings semantics;
- renderer-neutral eye/view data;
- proven locomotion/tracking/interaction policy.

Runtime must not own:

- game RVAs or signatures;
- exact HPL object layouts;
- player-class assumptions;
- binary calling conventions;
- game-specific entity queries.

Backend/adapter code owns those details.

## Validation discipline

Keep these states separate in code and documentation:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`.

Compilation and CTest do not imply headset validation.

When a behavior is reported as a regression, compare:

**REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION**.

Do not patch symptoms first. For the known recurring issues, read `DEBUG_HANDOFF.md` before changing movement timing, body collision, held-object collision, mechanism grabbing or tool grip placement.

## Immediate next session

1. Read `AGENTS.md`, `CODEX_HANDOFF.md` and `DEBUG_HANDOFF.md`.
2. Do not repeat the completed Overture source-host migration. Preserve the tested Release executable hash and use Rework `23c890f` only when a concrete behavioral comparison is needed.
3. Resume Black Plague exact-build research at the real player body/capsule/collision-resolution boundary.
4. Add telemetry for requested/accepted displacement, body/feet position and head/body divergence before enabling positional HMD translation.
5. Only after that boundary is measured, route the shared Rework displacement policy through a Black Plague body adapter.
6. Keep mirror, Requiem, production installer work and speculative movement/collider changes out of this milestone.

## Black Plague investigation order

1. exact-build native body/player structure;
2. capsule/character collision representation;
3. native movement/update boundary;
4. collision resolution and accepted displacement;
5. HMD-to-body relationship;
6. route shared Rework displacement policy through the adapter;
7. validate in headset.

For interaction, prioritize:

1. palm collision boundary;
2. body/character exclusion during held-body interaction;
3. jointed/mechanism state mapping;
4. per-game tool/glowstick grip profiles;
5. inventory/UI completeness.

## Do not spend effort on yet

- production installer implementation;
- mirror polishing;
- speculative Requiem ports;
- large new runtime abstractions before a real backend requires them;
- replacing proven Rework algorithms with cleaner-looking alternatives merely for style.

## Repository facts

- Repository: `rubocopter/penumbra_vr_framework`
- Default branch: `main`
- User-facing name: **Penumbra VR**
- Original Overture mod: `veryjos/penumbra_vr`
- Proven Rework reference: `rubocopter/penumbra_vr_rework` revision `23c890f`
- Windows/x86 target for current binary research
- OpenVR is optional for root CMake targets; the Overture product pins its required OpenVR 2.15.6 Win32 SDK under `products/overture/dependencies`
- Rework `23c890f` is reference-only and may be reset without affecting Framework builds

## Required documentation after meaningful changes

Update the smallest relevant set among:

- `README.md` — public/current status;
- `ROADMAP.md` — milestone state;
- `CHANGELOG.md` — implemented/validated changes;
- `CODEX_HANDOFF.md` — current operational checkpoint;
- `DEBUG_HANDOFF.md` — repeated defects, evidence and next diagnostic step;
- `docs/REWORK_PORTING_PLAN.md` — extraction/porting boundary;
- `docs/OVERTURE_BACKEND_MIGRATION.md` — exact Rework-to-Framework comparison;
- `docs/VR_STARTUP_AND_CONTROLLERS.md` — real headset state and controls;
- `docs/VR_HEADSET_TEST_CHECKLIST.md` — next manual validation;
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md` / `docs/BLACK_PLAGUE_PROBE.md` — binary evidence and live probe findings;
- `docs/SUPPORTED_BUILDS.md` — only when build support/evidence changes;
- `THIRD_PARTY.md` — whenever upstream-derived code or assets are added/changed.

Never mark a feature complete merely because a code path exists.
