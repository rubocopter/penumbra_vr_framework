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
| Positional tracking / room-scale rejection | Proven | Shared tracking/reconciliation policy | Consumed; latest zero-between-tick-X/Z presentation candidate is host-tested after PID 26940 and supersedes PID 25484 for current-code comfort status | Headset-validate the current wall/step/pullback candidate without changing proven ownership |
| Direct walk/sprint locomotion | Proven `1.5/2.25 m/s` policy | Shared locomotion policy | Consumed through BP single-tick collision boundary; constrained state still needs focused validation | Validate target adaptation and keep speed policy reusable |
| Physical/button crouch and tracked Y | Proven | Shared crouch/tracking policy | Consumed; headset evidence exists, Hybrid release-hold and blocked stand remain | Close remaining target-specific edges |
| Recenter and turn policy | Proven | Shared settings/input/tracking policy | Recenter wired; snap/smooth turn wired; tracking-world-yaw ownership still a focused gate | Validate final yaw ownership and recenter/tracking-loss behavior |
| Controller actions/bindings | Proven profile set | 42 actions, 6 sets, 8 binding graphs are host-gated | Consumed; per-hardware live parity incomplete | Keep binding graph gate; validate available hardware/backend behavior |
| Hand pose / finger articulation | Proven hands; BP has richer articulation semantics | Shared conditioning/articulation boundary | Consumed; imported Rework rigs host-tested, headset presentation gate open | Clean headset validation of scale/orientation/finger channels/performance |
| Palm collision / physical hand resolution | Proven | Shared collision policy | Native BP query/lifecycle live-tested; gameplay composition host-tested | Complete clean combined headset gate |
| Direct hand nudge | Proven | Rework behavior identified; reusable portions adapted | Host-tested BP candidate with narrower evidence-backed filters | Headset validate and record intentional differences from Rework filters |
| Free-body grab / hold / throw | Proven | Shared palm-relative grab/release math | Consumed for `Grab=6`; free `Move=2` has separate Rework-derived force path | Clean headset validation including long-body behavior |
| Magnetic inventory-item acquisition | Proven | Shared item-only magnetic policy host-tested | **Not consumed**; BP lacks safe item classifier/visibility boundary | Map classifier/visibility and consume shared magnetic policy |
| Jointed mechanisms: doors/sliders/levers | Proven | Shared free/slider/hinge servo math host-tested | **Not consumed**; jointed bodies intentionally remain native | Map representative native joint/state/update paths and consume/adapt shared policy |
| Tool attachment / flashlight / glowstick | Proven concept, Overture-specific geometry | Shared attachment composition | Consumed with BP-specific sockets; definitive geometry/light direction not headset-validated | Validate BP profiles; keep model data per-game |
| Stable VR panels / menu pointer | Proven | Shared panel and pointer policy | Consumed for tracked menus | Preserve; extend same ownership rules to remaining UI surfaces |
| Inventory / notebook / HUD / subtitles | Proven VR presentation behavior | Only reusable panel/pointer/settings pieces extracted | **Incomplete** | Map BP native UI/draw-state boundaries; deliver usable VR presentation and subtitle scaling where applicable |
| VR-native quick access / radial menu | New Framework UX extension | **Planned** shared input/selection/presentation policy; per-game actions/items stay behind adapters | **Planned after the current headset/body/interaction validation gates** | Build once as optional shared VR UX, preserve the original inventory/menu path, then consume through narrow Overture/BP/Requiem action or inventory adapters |
| UI dimmer / transient overlay ownership | Proven in Overture flows | Overlay handoff shared; dimmer remains product-owned | **Incomplete** | Determine BP-compatible boundary; extract only if a second consumer proves reuse |
| Loading / map-transition compositor fades | Proven | Overture-specific lifecycle today | **Incomplete** | Map BP transition lifecycle and implement equivalent comfort behavior; extract common policy if real |
| Haptics | Proven event profiles | Shared event profiles/strength/cooldowns | Pickup/drop consumed; UI-select and direct-hand-contact feedback are connected, host-tested and telemetry-gated; light/melee/damage boundaries remain open | Headset-validate exercised submissions, then connect the remaining applicable BP events |
| Spatial audio / HRTF / occlusion / reverb | Proven | Shared reference behavior and tests | **Not consumed** by BP audio backend | Map safe audio boundary and consume/adapt shared behavior or document incompatible equivalent |
| Enhanced VR visuals / visual calibration | Proven v4 behavior | CPU reference/calibration extracted | **Not fully consumed** by BP renderer | Decide target-compatible renderer stages and implement/validate equivalent visual behavior where applicable |
| VR settings semantics | Proven 18-row behavior | Shared schema/editor/store | Capability map exposes handedness, turn, move, play mode/player height, height offset, crouch, UI and render-scale controls because those paths are consumed by BP runtime. Enhanced Visuals, HRTF and subtitle scale remain unwired | Keep runtime consumption, editor exposure and validation as separate states; headset-validate target-visible behavior independently |
| Monitor mirror / focus recovery | Rework behavior exists but Framework mirror is experimental | Shared optional mirror policy | Experimental; Alt+Tab/focus-loss boundary open | Not a core parity blocker unless required for stable gameplay lifecycle; keep explicit release gate |
| Representative gameplay/chapter validation | Proven playable Rework baseline | N/A | Still open | Complete representative chapter-level BP validation before calling BP playable-alpha parity achieved |

## Black Plague parity work order

Do not work this list as a blind feature checklist. Preserve already validated
boundaries and advance the smallest evidence gate that closes a ledger row.

1. Finish the current combined body/palm candidate: physical wall pressure and
   slide without mini-jumps, ordinary stick stair/ledge stepping, stable hand
   rendering/fingers, representative `Grab=6` and free `Move=2`, short X/Z and
   crouch before/after contact.
2. Close focused body/comfort leftovers: Hybrid release-hold, blocked stand/low
   ceiling, constrained Push/Move locomotion, tracking-world-yaw, recenter/
   tracking-loss and final player-camera/head/footstep-bob ownership.
3. Close interaction parity instead of leaving shared policies unused: map BP
   item classification/visibility for magnetic pickup and representative native
   joint mechanisms for slider/hinge/door interaction. Validate long bodies and
   definitive tool/light sockets in the same interaction phase.
4. Close presentation parity: inventory, notebook/notes, HUD, subtitles,
   transient overlays/dimmer and loading/fade comfort behavior. Generalize only
   the policy proven to have a second consumer.
5. After the current validation, interaction and presentation gates are stable,
   add the first Framework-native VR UX extension: an optional shared radial/
   quick-access menu. Runtime owns hold/open/close, dead-zone, sector selection,
   handedness, cancellation and haptic-selection semantics; each game adapter
   exposes only available actions/items and activation. Keep the original
   inventory/menu route intact, and reuse the same shared behavior in Overture,
   Black Plague and later Requiem rather than implementing three product-local
   radial menus. This enhancement must not displace the current headset gates.
6. Close feedback/output parity: connect the remaining applicable shared haptic
   events, map the safe BP audio boundary for HRTF/occlusion/reverb, and decide
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
