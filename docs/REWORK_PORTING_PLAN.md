# Penumbra VR Rework porting plan

This plan treats `rubocopter/penumbra_vr_rework` revision `23c890f` as the proven Overture reference. When Rework already solves a VR behavior, its implementation and observed behavior are the primary source of truth for the Overture path. The Framework should extract game-neutral policy and adapt only the game-specific mechanism.

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
- Overture contributes rig/profile details such as bind poses, bone axes, deadzone/smoothing and handle poses.
- Future work should adapt Overture to the shared articulation output, not regress Black Plague to Overture's simpler semantics.

## Portability boundary

| Rework subsystem | Unified destination | Expected reuse | Per-game work still required |
|---|---|---|---|
| OpenVR poses, optics and compositor lifecycle | shared runtime | high | backend chooses frame/render boundary |
| tracking-to-world, yaw recenter and height policy | shared runtime | high | backend supplies body/camera mechanism |
| accepted body displacement / reconciliation policy | shared runtime | high | backend publishes intent and reports accepted movement through its native body boundary |
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

- Overture `1.5 / 2.25 m/s` speed constants into Black Plague before a dedicated VR locomotion gate;
- BP native `3.0 / 4.5 m/s` limits into shared policy;
- Black Plague jump/vertical state;
- Black Plague crouch transition/stand-clearance mechanism;
- Overture `vr_velocity` or `vr_stepstaticonly`-style source fields;
- RVAs, player/body offsets or calling conventions;
- camera/head-bob ownership;
- Requiem assumptions.

The first shared contract exists to separate **policy** from **native mechanism**, not to flatten every game difference immediately.

## Current priority — Black Plague physical displacement mechanism

The body/collision/adapter mapping milestone is complete enough that more probing should require a concrete contradiction. The shared reconciliation extraction is host-tested and its default-off Black Plague shadow consumer is live-tested through PID 28172 with positional translation still zero.

Current order:

1. Preserve the completed Overture source host and validated artifact behavior.
2. Preserve the live-tested single-owner Black Plague body adapter boundary.
3. Preserve the live-tested shared reconciliation/shadow implementation with Black Plague positional HMD translation at zero.
4. Do not treat the shadow plan as accepted/rejected physical motion; no physical request is injected.
5. Identify and demonstrate a bounded collision-aware physical X/Z displacement request in metres, consumed by the existing single native tick. Native analog intent is not that capability.
6. Only after that physical boundary has separate evidence should active room-scale/positional tracking be enabled for live/headset validation.
7. Decide Black Plague VR walk/sprint tuning separately; do not use analog scaling as a substitute for Rework-equivalent displacement policy.
8. Keep physical crouch and jump comfort/tuning as separate milestones.
9. Treat camera/head-bob/footstep-bob ownership as a separate comfort track rather than a prerequisite for the initial body contract.
10. Continue palm collision, mechanism state and definitive tool/light profile work after the player-body path is stable.
11. Repeat exact-build binary research for Requiem wherever evidence cannot safely transfer.

This order proves each boundary in isolation and prevents a game-adapter defect from being mistaken for a shared-runtime defect.

## Black Plague interaction priority

Generic controller picking remains capped to the shared Rework direct physical reach of `0.18 m`. Free-body grabbing uses shared palm-relative pose/release behavior, but jointed mechanisms remain backend-owned.

Long bars/tables following the palm rigidly are a limitation of the free-body pose model, not evidence that arbitrary springs should be added. Doors/levers/sliders need their native mechanism state mapped.

Glowstick/flashlight placement is geometry-specific. Rework and Black Plague use different DAE resources, so Rework grip constants cannot be copied blindly. The correct direction is shared grip-profile semantics plus measured per-game sockets.

## Extracted so far

- `src/runtime/vr_tracking_space.*`, `src/runtime/vr_locomotion.*`, `src/runtime/vr_interaction_policy.*` and `src/backends/overture/*` port the proven Overture/Rework tracking/body policy behind a narrow source-game adapter.
- `src/runtime/vr_haptics.hpp` owns Rework's gameplay-level haptic event
  profiles, strength scaling and per-event cooldown semantics. Overture keeps
  pose validity and OpenVR submission in `cVRHaptics`; Black Plague reuses the
  same pickup/drop profiles at its existing native-input submission boundary.
