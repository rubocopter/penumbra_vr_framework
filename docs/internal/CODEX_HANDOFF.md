# Codex handoff — Penumbra VR Framework

This is the operational checkpoint for future coding sessions. Read `AGENTS.md` first. Read `DEBUG_HANDOFF.md` before changing any known regression boundary, and use `docs/REWORK_PORTING_PLAN.md` for the extraction contract.

## Source-of-truth rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR. Do not invent a replacement for demonstrated Rework behavior before locating the original implementation and tests, separating game-neutral policy from game/HPL mechanism, and documenting why direct adaptation would be unsafe or impossible.

The framework is not a one-way Overture port. If another backend demonstrates a stronger game-neutral implementation, preserve the better behavior and move it toward shared runtime policy while keeping per-game layouts, rigs and native state behind adapters/profiles. Black Plague finger articulation is currently the clearest example.

## Repository checkpoint

- Working-tree maintenance pass on 2026-09-12: settings writes are transactional,
  IAT/OpenGL/spatial teardown reports partial state, OpenVR retains a loader
  handle after unload failure, renderer types and Black Plague body-owner
  contracts have narrower headers, and the probe/launcher expose installed
  capabilities explicitly. Stale state documentation is synchronized, `work/`
  is ignored and the redundant root patch is removed. These changes are
  `implemented` and statically reviewed only; no build or test executable was
  run during the pass.
- Follow-up lifecycle hardening in the same working tree: launcher failures
  after initialization now compensate with shutdown, while spatial interaction
  and native input teardown unhook first and then wait for active callbacks.
  This remains `implemented` and statically reviewed only; no new live or
  headset evidence exists.
- Current hook hardening pass: exact-build Black Plague hook owners now reject
  incomplete installation states, keep immutable published image bases, and
  avoid clearing native callback targets while wrappers can still be active.
  This is Release/build/test validated only; it does not add live or headset
  evidence.
- Repository: `rubocopter/penumbra_vr_framework`
- Default branch: `main`
- Reviewed feature checkpoint: `18c63ef39adb9d3af6df0a6e9f97d0889df5194c` (`feat: add shared tracking body reconciliation shadow`)
- CI hardening checkpoint: `ea12955f494976ed7e3fb2913a3da6221d7dcff8` (`ci: validate autonomous Overture regression on Windows`)
- Shadow launch hardening checkpoint: `48e07dd6033c13326a36a6cb5c91103620e716ee` (`feat: add transient Black Plague shadow validation launch`)
- Framework state: pre-alpha
- Windows/x86 remains the current binary-research target
- Rework `23c890f` is reference-only; Overture build/package no longer depends on that checkout

Always inspect `git status`, HEAD and origin synchronization before editing. Do not assume a local tree is clean just because this checkpoint is clean on GitHub.

## Overture

The autonomous Framework-owned Overture source/build/package integration is complete.

Proven/shared behavior includes:

- tracking space and calibrated height;
- seated/standing composition;
- world yaw/recenter;
- room-scale displacement rejection/reconciliation;
- fixed physical body steps of `0.05 m`;
- locomotion policy of `1.5 m/s` walk and `2.25 m/s` sprint;
- shared direct interaction reach of `0.18 m`.

`OvertureBodyAdapter` remains the source-level HPL boundary for body position, feet height, collision movement and jump. The concrete adapter is under `src/adapters/overture_source`; the Framework-owned host, dependencies and package tooling are under `products/overture`.

The tested autonomous Release executable is 3,302,912 bytes with SHA-256:

`D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`

On 2026-09-10 the user deployed the Framework package over a valid retail installation and completed an initial SteamVR/headset/controller pass. Perceived behavior matched the prior Rework build with no evident regression. This is initial headset validation, not exhaustive equivalence and not a supported-release claim.

After the shared tracking/body extraction, GitHub Actions run `34616820448` re-ran the autonomous Overture Release pipeline from a clean Windows 2022 checkout through `Build-OvertureProduct.ps1 -Configuration Release -Full` and completed successfully. The dedicated CI job is now a regression gate for future shared-runtime changes.

Do not reopen Overture source-host migration unless a concrete regression requires the `REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION` comparison.

## Black Plague — established boundaries

The supported research executable hash remains:

`FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF`

Previously headset-validated:

