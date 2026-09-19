# Trilogy parity plan

This document is the capability ledger for turning Penumbra VR Framework into a
real trilogy framework rather than three loosely related ports.

`rubocopter/penumbra_vr_rework` revision `23c890f` is the behavioral baseline
for capabilities already demonstrated in Overture. Overture is the reference
product, Black Plague is the second-backend proof that reusable behavior has
actually been generalized, and Requiem is the third-backend consumer.

## What counts as ported

A capability is **not** ported merely because its constants, policy, data or
tests exist in shared runtime code.

For each reusable capability, track these independent facts:

1. **Reference** — the exact proven Overture/Rework behavior is identified.
2. **Shared** — game-neutral policy/data has been extracted where reuse is real.
3. **Consumed** — the target backend actually uses that shared behavior, or a
   documented game-specific equivalent where the native boundary is incompatible.
4. **Validated** — the target has evidence at the appropriate host/live/headset
   level. Evidence never transfers automatically between games.

An extracted policy with no Black Plague consumer remains an open parity item.
An asset or setting present in the repository with no applied backend behavior
also remains open.

The normal evidence ladder still applies:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

## Framework-readiness gate before Requiem gameplay work

Requiem exact-build reconnaissance may record executable identity and other
non-invasive facts at any time, but gameplay implementation should not become
the active milestone until Black Plague has proved the framework boundary below.

The gate is satisfied when every demonstrated Overture VR capability is in one
of these states:

- consumed by Black Plague through shared runtime policy;
- implemented by a documented Black Plague-specific equivalent because the
  native interface is incompatible;
- explicitly classified as Overture-only/not applicable, with evidence and a
  reason; or
- deliberately deferred from trilogy parity because it is release tooling rather
  than gameplay/runtime behavior.

For reusable gameplay/runtime families, `shared but not consumed` does **not**
satisfy the gate.

The gate does not require every controller model to be physically headset-tested
before Requiem work can begin. It does require the shared binding graphs to stay
regression-gated and available target hardware to prove backend routing, poses,
interaction and haptics. Full per-device support remains a release/support gate.

## Capability ledger

Statuses below describe the current repository state, not the desired end state.

