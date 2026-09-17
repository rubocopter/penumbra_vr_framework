# Penumbra VR Rework porting plan

Cross-game capability completion is tracked in `TRILOGY_PARITY_PLAN.md`. This
document governs how demonstrated Rework behavior is extracted and adapted; the
parity ledger governs whether a target game actually consumes that behavior.
`Extracted` and `ported to Black Plague` are deliberately different states.

This plan treats `rubocopter/penumbra_vr_rework` revision `23c890f` as the proven Overture reference. When Rework already solves a VR behavior, its implementation and observed behavior are the primary source of truth for the Overture path. The Framework should extract game-neutral policy and adapt only the game-specific mechanism.

## 2026-09-16 implementation checkpoint

The current tree keeps the shared play-mode policy, identified tracking samples
and yaw epochs, same-tick body transaction, generation-safe crouch/holds and
partial probe lifecycle rollback behind their documented evidence levels. PID
25484 supplies focused headset evidence for the current crouch/Y, direct
locomotion and rejected-direction short-X/Z comfort composition. The extracted
hand-contact mathematics is shared with Overture; Black Plague has pinned
exact-image shape-query/callback/lifetime evidence, PID 28412 live-tested the
default-off no-write query and PID 8644 live-tested the corrected backend-owned
palm-shape lifecycle plus isolated Rework-derived resolver. Gameplay hand
publication is now connected and host-tested, including per-hand held-body
exclusion plus distinct `Grab=6` and free-body `Move=2` ownership. PID 23000 is
inconclusive because severe FPS loss and a right-controller dropout occurred in
the same headset run, so palm gameplay remains below headset-validated/supported.
PID 4720 later showed why the focused palm run also cannot be used to judge the
known-good room-scale path: its helper had room-scale, physical displacement and
positional translation disabled. The helper now composes those gates. A fresh
`23c890f` comparison also restored Rework's bounded raw-controller acquisition
pose while leaving visible/held palms collision-resolved.

Penumbra VR is GPLv3-or-later and records adapted components/provenance in `THIRD_PARTY.md` and the relevant product/source notes.

## Porting rule

For every demonstrated Rework feature:

1. locate the exact implementation and tests;
2. identify game-specific dependencies;
3. extract genuinely game-neutral behavior into shared runtime/interaction policy;
4. expose the narrowest per-game adapter for HPL/entity/physics/render differences;
5. preserve proven behavior/constants unless target evidence requires a measured difference;
6. validate the adapted path independently.

A new algorithm is justified only when Rework has no equivalent, the target exposes an incompatible interface, or evidence shows the original cannot safely be adapted. Record that reason before replacing proven behavior.

## Bidirectional reuse rule

Rework is the historical and behavioral reference for Overture, not a mandate that every shared subsystem copy Overture forever.

When another backend demonstrates a stronger **game-neutral** implementation, preserve the better behavior and move that behavior toward shared runtime policy while keeping game-specific details behind profiles/adapters.

Current example:

- Black Plague finger articulation preserves independent per-finger curls, per-joint curves, spread and thumb opposition and is the better candidate shared articulation output.
- Rework's game-neutral input conditioning is now shared: the measured `0.08`
  skeletal deadzone, grip/trigger fallback closing windows and ~70 ms smoothing
  live in `vr_hand_pose.*` and are host-tested through both Overture and the
  Black Plague skeletal path.
- Overture keeps rig/profile details such as bind poses, bone axes, handle poses
  and forced-grab presentation. Black Plague keeps its richer shared
  articulation semantics.
- Black Plague does not yet expose normalized grip/trigger analogs through the
  Framework action layer, so its non-skeletal visual fallback remains local
  instead of inventing analog data.

## Portability boundary