- `src/runtime/vr_panel_policy.hpp` owns the reusable stable-panel anchor
  lifetime and transient overlay ownership handoff. Black Plague consumes the
  stable-anchor plan for its tracked menu; Overture consumes the overlay handoff
  for radio/subtitle presentation while draw order, transforms and menu state
  remain game/backend-owned.
- `src/runtime/vr_interaction_policy.hpp` now also owns Rework's demonstrated
  palm collision dimensions, contact/sweep/refinement constants and recovery
  predicates. Overture's `VRHandCollisionPolicy.h` is a compatibility shim, so
  future Black Plague/Requiem palm adapters can reuse the same policy while
  keeping native collision queries and body exclusions game-specific.
- `runtime::VrAcceptedBodyMotion` now supplies the first body observation shared by Overture and Black Plague.
- `PlanBodyReconciliation`, `ReconcilePhysicalBodyMotion` and `CarryHeadAnchorWithLocomotion` now provide the shared stateless reconciliation phases used by Overture and the Black Plague shadow consumer.
- `src/runtime/render_target_policy.*` preserves Rework render-scale defaults/fallback.
- `src/graphics/visual_calibration.*` is the CPU reference for the accepted v4 tone/ambient/dark-diffuse/sharpening/glowstick-halo behavior; GPU renderer-stage integration remains pending.
- `src/audio/spatial_audio.*` owns HRTF config text, distance/occlusion low-pass
  behavior and the complete accepted mine-gallery EFX preset, including echo,
  modulation and room-rolloff fields; safe binary audio hook work remains open.
- `src/deployment/pe_large_address.*` performs the pure PE32 LAA transformation; the build catalogue/manifests recognize host-verified exact Black Plague/Requiem LAA variants while transactional deployment remains installer work.
- `src/adapters/hpl1/camera_matrix_override.*` owns the reusable byte-exact camera transaction; exact layouts remain backend-owned.
- `src/runtime/stereo_render_policy.*` makes the optional monitor mirror explicit, but the mirror is not supported/validated.
- `src/runtime/vr_grab_pose.*` preserves palm/body pose and bounded release behavior.
- `src/runtime/vr_input_state.*` owns logical actions, dead-zone scaling, context/handedness edge latching, pose-loss releases and action-idle grace.
- `src/runtime/vr_settings.*` owns Rework-derived defaults, ranges, enum semantics and legacy migration plus framework mirror state; `vr_settings_editor.*` owns the demonstrated 18-row edit/format/dependency policy, and `vr_settings_store.*` persists the complete shared schema.
- Black Plague exposes an explicit backend capability map for the currently wired editor settings. A dedicated in-game settings page remains game-specific work until a safe native-menu insertion boundary is demonstrated.
- `assets/openvr` contains shared actions/bindings; `assets/openvr/overture` preserves exact Overture product mappings.

Controller support is a behavioral parity requirement, not an asset-presence check. The shared package currently carries eight default OpenVR profiles: PS VR2 Sense, Vive, Valve Index/Knuckles, Oculus/Meta Touch, Pico 4, Pico Neo 3, Windows Mixed Reality motion controllers and the holographic-controller variant. A profile is only considered at parity after its logical actions, left/right-handed routing, grip/aim poses, menu and picking controls, haptics and any hardware-supported finger articulation behave equivalently to the proven Overture baseline in the target backend. Black Plague must close that matrix before controller parity is claimed; Requiem should inherit the same matrix and repeat only backend-specific validation.

## Validation rule

Do not promote a shared behavior merely because it compiles in both backends.

Use the evidence ladder consistently:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

For any reported regression, compare:

`REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION`

The next Black Plague body work should consume existing evidence, not restart it.

## Tracking/body extraction checkpoint (2026-09-11)

The remaining plan/rebase, physical rejection correction and native anchor carry
phases have moved from `OvertureBackend` to `vr_locomotion.*`. The BP consumer
is shadow-only and default-off: it cannot inject a physical request. Portable
tests and the Overture differential trace passed during implementation. Post-push
Windows CI then passed root metadata/Debug/Release tests and the autonomous
Overture Release regression gate. The exact Rework sequence, ownership table,
API and implementation-time test results remain in
[the internal report](internal/TRACKING_BODY_RECONCILIATION.md); the post-push
host result and next live gate are recorded in
[the validation follow-up](internal/TRACKING_BODY_VALIDATION_FOLLOWUP.md).