- native stereo;
- yaw-aligned rotational HMD tracking;
- keyboard/mouse preservation;
- conservative HMD-aware render-list visibility.

Implemented/code-tested but not automatically headset-validated:

- OpenVR actions and exact-build native input bridge;
- tracked menus;
- provisional procedural gloves;
- controller picking;
- palm-relative free-body grab/release/throw;
- `0.18 m` direct physical reach fallback;
- tool/light attachment groundwork;
- eye-scissor/light remapping.

Controller parity is an explicit framework requirement. `assets/openvr` ships
eight shared default profiles: PS VR2 Sense, Vive, Valve Index/Knuckles,
Oculus/Meta Touch, Pico 4, Pico Neo 3 and the two Windows Mixed Reality
controller types. Asset presence is not sufficient evidence. Before Black
Plague controller support is considered equivalent to Overture, each available
profile must cover the same logical actions and handedness routing plus grip/aim
poses, menu/picking behavior, haptics and hardware-supported finger articulation.
Requiem should inherit this shared matrix and repeat only the backend-specific
intent/headset validation needed by its exact build.

The host metadata gate now locks the shared action manifest and all eight
functional binding graphs to the preserved Overture mappings, ignoring only
descriptive binding text. That check caught and restored the missing left PS VR2
Sense `L2 -> /actions/ui/in/select` route. Treat this as host-tested static
profile parity; device behavior remains a live/headset validation gate.

The Rework tracked-menu pointer policy is also now extracted and host-tested.
Shared runtime selects the configured hand when its grip pose is valid, falls
back to that hand's grip when aim is unavailable, and lets the opposite tracked
hand take over when the configured hand loses tracking. Menu-plane projection
pins out-of-rectangle hits to the nearest edge and applies Rework's `0.40`
screen-pointer smoothing. Black Plague consumes this policy through its existing
tracked-menu/native-cursor boundary. All shared controller profiles already bind
both physical triggers to the active UI `select` action, so off-hand pointer
takeover retains selection routing without another input owner. Release build
and all 30 root CTest tests pass. This is not live/headset validation.

### Player/body and collision

This reverse-engineering milestone is closed unless contradictory evidence appears.

Confirmed exact-build structure and sequence:

- `cPlayer+0x274` → native `iCharacterBody*`;
- current/previous position at `+0x48/+0x54`;
- active size at `+0xC4`;
- active `iPhysicsBody*` at `+0x23C`;
- `iPhysicsWorld*` at `+0x240`;
- `iCharacterBody::Move` at `D4F50` updates native horizontal acceleration/speed state;
- normal physics boundary is `D460A -> iCharacterBody::Update(D6E00)`;
- first horizontal collision resolution is `D7312 -> CheckShapeWorldCollision(D4830)`;
- live standing shape is a `0.70 x 1.65 x 0.70 m` cylinder, radius `0.35 m`;
- measured physics tick is `dt=1/60`;
- free movement, total block, slide and partial acceptance are live-observed.

Do not create a Framework head collider. The real player body already owns collision.

### Sprint, crouch and jump ownership

Native walk/sprint limits are effectively about `3.0 / 4.5 m/s`. Do not treat analog-input scaling as equivalent to Rework's direct `1.5 / 2.25 m/s` displacement policy.

Native crouch is a real shape/body swap, not just a camera effect:

- standing: `0.70 x 1.65 x 0.70`;
- crouched: `0.70 x 0.95 x 0.70`;
- feet Y remains fixed;
- the active physics-body pointer changes and restores.

The exact Black Plague stand-clearance mechanism is not sufficiently demonstrated. Do not import Overture `CanStand` assumptions.

Jump remains native and separate from horizontal intent:

- state index `3` correlates to `cPlayerMoveState_Jump`;
- `EnterState` prepares vertical force before the next `D6E00`;
- first observed accepted vertical speed is about `+5.53 m/s`;
- apex is about `0.95 m` above baseline;
- landing is resolved in the native body tick, followed by state `3 -> 0`;
- horizontal `requested.y` and `solver.y` stay zero during the jump while final accepted Y changes;
- `+268` is a 25-tick ground-grace counter;
- `+26C` remains only an unclassified alternate jump-eligibility branch;
- `+204=0.3` is a hold threshold, not a hard clamp for `+200`.

Do not move vertical/jump ownership into the shared horizontal contract.

## BlackPlagueBodyAdapter — live-tested initial boundary