| Rework subsystem | Unified destination | Expected reuse | Per-game work still required |
|---|---|---|---|
| OpenVR poses, optics and compositor lifecycle | shared runtime | high | backend chooses frame/render boundary |
| tracking-to-world, yaw recenter and height policy | shared runtime | high | backend supplies body/camera mechanism |
| accepted body displacement / reconciliation policy | shared runtime | high | backend publishes intent and reports accepted movement through its native body boundary |
| accepted-body VR footstep cadence | shared runtime | high | backend supplies collision-accepted VR displacement and invokes its native surface/material footstep mechanism |
| logical VR input state, action sets and haptics | shared runtime | high | backend maps intents to each game's actions |
| generated bindings for PS VR2, Index, Touch, Pico, WMR and Vive | shared package | nearly direct | bindings are only the starting point; each profile must reach Overture-equivalent action/handedness/pose/menu/haptic behavior, with finger articulation where the hardware exposes it, and be validated per backend |
| settings types, limits and defaults | shared runtime | high | backend/installer supplies storage/UI |
| render scale and per-eye targets | shared runtime | high | backend owns renderer/GL integration |
| hand pose validity, aim/grip poses and finger articulation output | shared runtime | high | backend owns model/bone rig profile |
| palm dimensions, sweep/refinement and recovery policy | shared interaction layer | high at algorithm level | backend wraps HPL physics/contact queries |
| pointer/raycast intent | shared interaction layer | high | backend maps entity queries/focus/line drawing |
| held-object pose, throw velocity and haptics | shared interaction layer | medium/high | backend maps ownership/body APIs |
| flashlight/glowstick grip presentation | shared HPL behavior/config | medium/high | verify resource geometry/light/inventory per game |
| room-anchored menus, subtitles and cinematics | shared HPL behavior | medium | map draw order/menu state per game |
| HRTF, occlusion and environmental reverb | shared audio runtime | medium/high | validate proxy/hook/world queries |
| Enhanced visuals v4 calibration/final curve/halo math | shared visual runtime and shader package | high | binary games still need renderer-stage hooks |
| Large Address Aware requirement | unified installer | direct policy reuse | patch only known x86 PE32 hashes with backup/rollback |
| backup, hash verification, repair and rollback | unified installer | high | manifests/payload differ by game/build |
| Overture melee/weapon state machines | Overture backend only | none for current BP/Requiem scope | do not burden shared API with unused combat concepts |
| executable addresses, layouts and direct C++ calls | exact-build backend manifests/adapters | none | research BP/Requiem independently |

## Overture extraction state

The first Overture backend milestone is complete as `pvr_overture_backend`.

Shared/runtime behavior now includes:

- tracking space;
- seated/standing calibration;
- world yaw/recenter;
- room-scale rejection/reconciliation;
- fixed `0.05 m` physical steps;
- Rework locomotion policy of `1.5 m/s` walk and `2.25 m/s` sprint;
- Rework's `>0.85 m` horizontal accepted-body VR footstep cadence, with native
  sound/material selection deliberately left to each backend;
- common interaction reach of `0.18 m`;
- the proven palm dimensions, contact tolerances, sweep/refinement limits and
  recovery thresholds from `VRHandCollisionPolicy.h`, now owned by
  `src/runtime/vr_interaction_policy.hpp` and consumed by Overture through a
  compatibility namespace;
- accepted-body-motion observation through `runtime::VrAcceptedBodyMotion`;
- stateless body reconciliation planning, physical rejection correction and locomotion-anchor carry in `vr_locomotion.*`.

`OvertureBodyAdapter` keeps source-game HPL body position, feet height, collision movement and jump calls behind the game boundary.

The Framework-owned source host under `products/overture` builds and packages independently of the Rework checkout. The exact autonomous Release artifact has completed an initial SteamVR/headset/controller pass with no evident regression versus the previously tested Rework behavior. Exhaustive feature/hardware equivalence remains a separate evidence gate.

The autonomous Overture Release pipeline is now also a dedicated Windows CI regression gate. After the shared reconciliation extraction, a clean Windows 2022 job passed `Build-OvertureProduct.ps1 -Configuration Release -Full`, retaining the existing project/shader/visual/texture/LAA/`VRTrackingTest` checks.

## Black Plague body boundary state

The original binary movement problem is no longer an unmapped blocker.

Live evidence establishes:

- `cPlayer+0x274` → native `iCharacterBody`;
- standing body `0.70 x 1.65 x 0.70 m`, cylinder radius `0.35 m`;
- native body tick `D460A -> D6E00` at `1/60`;
- horizontal request/collision/accepted displacement;
- free movement, total blocking and sliding/partial acceptance;
- physical crouch body/shape swap from `1.65 m` to `0.95 m` preserving feet Y;
- native Jump-state vertical ownership separate from the horizontal request/solver path.

The first narrow `BlackPlagueBodyAdapter` is live-tested.