| Capability family | Overture/Rework reference | Shared framework state | Black Plague state | Required before Requiem gameplay milestone |
| --- | --- | --- | --- | --- |
| Stereo rendering / compositor lifecycle | Proven | Shared renderer/session policy plus HPL adapter pieces | Consumed; substantial live/headset evidence | Preserve as regression gate; close current frame-pacing/focus edges |
| Rotational tracking / camera composition | Proven | Shared transforms and camera transaction | Consumed and headset exercised | Preserve |
| Positional tracking / room-scale rejection | Proven | Shared tracking/reconciliation policy | Consumed; the 2026-09-19 combined headset run reported open-space room-scale as good. PID 22004 proved the original wall bounce was native step-up: the body rises exactly `0.05 m`, falls under native gravity and repeats. PID 30036 reported the wall/head approach improved but still reproduced the same `+0.05 m` cycle near low props, chiefly on ticks carrying both physical room-scale and direct locomotion. The previous static/upward step gate only ran on physical-only ticks. The current host-tested candidate now applies that Rework gate whenever a physical room-scale component is present while preserving pure stick/native step. Exact-build analysis also pins BP's dynamic-body push callback, whose movement guard now receives the VR movement signal plus Rework's `0.2x` push force only around the existing horizontal solver | Revalidate deliberate wall pressure, combined stick+room-scale approach to low props, one genuine low static obstacle and bottle/board/chair approach without changing proven room-scale ownership |
| Direct walk/sprint locomotion | Proven `1.5/2.25 m/s` policy | Shared locomotion policy | Consumed through BP single-tick collision boundary; constrained state still needs focused validation | Validate target adaptation and keep speed policy reusable |
| Physical/button crouch and tracked Y | Proven | Shared crouch/tracking policy | Consumed; headset evidence exists, Hybrid release-hold and blocked stand remain | Close remaining target-specific edges |
| Recenter and turn policy | Proven | Shared settings/input/tracking policy | Recenter wired; snap/smooth turn is consumed through tracking `world yaw`. The 2026-09-19 run exposed an occasional one-frame left/right hand discontinuity during turning; gameplay palm history was world-space but unaware of presentation yaw epochs. The current host-tested candidate invalidates stale resolved poses immediately and rebases resolver history on yaw-epoch changes | Retest snap/smooth turn hand continuity, then close recenter/tracking-loss behavior separately |
| Player camera / head-bob / footsteps | Rework adds a `>0.85 m` horizontal body-travel cadence and delegates sound/material selection to native `FootStep(0.8)` | Shared `VrFootstepCadence` now owns the game-neutral accepted-travel cadence | **Consumed / host-tested for VR footsteps**: BP feeds only collision-accepted physical/direct-VR displacement; the body observer queues cadence and the existing game-thread input owner dispatches exact-build `cPlayer::FootStep` after native update return through the verifier-pinned MSVC 2003 string ABI. Dispatch telemetry is available; native bob/animation behavior remains game-owned and headset observation is open | Headset-validate cadence/surface sound during direct and physical movement and record remaining native bob/body-animation behavior without retuning the body path |
| Controller actions/bindings | Proven profile set | 42 actions, 6 sets, 8 binding graphs are host-gated | Consumed; per-hardware live parity incomplete | Keep binding graph gate; validate available hardware/backend behavior |
| Hand pose / finger articulation | Proven hands; BP has richer articulation semantics | Shared conditioning/articulation boundary plus Rework authored attachment grip pose | The 2026-09-19 headset run showed the imported Rework mesh itself stable but thumb/little effectively frozen. Earlier Black Plague provisional-hand headset behavior was better and responded naturally to the controller finger sensors, so Rework's trigger/grip-derived fallback is not a valid improvement here. The bridge preserves BP's independent skeletal channels and restores the provisional hand's stronger free-hand curl amplitudes. The imported Rework rigid skin keeps its proven long-finger and diagonal-thumb axes and deliberately omits the provisional renderer's extra spread/thumb yaw, which twists the fused web/little finger on this mesh. The separate Rework authored held-tool pose remains intact; headset validation remains open | Retest all five fingers against the previously good BP response; if thumb/little still fail, instrument raw/smoothed curls before changing bone mapping |
| Palm collision / physical hand resolution | Proven | Shared collision policy including bounded interaction-assist skin and recovery diagnostics | 2026-09-19 headset evidence: contact and partial sliding work, but occasional snap/reset remained and physical/stick head motion could produce mini-jumps near collision. Yaw-epoch history rebasing is now host-tested and separately observable through `yaw_epoch_resets` telemetry | Retest wall/table slide and turns; correlate only remaining snaps with true reanchor/recovery counters |
| Direct hand nudge | Proven | Rework behavior identified; reusable portions adapted | Headset evidence confirms direct hand pushing works, but PID 30036 showed a fallen shelf being launched merely by approaching it. The old BP fallback treated every jointed body as its strongest class. The current host-tested candidate keeps fresh-target protection and ports the evidenced Rework safety shape: only Object/Item/SwingDoor families are eligible, large/heavy bodies receive the reduced cap, ordinary Objects use the gentle grab/move cap, recognized one-joint mechanisms project hand velocity onto their permitted hinge/slider motion, and unknown jointed mechanisms fail closed. BP fields for breakable/lock state remain unmapped and are not guessed | Retest hand approach to the fallen shelf, gentle push of a loose prop and a representative hinge/slider without proximity launching |
| Free-body grab / hold / throw | Proven | Shared palm-relative grab/release math | PID 30036 still produced many Interact attempts with very few committed `Grab=6`/`Move=2` acquisitions; a chair and locker could not be acquired reliably. The current host-tested candidate now reuses the existing exact-build palm collision box as the primary nearby target query, matching Rework's physical-overlap priority. Only when resolved-palm contact finds nothing does the same box follow the raw controller up to `0.18 m`, with solid LOS, before falling back to the existing rays and item-only magnetism. The winner still enters the native callback/state transition and existing hold owners | Retest natural touch-to-grab on small props/chair and touch-to-Move on the locker; once acquisition is reliable, separately judge whether committed held bodies still float/orbit |
| Magnetic inventory-item acquisition | Proven | Shared item-only magnetic policy host-tested | **Consumed / host-tested**; BP exact-build world-body enumeration, item classification, BV geometry, top-five ranking and dual controller/HMD solid LOS run as a fallback inside the existing native-pick owner; BP-only gasmask/collectable remain excluded | Headset-validate representative magnetic pickups and native state transitions before promotion |
| Jointed mechanisms: doors/sliders/levers | Proven | Shared free/slider/hinge servo math host-tested | **Consumed / host-tested for one-joint `cGameLever` and hinge-only one-joint `cGameSwingDoor`**; exact entity family, hinge/slider type, pin/pivot, velocity setters and native Enter/Leave controller lifecycle are verifier-pinned. Lever accepts demonstrated hinge/slider forms; SwingDoor is fail-closed to hinge. Wheel/unknown/multi-joint mechanisms remain native | Headset-validate representative Lever/SwingDoor interaction; map Wheel or compound families only from target evidence |
| Tool attachment / flashlight / glowstick | Proven concept; Rework glowstick primary cylinder matches BP | Shared attachment composition and authored hand-side grip frame | The 2026-09-19 headset run positively observed the glowstick in the intended hand position and its toggle feedback. Flashlight/socket/light-direction coverage remains open | Preserve glowstick as regression coverage; headset-validate flashlight/light direction separately |
| Stable VR panels / menu pointer | Proven | Shared panel and pointer policy | Consumed for tracked menus | Preserve; extend same ownership rules to remaining UI surfaces |
| Inventory / notebook | Rework routes VR actions through the native inventory/notebook owners and treats both as VR UI contexts | Shared tracked-panel/pointer policy | **Consumed / host-tested**: BP's existing VR query sites open native `StartInventory` / `Notebook::SetActive(true)`; exact-build verification pins those calls plus `inventory+0x5C` / `notebook+0x44`, and the native-input harness proves those bytes enter `UiContext`. Post-native-update publication feeds the existing whole-frame tracked-menu capture | Headset-validate inventory/notebook readability, pointer/select/back behavior and focus recovery without adding another UI owner |
| HUD / subtitles | Rework has product-owned HUD/subtitle drawing and applies `GetSubtitleScale()` to message line height/font size | Shared settings and overlay/panel primitives exist; product draw ownership remains game-specific | **Consumed presentation shell / host-tested**: exact BP `OnPostSceneDraw -> GetDrawer -> DrawAll` ownership is verifier-pinned. Framework defers gameplay submit to the sole `DrawAll` owner, captures its native 800x600 RGBA queue once and alpha-composites that surface into both eye targets using Rework-derived placement. PID 25952 exposed that HPL can leave a stale GL error before this optional overlay; the draw now clears inherited errors and attributes only its own GL failures. `cGameMessage::Draw` is mapped at `0x3F380`; persisted `subtitle_scale` is still intentionally unwired | Headset-confirm HUD/subtitles actually arrive in-eye without compositor regression, then wire target-specific subtitle scale and validate readability |
| VR-native quick access / radial menu | New Framework UX extension | **Planned** shared input/selection/presentation policy; per-game actions/items stay behind adapters | **Planned after the current headset/body/interaction validation gates** | Build once as optional shared VR UX, preserve the original inventory/menu path, then consume through narrow Overture/BP/Requiem action or inventory adapters |
| UI dimmer / transient overlay ownership | Proven after-world/before-UI dimmer in Overture flows | Overlay handoff shared; dimmer scalar remains product-owned | **Partially classified**: BP tracked inventory/notebook uses a complete desktop capture projected over a dark VR background, so Rework's behind-UI dimmer is not directly applicable there without changing presentation semantics. Gameplay message/transient-overlay dimming remains unmapped | Map a BP native active-state boundary for gameplay messages/transient overlays before consuming/extracting dimmer policy; do not darken the captured menu surface itself |
| Loading / map-transition compositor fades | Proven | Overture-specific lifecycle today | **Incomplete** | Map BP transition lifecycle and implement equivalent comfort behavior; extract common policy if real |
| Haptics | Proven event profiles | Shared event profiles/strength/cooldowns | Pickup/drop, UI-select and direct-hand-contact feedback are consumed; **LightToggle, Damage and confirmed-contact MeleeImpact are consumed / exact-image verified / host-tested** at native success/contact boundaries | Headset-validate exercised submissions, including dominant-hand melee feedback on real contact and no pulse on an empty swing |
| Spatial audio / HRTF / occlusion / reverb | Proven | Shared reference behavior and tests | **HRTF consumed / host-tested** through the launcher-owned pre-audio `alsoft.ini` boundary. **Mine-gallery reverb consumed / exact-image verified / host-tested** through BP's native OpenAL/EFX effect-attach and environment-gain boundaries, with late-start bootstrap that preserves an already-authored non-default environment. The Rework distance-HF path remains absent from the mapped BP audio code | Headset/audio-device validate HRTF and the consumed reverb/bus trim; map a narrow runtime boundary for distance/occlusion low-pass before consuming that remaining policy |
| Enhanced VR visuals / visual calibration | Proven v4 behavior: per-eye RGBA16F + 2x MSAA/final treatment plus VR-specific HPL ambient/light responses | CPU reference/calibration plus shared OpenGL eye-stage implementation | **Partially consumed / host-tested**: BP routes enabled persistent eyes through the Rework-equivalent RGBA16F + 2x MSAA resolve and exact v4 final treatment, with direct-eye fallback when the optional GL feature set is unavailable. Rework's HPL material/light response is not yet consumed | Headset-validate the eye stage, then map/adapt the remaining material/light behavior only at demonstrated BP renderer/material boundaries |
| VR settings semantics | Proven 18-row behavior and native Options integration | Shared schema/editor/store | **Consumed / host-tested for the native settings page**: BP injects a `VR Settings` entry into exact native Options state 8 and exposes exactly its 17 wired capabilities through shared adjustment/format/dependency policy. Accepted changes persist immediately; input/tracked-presentation consumers refresh live, while render scale and HRTF remain startup-owned and are labelled restart-required. Subtitle scale remains intentionally unwired | Headset-exercise navigation, left/right edits and persistence, then validate target-visible behavior independently |
| Monitor mirror / focus recovery | Rework behavior exists but Framework mirror is experimental | Shared optional mirror policy | Mirror-on was reported correct in the 2026-09-19 combined headset run; Alt+Tab/focus-loss recovery remains open | Preserve mirror behavior and validate focus recovery separately |
| Representative gameplay/chapter validation | Proven playable Rework baseline | N/A | Still open | Complete representative chapter-level BP validation before calling BP playable-alpha parity achieved |