The first narrow adapter is implemented and live-tested on PID 8628.

Ownership is intentionally single-owner:

- `NativeInputBridge` owns the `MoveForward` and `MoveSideways` callsites;
- `BodyCollisionProbe` owns `D460A -> D6E00`;
- `BlackPlagueBodyAdapter` binds to those verified owners and receives fan-out; it does not stack another hook on either boundary.

The adapter:

- re-resolves the current `cPlayer+0x274` body on each publish/observe callback;
- publishes the already-computed native horizontal amount through the mapped native movement method;
- never calls `D6E00`;
- observes body/feet after the one native update returns;
- converts before/after positions into `runtime::VrAcceptedBodyMotion`;
- fails closed if the exact initialized owner state no longer matches.

PID 8628 demonstrated:

- all relevant owners and adapter installed together;
- free displacement;
- total blocking;
- sliding/partial acceptance;
- body replacement without stale-pointer caching;
- approximately 60 native body updates/s at `dt=0.016667`;
- no evidence of a second `D6E00` call.

`positional_translation_enabled=0` throughout that validation. Therefore the adapter boundary is live-tested, while the later active room-scale consumer still requires its own validation. VR speed tuning, physical crouch, jump tuning and camera/bob comfort remain separate.

## First shared body policy

`runtime::VrAcceptedBodyMotion` is the first body contract used by both Overture and Black Plague. It contains finite before/after positions and accepted displacement only. It must remain free of RVAs, HPL layouts, speed constants and native-update ownership.

Overture consumes it without changing its proven room-scale/locomotion behavior.
Black Plague produces it from the native tick; the live-tested validation path
feeds the matched physical observation into shared reconciliation. A separate
default-off active consumer can now expose the resulting horizontal anchor/body
offset to rendering during the next validation gate.

## Current milestone — active room-scale implemented; validation pending

The shared stateless phases live in `vr_locomotion.*`:
`PlanBodyReconciliation`, `ReconcilePhysicalBodyMotion` and
`CarryHeadAnchorWithLocomotion`. Overture calls them in its existing order.
A same-compiler 512-frame differential trace matches the checkpoint bit-for-bit.

BP feeds a default-off `BodyReconciliationShadow` directly from the existing
post-native-tick adapter callback, using raw tracking published at the existing
render boundary. `tools/Start-BlackPlagueShadowValidation.ps1` remains the
reproducible shadow-only diagnostic. The original
`PVR_BP_RECONCILIATION_SHADOW=1` mechanism remains supported only when the
**game process itself** actually inherits that variable. Sampling uses existing
owners, with periodic logs every 300 swap frames. The shadow-only mode leaves
positional translation at zero; active translation requires the separate
room-scale request described below.

On 2026-09-11 the adapter gained one-shot installation telemetry that records
whether shadow is disabled, requested from the game-process environment, or
requested from the transient mutex. Earlier PID 25784 crashed before native
bridge/body-adapter installation in `SDL.dll`, while PID 21048 later established
that the crash was not a stable startup blocker but ran with shadow disabled.
The launcher was then hardened to wait for the actual forwarded `LoadLibraryW`
owner DLL in a freshly Steam-started process.

The subsequent PID 28172 live session closed the shadow gate. The fresh process
reported `body_reconciliation_shadow enabled=1 source=mutex`, retained
`positional_translation_enabled=0`, exercised stationary/small physical head
deltas plus native free movement, blocking and sliding, observed body replacement
and recenter handling, and retained the existing ~60 Hz native body update with
no evidence of a duplicate `D6E00`. This promotes the Black Plague reconciliation
shadow path to **live-tested**, while active positional/body reconciliation is
still disabled and not headset-validated.

The host gate was green at that checkpoint. GitHub Actions run `34616035023` for feature commit
`18c63ef` passed metadata validation, Visual Studio 2022 Win32 configure and the
established Debug/Release CTest jobs. Run `34616820448` repeated that root gate
and also passed the new autonomous Overture Release regression job. That root
configuration contained 27 CTest tests; hosted CI intentionally excluded only
the real-driver `opengl_eye_targets` pixel test as documented in the workflow.

