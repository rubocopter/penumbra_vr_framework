# Penumbra VR Rework porting contract

This document defines how proven Overture/Rework behavior moves into the shared
Framework. Cross-game completion is tracked separately in
`TRILOGY_PARITY_PLAN.md`.

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral
reference. The Framework-owned Overture product is the forward integration line;
Rework is not a build/runtime dependency.

## Porting rule

For a demonstrated Rework capability:

1. locate the exact implementation and tests/evidence;
2. identify HPL/game/model-specific dependencies;
3. extract only genuinely game-neutral policy;
4. expose the narrowest backend/profile boundary needed by the target;
5. preserve proven behavior/constants unless target evidence demonstrates a
   necessary difference;
6. validate the target independently.

A replacement algorithm is justified only when Rework has no equivalent, the
target interface is incompatible, or evidence shows direct adaptation is unsafe
or incorrect. Record the reason in durable architecture/parity documentation.

## Bidirectional reuse

Rework is the baseline, not the ceiling. When Black Plague or a future Requiem
backend demonstrates stronger game-neutral behavior, that behavior may become the
shared implementation after a real cross-consumer boundary is proven. The
Framework-owned Overture product should then consume the shared improvement too.

Game-specific mechanics, binary layouts, tool/model sockets and rig details stay
with their owner even when the surrounding policy becomes shared.

## Current portability boundary

| Capability | Shared destination | Target-owned part |
| --- | --- | --- |
| OpenVR poses/compositor lifecycle | runtime | frame/render entry point |
| Tracking/world yaw/recenter/play mode | runtime | native body/camera application |
| Metric locomotion and accepted displacement | runtime | native movement/collision boundary |
| VR footstep cadence | runtime | native surface/material footstep call |
| Logical input/actions/haptics | runtime + shared OpenVR assets | native intents and per-game action lifecycle |
| Settings/ranges/editor policy | runtime | storage/UI/backend capability map |
| Per-eye target/render policy | runtime | renderer/GL integration |
| Hand conditioning/articulation output | runtime | rig/bone/profile mapping |
| Palm sweep/recovery/contact math | interaction policy | native shape/contact query adapter |
| Grab/throw/nudge math | interaction policy | entity/body ownership and mechanism lifecycle |
| Slider/hinge servo behavior | interaction policy when compatible | native joint selection and special mechanisms |
| Tool/light grip behavior | shared behavior + profile | model geometry, sockets, inventory semantics |
| Tracked panel/menu policy | runtime | game UI/draw owner |
| HRTF/reverb policy | audio runtime | audio startup/effect hook/world query |
| Distance/occlusion low-pass | audio runtime | target world-query/audio boundary |
| Enhanced Visuals final eye treatment | visual runtime | renderer-specific material/light path |
| LAA requirement | installer policy | allowlisted executable transform/hash |
| Backup/repair/rollback | installer | per-game payload/build manifest |

## Extraction state

Already demonstrated as reusable and consumed by Overture plus Black Plague in
some form:

- tracking transforms, play mode and world-yaw policy;
- settings schema/editor semantics;
- logical OpenVR actions and controller binding graph;
- metric locomotion and accepted-motion policy;
- hand pose conditioning, contact math and free-body interaction policy;
- haptic event policy;
- stereo/render-target policy and visual calibration pieces;
- HRTF configuration and environmental-audio parameter policy.

Still incomplete at the cross-game level:

- final mechanism coverage and physical-interaction headset parity;
- distance/occlusion audio consumption in Black Plague;
- complete reusable UI/subtitle behavior;
- full Enhanced Visuals parity beyond the transferable eye stage;
- controller-family hardware validation;
- unified installer transaction.

Some Rework behavior is intentionally product-specific unless future evidence
proves otherwise, including exact HPL material/light response, model/weapon
statistics, authored rig/socket data and source-game UI/gameplay mechanics.

## Validation rule

Extraction, target implementation and target validation are different facts.
Use the common evidence ladder and never promote a target because Overture proved
the reference behavior.

For regressions, inspect in this order:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Current per-capability state belongs in `TRILOGY_PARITY_PLAN.md`; executable and
support identity belongs in `SUPPORTED_BUILDS.md`; active tasks belong in the
root `ROADMAP.md`.
