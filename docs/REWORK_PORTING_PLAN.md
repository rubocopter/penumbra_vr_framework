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

Rework is the baseline, not the ceiling. When Black Plague or the Requiem
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
- final installer user acceptance and release audit.

Some Rework behavior is intentionally product-specific unless future evidence
proves otherwise, including exact HPL material/light response, model/weapon
statistics, authored rig/socket data and source-game UI/gameplay mechanics.

Rework `23c890f` copies refraction from a framebuffer whose dimensions match
its native screen texture. Requiem's binary renderer retains that copy path,
but the Framework draws each VR eye into a larger target; the shipped shader
samples the copy in eye viewport pixels. The refraction-on/off headset
comparison exposes this incompatible renderer boundary. A Requiem exact-build
adapter now captures the full eye into the bound screen texture only at the two
verified refraction copy callsites. The headset clip with Refractions enabled
shows the portal ring without the prior rectangular/striped defect; the runtime
copy counters match the intercepted calls, with normal pacing and exit.
Black Plague's initialized image independently maps the same renderer copy
contract at its own vtable and two callsites; its installed refraction shaders
also sample in viewport pixels. Both adapters now consume one OpenGL full-eye
capture implementation, with per-game caller allowlists and rollback ownership.
A real-driver host test verifies expansion, full-corner refresh and preservation
of GL binding/viewport state. BP visual appearance is still unvalidated; this
port does not establish that every smoke effect uses refraction.

Rework `23c890f` fixes the visible hand curl around an attached tool according
to its grip radius, overriding sensor curl while attached. Black Plague and
Requiem consume the shared hold-pose policy. Requiem's tool clips exposed
finger intersection: its visible hand first lacked the attachment weight, and
the shared articulation then allowed sensor curl to exceed that weight. The
adapter now supplies the weight only after validating the live attachment,
and shared articulation fixes the curl to that weight. The latter change is
host-tested and still needs visor validation. The flashlight's apparent light
offset remains a separate target-owned model/light question; Rework's modified
DAE positions its ray billboard differently from Requiem's installed asset.
The subsequent Requiem trial showed that the fixed curl alone did not keep the
native tool attached to the visible hand during stick movement. Unlike Black
Plague's tool path, Requiem used a raw grip for the tool while drawing the hand
from the collision-resolved palm. Its adapter now uses the same resolved palm
for both, with a raw fallback across missing publications and snap-turn epochs.
The latest headset clip and log validate motion coherence for all three tested
tools. The installed BP/Requiem flashlight DAE places spotlight/ray nodes on
model -Y. A target-owned measured-grip socket has headset evidence for beam/
model alignment, but copying BP's rotation mapped that axis backwards in
Requiem. Reversing the target socket has headset evidence for forward direction,
but restoring Rework's 1.6 parent scale reintroduced beam separation. HPL's
`BillBoard.cpp::GetModelMatrix` normalizes the axis billboard basis; its installed
0.210 height stays fixed while the centre at model Y=-0.203767 inherits scale.
Against the emitter at Y=-0.103966, 1.6 predicts a 0.05468 gap instead of the
native 0.00520 overlap. The target profile therefore keeps a rigid native-size
parent and 0.020 grip radius with the validated direction. This combined
correction is host-tested pending one headset check; a larger tool requires
separately proving the target billboard-size boundary. Earlier slight finger
intersection remains a known limitation.

Black Plague's VR-origin free `Grab=6` uses Rework's point-in-palm transform.
It previously anchored the body's local origin, independent of the selected
surface point.
Rework's variable surface contact and conditional `mbPickAtPoint` mode produced
different grip placements for the same Black Plague prop. In the 2026-09-24
headset clip, repeated native selections still needed many attempts despite a
valid nearby VR target: the extra palm-box contact test rejected them. A fresh
VR winner now uses the shared 0.40 m reach bound, while stale/native-only picks
retain the tighter box guard. The Requiem headset sequence later demonstrated
stable reacquisition of one prop from different surface points. BP now anchors
a fresh, accepted VR surface contact within the shared reach bound and retains
its origin fallback for stale/native-only picks; this port is host-tested
pending a BP visor trial. Native-origin grabs and jointed
`Move=2` mechanisms retain their native behavior. A published world-yaw epoch
also distinguishes snap turning from physical palm jumps: free bodies follow
that rebase without a one-frame force impulse, while mechanisms rebase their
hand offset without teleporting the joint.

## Validation rule

Extraction, target implementation and target validation are different facts.
Use the common evidence ladder and never promote a target because Overture proved
the reference behavior.

For regressions, inspect in this order:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

Current per-capability state belongs in `TRILOGY_PARITY_PLAN.md`; executable and
support identity belongs in `SUPPORTED_BUILDS.md`; active tasks belong in the
root `ROADMAP.md`.