The collision-aware physical displacement boundary is now **live-tested**.
Exact-build RVA `0xD7281` is the pre-comparison injection point:
a bounded one-shot X/Z request (maximum horizontal step `0.05 m`, Y forced to
zero) is injected immediately before the native horizontal comparison/collision
path and is consumed by the existing single `D6E00` tick. The gateway preserves
the original `fld [edi]` / `fld [esi+54h]` instructions, registers and flags,
fails closed on owner/body mismatch, and never calls `D6E00` itself. Acceptance
is measured from the pre-injection body position so earlier native stick
locomotion in the same tick is excluded.

The dedicated path is default-off and can be activated by
`PVR_BP_PHYSICAL_DISPLACEMENT_VALIDATION=1` or the transient mutex held by
`tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1`. Local Release build,
all **30/30** root CTest tests and `tools/Test-BlackPlagueInputMap.ps1` against
the initialized exact-build capture passed with the new `0xD7281` verification.
PID 26144 advanced the physical request boundary through
**implemented → host-tested → live-tested**. The session retained
`positional_translation_enabled=0`, exercised queue/injection/matched
reconciliation with the existing native `dt~=1/60` tick, and produced free,
blocked and slide/partial physical outcomes. The original helper could not mark
`stationary` because it required literally zero physical injection; Rework
`23c890f` itself reacts to any non-zero HMD delta, so real headset jitter makes
that criterion invalid. Re-analysis with the corrected 2 mm significance
boundary yields 12 stationary/jitter samples, 37 free, 2 blocked and 15
slide/partial samples from the same PID.

The dedicated launcher remains fail-closed for future repetitions. It requires fresh
telemetry proving a non-zero queued physical plan, consumed/injected boundary
request, matched reconciliation, body telemetry with the non-zero physical
request/injection, the existing `dt~=1/60` native body tick and explicit sampled
stationary/free/block/slide-or-partial outcomes. Classification uses the existing
pre-injection request and accepted physical displacement, so no extra hook or
owner was introduced. The helper reports each captured outcome once while the
game is still running, then performs the complete fail-closed check on exit. PID
18392 proved the negative case: mutex activation with
positional translation zero but no physical request telemetry exits with failure
instead of being counted as live validation. PID 24484 likewise remains
activation evidence only.

The active room-scale path is now implemented behind the separate
`PVR_BP_ROOM_SCALE_VALIDATION=1` or
`Local\PenumbraVR.BlackPlague.RoomScaleValidation` request. Activation fails
closed unless physical-displacement validation is also active. The body adapter
publishes a fresh, body-generation-matched
`predicted_anchor - body_after` horizontal offset; samples expire after 250 ms.
The renderer applies the same offset to the yaw-recentered head view,
HMD-aware visibility and controller game-view basis. It does not scale the raw
HMD delta a second time, does not apply Y and does not add a native body tick.

This new path and its tests are **implemented**. The affected Release targets
compile, but no project binary or test executable was run in this pass, so no
host-test promotion is claimed. The next gate
is `tools/Start-BlackPlagueRoomScaleValidation.ps1`, which holds both mutexes,
persists mirror on and requires camera application, a non-zero X/Z offset,
free/block/slide/stationary outcomes, the native `1.65 -> 0.95 -> 1.65 m`
crouch shape sequence and a fresh applied camera sample after returning to the
standing shape, followed by fresh free/block/slide outcomes, before passing.
Headset observation must additionally cover
recenter, hands, double motion/drift, focus and menus before promotion.

Two presentation regressions from the earlier headset session remain separate.
PID 19192 refined the mirror-off evidence: gameplay frames reported
`monitor_mirror=0`, no monitor world pass and one suppressed world pass, while
native 2D menus remained visible because they do not call `RenderWorld`. This
matches the intended pass boundary. After Alt+Tab/focus loss, main menu/inventory
can still render black; that issue is not patched. The
current tracked-menu capture still depends on the desktop framebuffer/focus and
needs a better evidenced ownership boundary before changing behavior.

## Camera/bob

`D790C -> D5F00` and `D7913 -> D6120` are gravity-disabled synchronization calls, not the general active-player camera composition path. Do not reuse them as a camera boundary.

Camera/head-bob/footstep-bob ownership remains separate comfort work. It is not a reason to reopen body ownership, and it should not be changed speculatively while implementing the first tracking/body reconciliation boundary.

## Hands/fingers