Ownership is intentionally singular:

- `NativeInputBridge` owns the movement callsites;
- `BodyCollisionProbe` owns `D460A -> D6E00`;
- the body adapter binds to their verified status/fan-out and does not install a competing hook.

The adapter dynamically re-resolves the current body, publishes the existing native horizontal movement call and observes accepted body motion after the original native update. It never invokes `D6E00` itself.

`runtime::VrAcceptedBodyMotion` is therefore the first concrete body contract used by both Overture and Black Plague without importing game-specific layouts, speeds or update ownership into shared runtime code.

## What is deliberately not shared yet

Do not force these into the first common body contract:

- Black Plague game-state, bob or animation effects, and the exact-build native
  `FootStep` ABI; only Rework's game-neutral accepted-travel cadence is shared;
- BP native `3.0 / 4.5 m/s` limits into shared policy;
- Black Plague jump/vertical state;
- Black Plague crouch transition/stand-clearance mechanism;
- Overture `vr_velocity` or `vr_stepstaticonly`-style source fields;
- RVAs, player/body offsets or calling conventions;
- camera/head-bob ownership;
- Requiem assumptions.

The first shared contract exists to separate **policy** from **native mechanism**, not to flatten every game difference immediately.

## Current priority — Black Plague focused headset validation

The body/collision/adapter mapping milestone is complete enough that more probing should require a concrete contradiction. The shared reconciliation extraction and its default-off Black Plague shadow consumer are live-tested through PID 28172. The bounded physical X/Z request at exact-build RVA `0xD7281` is live-tested through PID 26144 with positional translation still zero. PID 21548 proved that the combined-tick carry correction removed locomotion from in-place head tilt. PID 13672 then headset-exercised render-rate anchor placement and reported the prior continuous shake gone. PID 11804 confirmed that current HMD heading chooses the correct stick direction without recenter, but exposed speed inherited from the hidden signed native body axes. Rework's shared direct `1.5/2.25 m/s` displacement is adapted to Black Plague's one native tick. PID 28996 exposed the final indexed-state mapping defect in its permission predicate, and PID 8092 supplied headset evidence for the corrected stick/collision route. PID 20520 then exposed two remaining boundaries: the first physical-crouch edge mapping failed to synchronize standing, and short physical X/Z motion could still feel like a pullback. PID 22096 subsequently proved the single-consumption presentation sequence through sustained headset gameplay with zero stereo failures and narrowed the pullback to render-rate prediction continuing briefly into a direction already rejected by the previous physical solve. PID 25484 then supplied positive headset evidence for the corrected desired/native crouch path, continuous tracked Y, direct locomotion and the rejected-direction comfort filter. The remaining focused posture work is the uncaptured Hybrid release-hold interval plus blocked-stand/low-ceiling recovery; deliberate wall/slide remains a separate edge gate.

PID 23260 narrowed the crouch failure further. Physical height transitions and the
button latch both reached shared policy, but the native body stayed at `0.95 m`
with no native exit because Black Plague's exact crouch entries preserve legacy
pressed/released hold/toggle semantics. PID 24948 then proved that compensating
inside those callbacks was still insufficient: the native body alternated
between standing/crouched shapes (`native_entries=14`, `native_exits=14`) while
the game did not hold the expected crouch/stealth state. Exact-build decoding
identifies `cPlayer::ChangeMoveState` at `0x9C750`; Black Plague's own normal
crouch handlers prove state `4` is crouch and `0` is walk. The current backend
therefore ports Rework's actual ownership model: shared runtime owns one desired
state and the existing game-thread backend applies it directly with
`ChangeMoveState(4/0)`. Native body replacement and clearance remain
authoritative. PID 25484 provides focused headset evidence for stable state-4
crouch/stealth, physical entry/exit, final standing state and tracked Y; the
Hybrid release-hold and blocked-stand/low-ceiling edges remain open.

Current order:

1. Preserve the completed Overture source host and validated artifact behavior.
2. Preserve the live-tested single-owner Black Plague body adapter boundary.
3. Preserve the live-tested shared reconciliation/shadow implementation and keep active Black Plague translation default-off outside its dedicated validation request.
4. Preserve shadow-only mode as observation-only; its plan is not evidence that the dedicated physical request was injected.
5. Preserve the live-tested `0xD7281` bounded X/Z injection boundary, its `0.05 m` horizontal clamp, body/generation matching, one-shot semantics and single native tick ownership.
6. Preserve the corrected combined-tick partition: reconcile matched physical X/Z once, then carry the anchor only with the remaining native locomotion while retaining actual `body_after` for camera space.
7. Preserve the new direct locomotion adaptation: game-neutral HMD direction and
   `1.5/2.25 m/s` policy stay in runtime; exact-build Black Plague action-state
   indices `1` (Push) and `2` (Move) select the same shared constrained
   `0.5 m/s` policy. The exact pre-Move state predicate,
   single-tick `0xD7281` injection, `+0x264` post-publication state mirror and
   accepted-component partition stay in the Black Plague backend. PID 17612
   proved that `+0x264` cannot be used as the pre-publication permission oracle
   after the VR native axis is zeroed. This differs from Rework's two sequential
   body updates because the target exposes only one live-tested native update
   owner. The new constrained-state mapping is host-tested only and must not
   inherit PID 8092's headset status.
8. Preserve PID 8092 as headset evidence for corrected direct locomotion and the collision route, PID 22096 as positive presentation-sequence evidence, and PID 25484 as positive evidence for the current tracked-Y/crouch/short-X/Z comfort composition. Keep deliberate wall/slide and the exact PID 20520 pullback reproduction as narrower edge gates.
9. Close only the remaining crouch edges: capture Hybrid release-hold after the physical source clears, then exercise blocked-stand/low-ceiling recovery. Do not rerun the full PID 25484 batch merely to reproduce already captured evidence.
10. Preserve the live-tested BP palm query (PID 28412) and owned lifecycle/resolver (PID 8644). The current gameplay candidate is host-tested with full-hit multi-ray ranking, an Enter→committed-state hand latch, nudge exclusion during selection/acquisition and Rework's nearby physical interaction-assist skin computed from the resolver's actual start pose. Cleanly headset-validate that stack after reboot with the corrected combined helper: room-scale/crouch must remain active while both hands exercise representative `Grab=6` and free-body `Move=2` props, wall/slide contact and one native jointed mechanism. Correlate any abrupt palm snap with the new reanchor/recovery telemetry before changing shared thresholds. PID 23000 is inconclusive because FPS and one controller failed; PID 4720 launched the old palm helper without room-scale and therefore cannot be used as regression evidence for that stack.
11. Continue mechanism-state and definitive tool/light profile as separate
    evidence gates. Tracking-world-yaw ownership is already code-aligned with
    Rework and the accepted-body VR footstep cadence is host-tested; keep their
    directed headset checks plus final camera/body-bob presentation open.
12. Close the remaining Overture capability families recorded in
    `TRILOGY_PARITY_PLAN.md`: Black Plague now consumes the shared magnetic-item
    policy through a host-tested exact-build adapter, so headset validation is
    the remaining magnetic gate. Mechanisms, remaining UI/transitions, haptics,
    audio and compatible visual behavior must still be consumed, adapted or
    explicitly classified before Black Plague is used as the second-backend
    framework proof.
13. Make Requiem gameplay the active milestone only after that Black Plague
    framework-readiness gate closes. Repeat exact-build binary research wherever
    evidence cannot safely transfer, then consume the same shared capability
    families through narrow Requiem adapters/profiles.

This order proves each boundary in isolation and prevents a game-adapter defect from being mistaken for a shared-runtime defect.

## Black Plague interaction priority

Generic controller picking keeps the shared `0.18 m` bounded physical policy. Rework `23c890f` additionally lets acquisition follow the raw controller by at most `0.18 m` from a collision-stopped palm; Black Plague now ports that intent pose for selection and acquisition guards while visible hands and owned bodies remain on the resolved palm. The widened five-ray refresh enumerates every native ray hit before ranking candidates, and the native Enter→state-publication gap retains the originating VR hand while the button remains held. A nearby ordinary physical target also feeds Rework's bounded interaction-assist policy: only controller motion toward a target within `0.40 m` raises the collision skin from `0.002` to `0.008 m`, using the resolver's post-recovery/reanchor start. Free-body grabbing uses shared palm-relative pose/release behavior, but jointed mechanisms remain backend-owned.