## Black Plague parity work order

Do not work this list as a blind feature checklist. Preserve already validated
boundaries and advance the smallest evidence gate that closes a ledger row.

1. Retest the focused 2026-09-19 fixes: acquire several small props and a chair
   by physical palm contact, exercise the locker/door native Move transition,
   and approach the fallen shelf without proximity launching while an unrelated
   loose prop remains gently pushable; approach bottle/board/chair bodies with
   combined physical+stick movement without repeated
   mini-jumps; sweep palms along wall/table and perform snap/smooth turns without
   a one-frame hand discontinuity; compare all five finger responses with the
   previously good Black Plague provisional hand. Preserve
   the already-positive room-scale, glowstick placement/toggle feedback and
   mirror behavior as regression checks rather than reopening them.
2. Close focused body/comfort leftovers: Hybrid release-hold, blocked stand/low
   ceiling, constrained Push/Move locomotion, directed validation of the already
   tracking-world-yaw turn owner, recenter/tracking-loss, and headset validation
   of the host-tested VR footstep cadence plus remaining player-camera/head-bob
   presentation behavior.
3. Close interaction parity: headset-validate the now host-tested BP magnetic
   inventory fallback, then complete representative native joint mechanisms for
   slider/hinge/door interaction. Validate long bodies and definitive tool/light
   sockets in the same interaction phase.