Black Plague provides the better game-neutral articulation semantics: five independent curls, per-joint curves, spread and thumb opposition. Rework's game-neutral controller conditioning has now been extracted into `runtime::vr_hand_pose`: the measured `0.08` skeletal deadzone, grip/trigger fallback closing windows and ~70 ms smoothing are host-tested shared policy.

Overture consumes the complete shared conditioning path while keeping its rig-specific bind poses, bone axes, hand-chain order, handle geometry and forced-grab presentation local. Black Plague applies the shared skeletal deadzone/smoothing once per native input update before its richer articulation output. Its Framework input frame does not yet expose normalized grip/trigger analogs, so the existing non-skeletal digital visual fallback remains unchanged rather than synthesizing missing analog data.

Current boundary:

```text
shared input conditioning -> shared articulation output
        ↓
per-game rig/profile adapter
```

Release validation is host-only: the full autonomous Overture Release regression passes, including 289 `VRTrackingTest` checks, and all 30 root CTest tests pass. Do not mark this extraction live/headset validated until controller hardware is exercised. Do not degrade Black Plague articulation to match Overture merely because Overture is the historical reference.

## Interaction priorities after body reconciliation

The game-neutral Rework palm collision policy (dimensions, contact skin,
sweep/refinement and recovery thresholds/predicates) is now Framework-owned in
`vr_interaction_policy.hpp`; Overture consumes that exact policy through its
legacy `VRHandCollisionPolicy` namespace. Black Plague still needs a native
collision/contact adapter and character/body exclusion validation before this
becomes an implemented gameplay feature there.

Rework's semantic haptic profiles are also Framework-owned in
`vr_haptics.hpp`, including strength clamping/scaling and per-event cooldowns.
Overture consumes them through its existing `cVRHaptics` API. Black Plague's
existing pickup/drop feedback now uses the same proven profiles while its
backend continues to own session availability and actual OpenVR submission.
This is host-testable policy reuse only; it does not complete Black Plague's
comfort/haptics milestone or constitute headset validation.

Stable panel anchoring and transient overlay ownership handoff are now also
Framework-owned in `vr_panel_policy.hpp`. Black Plague's tracked menu consumes
the stable-anchor lifetime plan, while Overture's radio/subtitle path consumes
the overlay handoff. Placement transforms, renderer calls and game menu state
remain backend/product-specific. The new policy is host-tested only.

The common tool/attachment socket composition is now Framework-owned in
`vr_grab_pose.*`: a per-game model-to-hand rotation plus measured model grip
point is composed onto the resolved hand pose using the same local-orientation
then `T(-grip point)` principle demonstrated by Rework. Black Plague consumes
this helper for its measured flashlight and glowstick sockets, and its existing
exact matrix tests remain unchanged. The shared composition is host-tested;
the current BP geometry/profile is not promoted to definitive placement until
final hand geometry and headset/light-direction evidence exist.

The shared spatial-audio reference now contains the complete accepted Overture
mine-gallery EFX parameter set, including echo, modulation and room-rolloff
fields. This strengthens the offline reference; Black Plague/Requiem audio hook
integration still requires game-specific evidence.

1. palm collision adapter and character/body exclusion validation;
2. jointed mechanisms / doors / levers through native mechanism state;
3. definitive per-game tool/glowstick geometry/profile and headset validation;
4. inventory, notes, menus, HUD and subtitles;
5. comfort/haptics and representative chapter-level validation.

Long bars and mechanisms must not be fixed by arbitrary springs or rigid palm offsets. Map native joint/slider/hinge state.

The remaining Rework VR code was re-audited host-only on 2026-09-12 after the
attachment-socket extraction. There is no further justified shared-runtime
extraction at the current evidence level: the VR dimmer is only consumed by
Overture UI flows; staged loading is tied to Overture's map/compositor lifecycle;
physical crouch still needs Black Plague stand-clearance evidence; inventory,
notes, HUD and subtitles need native menu/draw-state boundaries; and jointed
mechanisms need one mapped native mechanism consumer. Do not manufacture a
generic abstraction to make the roadmap checkbox move. Resume this audit when a
second backend boundary is demonstrated or after the relevant live/headset gate.

## Requiem

Requiem remains a future exact-build binary backend. Do not assume Black Plague RVAs, layouts, calling conventions or lifecycle boundaries transfer without evidence.

## VR settings/menu extraction