Rework's direct physical hand nudge is now adapted behind the Black Plague
physics boundary as a host-tested candidate. The backend reuses one `0.12 m`
sphere per physics world, queries from the raw moving hand, ignores held/player
bodies and applies the same contact-velocity-aware, bounded mass-scaled impulse
shape where the target exposes equivalent evidence. Black Plague does not yet
have pinned entity-kind, lock/breakable, body-radius or joint-axis/type access at
this boundary, so those Rework refinements are not copied from Overture memory
layouts or guessed. Mechanism state remains native until those interfaces are
mapped.

Long boards/bars following the palm rigidly are a limitation of the free-body pose model, not evidence that arbitrary springs should be added. Doors/levers/sliders need their native mechanism state mapped.

Glowstick/flashlight placement remains geometry-specific, but the two BP tool cases now have different evidence. The full Rework and Black Plague DAE files differ, so flashlight geometry remains BP-specific and keeps its measured socket. The primary cylinder position stream of the Black Plague glowstick, however, is numerically identical to Rework's proven asset. That narrow geometry match justifies reusing the exact Rework glowstick model profile: `VrScale=1.55`, `VrGripPoint=(0,0.0078,-0.078)` and X rotation `4.71`, composed after the authored long-finger attachment grip. This is host-tested; final hand placement, apparent scale and light direction still require the headset gate.

The actual Rework hand rig assets carried by the Framework-owned Overture
product now feed a narrow renderer-owned import for Black Plague. A deterministic
generation tool validates the two DAE rigid skins and emits the geometry, bind
hierarchy/inverse binds and reduced diffuse material as checked-in renderer data;
runtime code therefore needs no DAE parser or new HPL hook. The adapter maps
Black Plague's richer game-neutral `VrHandArticulation` output onto the proven
Rework rig axes, so the imported geometry does not regress finger semantics to
Overture's simpler pose policy. Generated draw topology deduplicates the authored
18,102 triangle corners to 3,376 position/UV vertices plus uint16 indices, and
the client-array path explicitly isolates/restores HPL1 VBO bindings before
supplying CPU pointers. Tool-held presentation now also consumes Rework's authored
forced grip pose rather than merely maxing the normal curl channels with one
scalar. This path is host-tested; headset presentation and frame pacing remain
open.

## Extracted so far

- `src/runtime/vr_tracking_space.*`, `src/runtime/vr_locomotion.*`, `src/runtime/vr_interaction_policy.*` and `src/backends/overture/*` port the proven Overture/Rework tracking/body policy behind a narrow source-game adapter.
- `src/runtime/vr_haptics.hpp` owns Rework's gameplay-level haptic event
  profiles, strength scaling and per-event cooldown semantics. Overture keeps
  pose validity and OpenVR submission in `cVRHaptics`; Black Plague reuses the
  same policy at its existing native-input submission boundary for pickup/drop,
  tracked UI selection, successful direct hand nudges and LightToggle. Light
  feedback observes the exact native flashlight/glowstick state across the
  existing update owner and uses Rework's off-hand routing only after a real
  toggle. BP also owns all ten direct `cPlayer::Damage` callsites through one
  exact-build wrapper, preserves the native damage method, and emits shared
  Damage feedback only when native health decreases. Those LightToggle/Damage
  boundaries are exact-image verified and host-tested. Melee impact now consumes
  three demonstrated post-contact BP owners: enemy `cGameEntity::Damage` at
  `0x603D5` and `HitBody (0x5F000)` at `0x60747/0x608CF`. The generic wrappers
  preserve the native helper's enemy-type exclusion and all routes emit the
  shared event only after returning from the native owner. This mapping is
  exact-image verified and host-tested; actual controller feedback remains a
  headset gate.
- `src/runtime/vr_panel_policy.hpp` owns the reusable stable-panel anchor
  lifetime and transient overlay ownership handoff. Black Plague consumes the
  stable-anchor plan for its tracked menu; Overture consumes the overlay handoff
  for radio/subtitle presentation while draw order, transforms and menu state
  remain game/backend-owned.
- `src/runtime/vr_action_input.*` and `src/runtime/vr_math.*` now own the
  demonstrated game-neutral tracked-menu pointer policy: configured-hand
  preference with off-hand takeover, aim-to-grip fallback, bounded panel-plane
  projection with edge clamping and Rework's `0.40` screen-pointer smoothing.
  Black Plague consumes these helpers while the native 800x600 cursor write and
  tracked-menu anchor remain backend-owned. This extraction is host-tested only.