4. Close presentation parity: headset-validate the host-mapped native inventory/
   notebook route and the new host-tested gameplay 2D surface, then wire subtitle
   scale only after actual in-eye subtitle evidence. Continue with transient
   overlays/dimmer and loading/fade comfort behavior. Generalize only policy
   proven to have a second consumer.
5. After the current validation, interaction and presentation gates are stable,
   add the first Framework-native VR UX extension: an optional shared radial/
   quick-access menu. Runtime owns hold/open/close, dead-zone, sector selection,
   handedness, cancellation and haptic-selection semantics; each game adapter
   exposes only available actions/items and activation. Keep the original
   inventory/menu route intact, and reuse the same shared behavior in Overture,
   Black Plague and later Requiem rather than implementing three product-local
   radial menus. This enhancement must not displace the current headset gates.
6. Close feedback/output parity: headset-validate the consumed haptic/HRTF/
   reverb paths, map a safe BP runtime boundary for distance/occlusion, and decide
   which Rework enhanced-visual stages have a compatible BP renderer boundary.
7. Run representative chapter-level Black Plague validation and update this
   ledger with actual evidence. At that point Black Plague becomes the proof
   that the shared runtime can support a second complete game.

## Requiem implementation contract

When the Black Plague framework-readiness gate is closed:

1. fingerprint and whitelist the exact Requiem build independently;
2. map renderer, player/body, native input, UI, interaction and audio boundaries
   from Requiem evidence rather than copying BP RVAs/layouts;
3. consume the same shared capability families in the ledger, adding only narrow
   Requiem profiles/adapters for binary/layout/model/state differences;
4. if Requiem exposes a better game-neutral behavior, move that behavior back
   toward shared runtime and regression-test both earlier games;
5. run the same parity ledger for Requiem, with its own live/headset evidence;
6. only after all three products remain green should installer/release work
   become the primary milestone.

This makes Black Plague the architectural proof and Requiem the reuse proof.