The Framework now persists the complete shared `VrSettings` schema through
`vr_settings_store.*`, including settings not yet consumed by Black Plague.
`vr_settings_editor.*` owns the demonstrated Overture Rework 18-row editor
semantics: row order/labels, formatting, edit increments, enum wrapping, clamps,
snap/smooth dependent visibility and crouch-depth label behavior.

Black Plague now has an explicit backend capability map. It marks only the
currently applied editor settings as available: handedness, turn mode and its
snap/smooth/dead-zone controls, move speed/dead-zone, UI distance/scale and
render scale. Monitor mirror remains a separate setting.
Play mode, player height, height offset, crouch settings, Enhanced visuals,
HRTF and subtitle scale are persisted but are not Black Plague functional
controls yet.

`PenumbraVR.ProbeLauncher.exe --configure-vr black-plague` now provides an
offline configuration surface for those backend-consumed controls plus monitor
mirror. It uses the shared Rework-derived editor semantics, applies dependent
snap/smooth row visibility, saves through the existing settings store, and its
reset action restores only Black Plague-supported controls plus mirror so
persisted-but-unwired values are preserved. This path does not require the game
or probe DLL to be running.

Do not invent a binary menu hook merely to expose this policy. A dedicated
Black Plague VR Settings page still requires a demonstrated safe native-menu
insertion boundary. Requiem has no playable backend and no menu integration.

## Large Address Aware exact variants

The pure PE32 one-bit LAA transform is unit-tested, and the build catalogue now
recognizes exact transformed variants of the canonical Black Plague and Requiem
builds without creating new semantic build IDs. Offline verification against
temporary copies of the installed canonical executables changed only file offset
`0x12E`, `Characteristics 0x010F -> 0x012F`, and reproduced these hashes:

- Black Plague: `DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196`
- Requiem: `577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955`

The installed originals were not modified. Transactional filesystem
backup/apply/verify/rollback remains future installer work.

The supplied Spanish translations for Black Plague and Requiem are now retained
as exact Framework deployment inputs under `assets/localization`, together with
their original `leeme.txt` attribution notices. `assets/localization/manifest.json`
pins their hashes and their real install-relative destinations
(`redist/config/Espanol.lang` and
`redist/expansion01/config/Espanol_exp.lang`). Metadata validation checks these
inputs, but the production installer still needs to deploy and roll them back
through the transactional filesystem layer. Overture continues to package its
own `products/overture/data/config/Espanol.lang` overlay.

## Validation discipline

Keep states distinct:

`planned` → `implemented` → `host-tested` → `live-tested` → `headset-validated` → `supported`

Compilation and CTest do not imply live or headset validation.

Current root validation count is 30 CTest tests in the full configured suite,
including the shared VR settings editor/store and Black Plague capability-map
regressions. The hosted SDK-less CI executes the established suite with the
real-driver `opengl_eye_targets` test excluded.
Overture retains its 289 historical `VRTrackingTest` checks plus
shader/visual/texture/LAA gates, now also exercised by the dedicated
`Overture Release regression` CI job. The latest complete local offline result
on 2026-09-12 passed the full Release build and all **30/30** root CTest tests,
including the real-driver `opengl_eye_targets` test. Metadata validation also
passed with 6 catalogue entries, 2 exact-build manifests, 42 actions, 6 action
sets and 8 controller bindings. `Build-OvertureProduct.ps1 -Configuration
Release -Full` then passed its project, 16-shader, 8,752 visual-reference,
231-texture decode, Large Address Aware and `VRTrackingTest` gates; the latter
reported **289 checks, 0 failures**. These were host-side checks only: no game,
SteamVR or headset process was launched.

For meaningful changes update the smallest relevant set among:

- `README.md`
- `ROADMAP.md`
- `CHANGELOG.md`
- `CODEX_HANDOFF.md`
- `DEBUG_HANDOFF.md`
- `docs/REWORK_PORTING_PLAN.md`
- `docs/OVERTURE_BACKEND_MIGRATION.md`
- `docs/VR_STARTUP_AND_CONTROLLERS.md`
- `docs/VR_HEADSET_TEST_CHECKLIST.md`
- `docs/BLACK_PLAGUE_SPATIAL_NOTES.md`
- `docs/BLACK_PLAGUE_PROBE.md`
- `docs/SUPPORTED_BUILDS.md`
- `THIRD_PARTY.md`

Do not mark the Black Plague backend or any executable as `supported` without the required evidence gate.