- `src/graphics/rework_hand_mesh.*` owns the renderer-side Rework hand
  geometry/rig consumer. The generated data originates from the already-carried
  Overture product assets; Black Plague supplies only resolved palm placement
  and shared articulation intent. No exact-build renderer hook is introduced.
- `src/runtime/vr_interaction_policy.hpp` now also owns Rework's demonstrated
  palm collision dimensions, contact/sweep/refinement constants and recovery
  predicates plus the bounded ordinary-target interaction-assist tolerance.
  Overture's `VRHandCollisionPolicy.h` is a compatibility shim, so
  future Black Plague/Requiem palm adapters can reuse the same policy while
  keeping native collision queries and body exclusions game-specific.
- `src/runtime/vr_magnetic_pickup_policy.hpp` owns Rework's item-only magnetic
  targeting policy: the three demonstrated range/bias profiles, aim-cone and
  scoring math, top-five ranking limit, closest-AABB visibility sample and
  `0.03 m` sight overshoot. Overture consumes it directly while its item enum,
  portal traversal and dual hand/HMD physics raycasts remain product-owned.
  Black Plague now consumes the same policy through its exact `PhysicsWorld`
  body list, real `cGameItem` subtype mapping, embedded body BV and existing
  native-pick ray owner. That second consumer is host-tested only; its exact
  layouts remain backend-owned and headset evidence is still required.
- `src/runtime/vr_mechanism_policy.hpp` owns Rework's demonstrated
  unconstrained, slider and hinge servo math plus the associated motion caps.
  Overture consumes it directly while native joint graph selection, HPL body
  writes and entity/mass-specific hinge lightness remain product-owned. Black
  Plague now consumes the same policy for verifier-pinned one-joint mechanism
  families: `cGameLever` accepts its demonstrated hinge/slider forms and
  `cGameSwingDoor` accepts only a hinge. Native `Move::Enter/Leave` keep HPL
  lifecycle ownership while the recognized `Move::Update` uses the shared
  servo. Unknown, Wheel and multi-joint mechanisms remain native. These
  consumers are host-tested only.
- `src/backends/black_plague/hand_contact_probe.*` owns a default-off diagnostic
  for the now-pinned BP `CheckShapeWorldCollision` ABI. It fans out after the
  existing native body update, reuses the current body shape, records callback
  contacts and verifies selected native bytes remain unchanged. Its synthetic
  harness is host-tested and PID 28412 live-tested the no-write query. The same
  backend now owns the shared-size palm box lifecycle and feeds native contacts
  into the shared Rework-derived resolver. PID 8644 live-tested that isolated
  lifecycle/resolver behind `--validate-palm-resolver`. Exact-image evidence now
  also pins BP's independent character and exact-`skip_body` filters, and the
  host resolver harness locks the Rework-compatible argument contract. The
  host-tested gameplay integration now publishes the held body per hand and
  consumes the resolved palm for visible hands, interaction and tools while aim
  remains raw. Its optional target-provider boundary lets the interaction owner
  supply the latest nearby physical selection without coupling the resolver to
  exact-build picking; the shared resolver computes the Rework assist decision
  after selecting its actual recovery/reanchor start and exposes assist/reanchor/
  recovery telemetry. The interaction adapter also distinguishes native `Grab=6` from
  `Move=2`: eligible free Move bodies preserve their picked contact and use the
  Rework-derived force path; recognized one-joint `cGameLever` and hinge-only
  `cGameSwingDoor` bodies use the dedicated shared-servo adapter while
  unsupported jointed mechanisms stay native. PID 23000 is
  inconclusive because FPS and the right controller failed in the same headset
  session, so clean headset contact/performance validation remains pending.
- `runtime::VrAcceptedBodyMotion` now supplies the first body observation shared by Overture and Black Plague.
- `PlanBodyReconciliation`, `ReconcilePhysicalBodyMotion` and `CarryHeadAnchorWithLocomotion` now provide the shared stateless reconciliation phases used by Overture and the Black Plague shadow consumer.
- `src/runtime/render_target_policy.*` preserves Rework render-scale defaults/fallback.
- `src/graphics/visual_calibration.*` remains the CPU reference for the accepted
  v4 tone/ambient/dark-diffuse/sharpening/glowstick-halo behavior. The
  transferable per-eye stage is now consumed by Black Plague through
  `OpenGlEnhancedEyeStage`: RGBA16F intermediate, 2x MSAA resolve and the exact
  v4 final sharpen/tone treatment are host-tested on a real WGL driver with
  graceful fallback. Rework's VR-specific HPL ambient/light material programs
  remain a separate target-owned renderer/material adaptation; do not treat the
  eye post stage as full Enhanced Visuals parity.
