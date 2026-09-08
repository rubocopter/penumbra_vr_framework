# Codex handoff — Penumbra VR Framework

This file is the operational handoff for future coding sessions. Read it before changing runtime, backend, interaction, tracking or deployment code.

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

The Framework is **pre-alpha**.

### Overture

`pvr_overture_backend` contains a host-tested first backend core derived from Rework:

- tracking space and calibrated height;
- seated/standing composition;
- world yaw/recenter;
- room-scale displacement rejection/reconciliation;
- fixed displacement steps of `0.05 m`;
- locomotion policy of `1.5 m/s` walk and `2.25 m/s` sprint;
- shared interaction reach of `0.18 m`.

`OvertureBodyAdapter` is intentionally the game-specific boundary for HPL body position, feet height, collision movement and jump.

**Current gate:** link this adapter into the real Overture executable, deploy a test build, and headset-validate behavior against Rework. Do not treat the host-tested backend as a playable Overture replacement yet.

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

Do not patch symptoms first.

## Immediate next session

1. Inspect the exact Rework implementation and current Framework Overture backend side-by-side.
2. Link `OvertureBodyAdapter` into the real Overture executable without duplicating runtime logic.
3. Build/deploy a test package and validate tracking, height, recenter, movement, sprint, crouch/jump and room-scale rejection in the headset.
4. Record any behavioral difference against Rework before changing constants.
5. Only after Overture equivalence is established, resume Black Plague body/capsule mapping.

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
- OpenVR SDK is an external optional dependency

## Required documentation after meaningful changes

Update the smallest relevant set among:

- `README.md` — public/current status;
- `ROADMAP.md` — milestone state;
- `CHANGELOG.md` — implemented/validated changes;
- `docs/REWORK_PORTING_PLAN.md` — extraction/porting boundary;
- `docs/OVERTURE_BACKEND_MIGRATION.md` — exact Rework-to-Framework comparison;
- `docs/VR_STARTUP_AND_CONTROLLERS.md` — real headset state and controls;
- `docs/VR_HEADSET_TEST_CHECKLIST.md` — next manual validation;
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md` / `docs/BLACK_PLAGUE_PROBE.md` — binary evidence and live probe findings;
- `docs/SUPPORTED_BUILDS.md` — only when build support/evidence changes;
- `THIRD_PARTY.md` — whenever upstream-derived code or assets are added/changed.

Never mark a feature complete merely because a code path exists.