- `src/audio/spatial_audio.*` owns HRTF config text, distance/occlusion low-pass
  behavior and the complete accepted mine-gallery EFX preset, including echo,
  modulation and room-rolloff fields. Black Plague now consumes the HRTF portion
  at the launcher-owned pre-audio boundary by writing the exact Rework-format
  `alsoft.ini` before a fresh game launch; this is host-tested only. Static
  exact-image work confirms BP already owns a native OpenAL/EFX environment
  stack, but its initialization does not implement Rework's added preset/bus
  trim and the distance-HF path has no demonstrated BP equivalent, so safe
  runtime occlusion/reverb adaptation remains open.
- `src/deployment/pe_large_address.*` performs the pure PE32 LAA transformation; the build catalogue/manifests recognize host-verified exact Black Plague/Requiem LAA variants while transactional deployment remains installer work.
- `src/adapters/hpl1/camera_matrix_override.*` owns the reusable byte-exact camera transaction; exact layouts remain backend-owned.
- `src/runtime/stereo_render_policy.*` makes the optional monitor mirror explicit, but the mirror is not supported/validated.
- `src/runtime/vr_grab_pose.*` preserves palm/body pose and bounded release behavior and owns the shared attachment-socket composition. Black Plague keeps a resource-specific flashlight socket; its glowstick instead consumes the exact Rework model scale/grip/rotation profile because the relevant primary cylinder geometry was verified identical. The composition is host-tested; definitive tool placement is still a headset/geometry gate.
- `src/runtime/vr_input_state.*` owns logical actions, dead-zone scaling, context/handedness edge latching, pose-loss releases and action-idle grace.
- `src/runtime/vr_settings.*` owns Rework-derived defaults, ranges, enum semantics and legacy migration plus framework mirror state; `vr_settings_editor.*` owns the demonstrated 18-row edit/format/dependency policy, and `vr_settings_store.*` persists the complete shared schema.
- Black Plague exposes an explicit backend capability map for the currently wired editor settings. A dedicated in-game settings page remains game-specific work until a safe native-menu insertion boundary is demonstrated.
- `assets/openvr` contains shared actions/bindings; `assets/openvr/overture` preserves exact Overture product mappings.

## Current host-only extraction frontier

The remaining Rework-specific VR code was re-audited on 2026-09-16 after the
magnetic-pickup and mechanism-servo extraction. Those two pure policies are
host-tested through an isolated regression target and the full Overture Release
gate. Further extraction currently needs either a second backend consumer or a
target-specific ownership boundary:

- Rework's VR dimmer has a separable scalar ramp (`0 <-> 0.75` at `1.75/s`),
  but its demonstrated render ownership is specifically after the world and
  before inventory/subtitle drawing. Black Plague's host-tested inventory and
  notebook path is structurally different: it captures the complete native
  desktop frame and projects that UI as a stable panel over its own dark VR
  background. Applying the Rework dimmer to that captured surface would darken
  the panel itself, while drawing it behind the panel would duplicate a
  background already owned by the tracked-menu presentation. Treat the
  inventory/notebook dimmer as not directly applicable to that presentation
  path. Keep the scalar policy product-owned until a second demonstrated
  consumer exists; gameplay message/transient-overlay dimming remains open
  until BP exposes the corresponding native active-state boundary.
- staged loading/fade is coupled to Overture's synchronous map load and
  compositor presentation lifecycle; a second backend presentation boundary is
  required before extracting it.
- physical crouch policy is now shared, while Black Plague keeps its exact
  `cPlayer::ChangeMoveState(4/0)` adapter, move-state/body-shape observation and
  stand-retry mechanism in the backend. A second backend is not needed to
  generalize that exact native boundary; headset and blocked-stand evidence are
  still required before promotion.
- inventory/notes/HUD/subtitle integration still depends on each game's menu,
  draw-order and native state boundaries. The reusable panel/pointer/settings
  policy already extracted should be consumed once those boundaries are mapped.
  Magnetic item targeting is already separated and Black Plague now consumes it
  through an exact-build item classifier and dual native visibility boundary;
  representative headset evidence remains open.
- shared Rework mechanism servo math now has verifier-pinned, host-tested Black
  Plague consumers for one-joint `cGameLever` and one-joint hinge
  `cGameSwingDoor`. Exact Rework `23c890f` shows SwingDoor explicitly entering
  `Move`, pausing controllers/gravity and treating its joints as hinges; the BP
  image independently pins type `8`, the same lifecycle flags and its base
  joint collection. Wheel remains intentionally excluded because BP type
  `0x13` owns dedicated joint/state fields and a substantial native update; the
  exact-image verifier pins vtable `0x6793E8`, `Update=5ABB0`, dedicated joint
  `+0x23C` and state `+0x244/+0x250` as an exclusion boundary.
  Compound joints and further mechanism families still require target evidence.

This is a deliberate host-only extraction frontier, not completion of the
corresponding trilogy capability. Resume extraction/adaptation when Black Plague
evidence exposes the necessary boundary. A policy that exists only in shared
runtime and Overture remains explicitly open in `TRILOGY_PARITY_PLAN.md` until a
second backend consumes it or an evidence-backed non-applicable/incompatible
classification is recorded.

Controller support is a behavioral parity requirement, not an asset-presence check. The shared package currently carries eight default OpenVR profiles: PS VR2 Sense, Vive, Valve Index/Knuckles, Oculus/Meta Touch, Pico 4, Pico Neo 3, Windows Mixed Reality motion controllers and the holographic-controller variant. A profile is only considered at parity after its logical actions, left/right-handed routing, grip/aim poses, menu and picking controls, haptics and any hardware-supported finger articulation behave equivalently to the proven Overture baseline in the target backend. Black Plague must close that matrix before controller parity is claimed; Requiem should inherit the same matrix and repeat only backend-specific validation.

The metadata gate now compares the shared action manifest and the functional graph of every shared controller binding against the preserved Overture profile set. Descriptive text may differ, but action sets, source routing, grip/aim poses, skeleton inputs and haptic outputs cannot silently drift. This host gate also restored the Rework-proven left PS VR2 Sense trigger route for UI select after detecting that it had been dropped from the shared profile. It establishes static/profile parity only; live SteamVR routing, device pose behavior, haptics and finger articulation still require the corresponding hardware/backend evidence.

## Validation rule

Do not promote a shared behavior merely because it compiles in both backends.

Use the evidence ladder consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

For any reported regression, compare:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

The next Black Plague body work should consume existing evidence, not restart it.

## Tracking/body extraction checkpoint

The plan/rebase, physical rejection correction and native anchor carry phases
have moved from `OvertureBackend` to `vr_locomotion.*`. Windows host gates and
the autonomous Overture Release regression pass. Black Plague subsequently
live-tested the shadow path, the bounded `0xD7281` request and the combined
room-scale/direct-locomotion transaction.

Subsequent work live-tested that shadow path in PID 28172 and the separate
default-off `0xD7281` bounded X/Z request in PID 26144. PID 21548 proved the
combined-tick carry correction and PID 13672 headset-exercised the later
render-rate anchor placement; the user reported the prior continuous shake gone.
PID 11804 then confirmed that tracking-only HMD direction fixes stick heading
without recenter while retaining stable presentation. It also proved that a
world-vector remap through native `MoveForward/MoveSideways` is insufficient:
speed still followed the signed hidden body axes. The transient room-scale path
now requests Rework's direct `1.5/2.25 m/s` displacement through the existing
single `0xD7281` collision owner. Because Black Plague cannot safely copy
Rework's two source-level body updates, physical and stick requests are combined
within the proven `0.05 m` boundary and the accepted result is partitioned for
physical reconciliation and locomotion carry. PID 8092 supplies headset evidence
for that direct-locomotion route, while PID 20520 keeps short-range physical X/Z
comfort open. Physical crouch/continuous tracked Y are now implemented and
host-tested together. Rework tracking-world-yaw turn ownership is already
consumed by the current input bridge at code level, while directed headset
validation remains a separate gate.
