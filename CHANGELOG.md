# Changelog

- 2026-09-18: Hardened the Black Plague launcher readiness gate after PID 24136
  reproduced the historical early SDL crash signature (`0xc0000005` at
  `SDL.dll+0x28c09`) after exact-build/profile logging but before the first
  OpenGL-hook result. The previous gate could accept any non-empty top-level
  window owned by the game process. It now requires the visible, stable SDL 1.2
  `SDL_app` window while deliberately retaining the earlier removal of the
  unreliable cross-thread `GetPixelFormat(GetDC(hwnd))` check. Startup failure
  reporting also distinguishes a game that exited on its own from one that is
  still running. Release builds, 38/38 root CTest, metadata, exact-image
  verification and `--check-vr` preflight pass without starting the game or
  SteamVR. This remains **host-tested only** until a fresh real launch crosses
  `PenumbraVR_Initialize`; the crash is not assigned to a VR subsystem without
  dump/stack evidence.

- 2026-09-18: Completed the first Black Plague runtime consumer for Rework's
  mine-gallery environmental audio. The exact-build adapter hooks the native
  OpenAL/EFX effect-attach and environment-gain boundaries, applies the shared
  20-field reverb preset plus bus trim, and can bootstrap a game that was already
  running while preserving an authored non-default environment. The two
  callsites, low-level sound vtable slots and 20 EFX setters are exact-image
  verified; Release build, lifecycle coverage and focused audio tests pass. This
  remains host-tested until headset/audio-device validation. Rework's separate
  distance/occlusion low-pass policy is still open for Black Plague.

- 2026-09-18: Clarified the long-term convergence rule for the trilogy. Rework
  remains the proven Overture baseline, while Penumbra VR Framework is the active
  evolution line: better game-neutral behavior demonstrated by another backend
  is promoted to shared runtime only after real cross-consumer evidence, and the
  Framework-owned Overture product then consumes that shared improvement too.
  Game-specific mechanics, exact-build details and profile data remain in their
  owning backend/profile rather than being generalized for symmetry.

- 2026-09-18: Audited the documentation boundary. The README remains the public
  landing page, durable architecture/state documents now point only to versioned
  sources of truth, and transient Codex handoffs, debugging/research chronology
  and the historical Astra audit were moved out of Git into the ignored local
  `work/` documentation area. `AGENTS.md` was reduced to stable engineering
  rules. The audit also corrected stale Black Plague status text for the
  `1.5/2.25 m/s` locomotion policy and PID 25484 rejected-direction comfort
  evidence. No runtime behavior changed.

- 2026-09-18: Added host-testable Black Plague presentation timing diagnostics
  around the existing single-consumption compositor contract. Telemetry now
  reports presentation-pose acquisition interval/jitter plus pose age at world
  render and at successful direct or deferred compositor submit, with explicit
  validity flags. The tracker is backend-local and observational; it does not
  alter pacing, prediction, pose ownership or the existing 250 ms stale-snapshot
  cutoff. Release builds and the focused presentation/stereo policy tests pass;
  headset validation is intentionally unchanged.

- 2026-09-17: Consumed the transferable Rework `23c890f` Enhanced Visuals eye
  stage in Black Plague without importing Overture/HPL material assumptions.
  The shared OpenGL stage now renders each enabled eye through an RGBA16F
  intermediate with 2x MSAA, resolves it and applies Rework's bounded 5-tap
  sharpen plus the exact v4 tone/saturation/contrast/gamma treatment before the
  existing compositor texture is submitted. Unsupported GL capabilities fall
  back to the existing direct eye target instead of invalidating stereo. The
  Black Plague capability map now exposes `EnhancedVisuals`; the real-driver WGL
  regression checks the output against the CPU calibration reference. Release
  build, metadata, exact-image verification and all 36/36 root CTest tests pass.
  This is **host-tested only**. Rework's VR-specific ambient/light material
  response remains HPL/product-owned work and full Enhanced Visuals parity is
  therefore still open.

- 2026-09-17: Consumed the Rework `23c890f` HRTF startup policy for Black
  Plague without adding a late audio hook. Rework writes `alsoft.ini` beside the
  executable before OpenAL opens its device; Framework now generates the same
  `[general] hrtf = auto/true/false` content from the shared spatial-audio policy
  before a fresh Steam launch. If Black Plague is already running, the launcher
  verifies that the saved HRTF mode matches the startup file and fails closed on
  a mismatch because probe injection occurs after native audio initialization.
  The Black Plague settings capability map now exposes HRTF. A focused temp-file
  harness, the settings capability test and the shared spatial-audio test pass,
  so this path is **host-tested only**. Static analysis of the initialized Black
  Plague image also confirmed its native OpenAL/EFX stack and environmental
  effect setup, but that path uses a `1.0` environment bus and does not contain
  Rework's added mine-gallery preset/`0.32` trim or its `0.45` distance-HF audio
  path. Occlusion and environmental reverb therefore remain separate open work.

- 2026-09-17: Extended Black Plague haptic parity with two exact native success
  boundaries, without adding a competing input owner. `LightToggle` now samples
  the mapped flashlight/glowstick active state before and after the existing
  `cButtonHandler::Update` owner and submits the shared Rework event to the off
  hand only after a real native toggle. `Damage` now owns all ten direct calls to
  `cPlayer::Damage (0x9BB80)` through one rel32 wrapper, calls the original method
  first and submits both-hand feedback only when `cPlayer+0x310` health actually
  decreases; BP's observed easy/normal/hard scaling is applied before the shared
  strength profile. The exact-image verifier pins both light-state fields, the
  damage entry/callsites and health boundary, while the native-input contract
  harness covers light transition/off-hand semantics and difficulty strength
  mapping. `MeleeImpact` is now connected at Black Plague's demonstrated
  post-contact melee boundaries: enemy `cGameEntity::Damage` callsite `0x603D5`
  and both generic `HitBody (0x5F000)` callsites `0x60747/0x608CF`. The wrappers
  call the native owners first, route the shared Rework event to the configured
  dominant hand and mirror `HitBody`'s enemy-type exclusion to avoid duplicate
  feedback. The exact-image verifier pins all three calls plus the native
  exclusion, and the contract harness covers dominant-hand routing and ordinary
  versus enemy-backed body eligibility. All three newly mapped event families
  remain **host-tested only** pending controller/headset evidence.

- 2026-09-17: Mapped and host-tested the Black Plague gameplay 2D presentation
  owner. Exact-image evidence pins `cScene::Render`'s native
  `OnPostSceneDraw (0xE1C40) -> GetDrawer (0xDF730) -> DrawAll (0xF3920)`
  sequence and the single `DrawAll` callsite at `0xEE042`. Rework `23c890f`
  added VR-only `DrawAll(dontClear, drawVr)` behavior; the original BP binary
  has a no-argument drawer that resets ortho and clears its queue. Framework
  therefore defers gameplay compositor submission to the native owner, captures
  the existing 800x600 RGBA draw once into a transparent FBO, alpha-composites
  that surface into both eye targets with Rework-derived placement and submits
  the pair. The real-driver WGL regression, Release build, exact-image verifier
  and all 35/35 CTest tests pass. Status remains **host-tested**; `SubtitleScale`
  stays unwired until headset evidence confirms native subtitles reach this path.

- 2026-09-17: Closed the host-side Black Plague inventory/notebook presentation
  mapping without adding hooks. Exact-image verification now pins the existing
  VR query sites `0x5096/0x50BE`, `cPlayer::StartInventory`,
  `cNotebook::SetActive(true)`, `cInventory::SetActive(true)` and the native
  active bytes consumed by `UiContext` (`notebook+0x44`, `inventory+0x5C`). The
  verifier also requires `cButtonHandler::Update` to republish `UiContext` after
  the native update, so an overlay opened on that tick reaches the existing
  tracked-menu framebuffer path immediately. The synthetic native-input harness
  now exercises both active bytes. This establishes **host-tested** consumption
  for the inventory/notebook presentation shell and input route; headset
  usability remains open, while HUD/subtitle draw ownership and Black Plague
  subtitle scaling are still unmapped/unwired.

- 2026-09-17: Closed the host-side Black Plague VR footstep ownership gap and
  corrected stale turn-ownership documentation. Rework `23c890f`'s `>0.85 m`
  horizontal body-anchor cadence is now shared as `VrFootstepCadence`; BP feeds
  only collision-accepted physical/direct-VR displacement. The body observer
  queues the cadence result and the existing `cButtonHandler::Update` owner
  dispatches native `cPlayer::FootStep(0.8)` after the original update returns,
  keeping sound/material work outside the body-update stack. Pending dispatch is
  identity-bound and now has cadence/attempt/success/reject/ABI telemetry. The
  cadence anchor also uses double precision internally so repeated small float
  deltas cannot cross the strict threshold from summation error. Exact-image verification pins
  `FootStep=0x9C7B0` and a native MSVC 2003 empty-string ctor/call/dtor sequence,
  so no modern-CRT `std::string` crosses the game ABI. The current BP turn path
  was rechecked against Rework and already uses tracking world yaw via
  `AddTrackedWorldYaw(-turn)`. Release build, CTest `35/35`, and the exact-image
  verifier pass; both changes remain below headset validation where applicable.
- 2026-09-17: Closed the host-only Black Plague representative mechanism
  boundary. Exact-image verification now pins `cGameLever` plus the concrete
  Newton hinge/slider type, pin/pivot data, body velocity setters and native
  joint-controller pause/resume lifecycle. Black Plague keeps native
  `Move::Enter/Leave` ownership for scripts, gravity and state transitions, but
  recognized one-joint Lever `Move::Update` now consumes shared
  `vr_mechanism_policy`; unknown and multi-joint mechanisms fall back to native
  behavior. Synthetic interaction coverage exercises the hinge servo and
  cap restoration, the palm helper parses mechanism diagnostics, Release build,
  exact-build verification and Framework CTest `35/35` pass. Status remains
  **host-tested only** pending a representative articulated headset test.

- 2026-09-18: Extended the Black Plague mechanism consumer to the first
  independently proven door family without broadening the generic runtime.
  Exact Rework `23c890f` shows `cGameSwingDoor` entering `Move`, pausing native
  controllers/gravity and treating authored joints as hinges; the supported BP
  image independently pins its `0x2EC` construction, vtable `0x6791F0`, entity
  type `8`, lifecycle flags and base joint collection. The backend now accepts
  only one-joint SwingDoor hinges through the existing shared servo and rejects
  slider identity fail-closed. Wheel and compound mechanisms remain native.
  Synthetic coverage exercises the new hinge consumer and rejection path;
  Release build, focused tests, exact-image verification and Framework CTest
  `36/36` pass. Status remains **host-tested only** pending headset evidence.

- 2026-09-17: Consumed Rework `23c890f` magnetic inventory pickup in Black
  Plague without adding another hook. The existing normal-player pick owner now
  falls back from bounded physical palm selection to exact-build world-body
  enumeration, real `cGameItem` subtype classification, shared top-five
  cone/scoring policy and solid visibility rays from both controller aim and
  HMD. BP-only gasmask/collectable subtypes remain excluded, and magnetic
  winners stay separate from nearby physical `interaction_assist`. The
  supported-image verifier now pins the consumed body-list, body/entity filter
  and embedded bounding-volume ABI; Release builds, the focused classifier test
  and Framework CTest `35/35` pass. The integration remains **host-tested only**
  until representative inventory pickups are exercised in the headset.

- 2026-09-17: Finished the interrupted Black Plague palm/grab follow-up against
  Rework `23c890f`. VR picking now lets the native ray callback traverse every
  hit before ranking the five bounded palm rays, retains the originating hand
  across the native Enter -> committed-state boundary, and suppresses direct
  hand nudge while that hand is selecting, pending or holding an interaction.
  The palm resolver now receives Rework's nearby physical interaction target
  and decides the `0.008 m` contact skin from the resolver's actual chosen start
  pose, including recovery/reanchor; new telemetry records interaction-assist,
  tracking-reanchor and recovery events without changing the proven recovery
  thresholds. Physical room-scale stepping now mirrors Rework's
  `vr_stepstaticonly`: the existing Black Plague character ray callback is
  observed to classify the winning step body by mass, static geometry retains
  native step climbing, and only dynamic winners are discarded on a
  physical-only tick. Glowstick presentation now uses the exact transferable
  Rework profile (`VrScale=1.55`, `VrGripPoint=(0,0.0078,-0.078)`, X rotation
  `4.71`) after confirming the BP primary cylinder geometry matches Rework; tool
  hands also use Rework's authored long-finger grip pose. Release build, the six
  focused interaction/render/body tests and the supported-image exact-build
  verifier pass. All of these newest gameplay/presentation changes remain
  **host-tested only** pending the focused headset gate.

- 2026-09-17: Incorporated headset evidence from Black Plague PID 14212. The
  gameplay palm resolver was never active in that run (`palm_collision
  enabled=0 source=disabled`) because the focused helper released its validation
  mutex before gameplay reached the one-shot resolver sample; the helper now
  keeps the request alive until palm + room-scale activation or game exit. Nudge
  telemetry was active with zero acquired grabs, so the nudge path now ignores
  the active interact hand for the full held-grip interval. The zero-between-tick
  X/Z presentation candidate also produced a forceful wall-lean pullback, so the
  previously headset-exercised rejected-direction render filter is restored.
  Glowstick/tool placement now composes Black Plague's measured model socket with
  Rework's proven long-finger grip frame. Finger telemetry confirmed all five
  logical curl channels, leaving the poor thumb/little-finger visuals and mesh
  deformation as a renderer/rig issue. Release build, 35/35 CTest, generated-hand
  determinism, PowerShell syntax, `git diff --check` and the supported-image
  verifier pass; the corrected palm/grab/wall-lean/tool behavior still needs the
  next headset run.

- 2026-09-17: Reworked project documentation around an explicit trilogy parity
  gate. Added `docs/TRILOGY_PARITY_PLAN.md` as the authoritative capability
  ledger separating Rework reference behavior, shared extraction, target
  consumption and validation evidence. Black Plague is now the required
  second-backend proof before Requiem gameplay becomes the active milestone;
  Requiem remains free to perform non-invasive exact-build reconnaissance and
  must repeat target-specific binary research rather than inherit BP RVAs or
  layouts. `AGENTS.md`, `README.md`, `ROADMAP.md`, the Rework porting plan and
  both handoff documents now follow that sequencing. The stale Black Plague VR
  settings handoff was also corrected: `PlayMode`, `PlayerHeight`,
  `HeightOffset`, `CrouchMode` and `PhysicalCrouchDepth` are now exposed because
  each is already consumed by the tracked presentation/crouch path; Enhanced
  Visuals, HRTF and subtitle scale remain unwired. Black Plague haptic reuse was
  also extended without new hooks: tracked UI selection now emits the shared
  `UISelect` profile on the actual pointer hand, and successful direct hand
  nudges emit the shared `Interaction` profile with bounded contact strength.
  The backend enforces focus, connected/valid grip pose and the shared per-event
  cooldown before OpenVR submission. These additions are host-tested only;
  light-toggle, melee-impact and damage feedback still require safe Black Plague
  event boundaries and headset validation remains open.
  The focused Black Plague palm validator is now a combined evidence run rather
  than a palm-only checklist: it aggregates `Grab=6`/`Move=2`, tool attachment,
  direct hand-nudge, tracked-crouch and per-event OpenVR haptic submission
  telemetry while retaining the known-good room-scale composition. Core
  palm/room-scale/Grab/crouch evidence remains mandatory; unavailable
  Move/tool/low-ceiling/haptic coverage is reported explicitly instead of being
  silently mistaken for a tested feature. The resulting working state passes
  Release **35/35** CTest, SDK-less Release **35/35** CTest, metadata validation
  and the initialized Black Plague exact-image verifier; no new headset status
  is implied by those host-side gates.

- 2026-09-17: Reproduced and hardened the remaining Black Plague Rework-hand
  color corruption. The existing hostile-state WGL test now enables a
  VBO-backed `GL_SECONDARY_COLOR_ARRAY` plus `GL_COLOR_SUM`; before the fix the
  hand pixel changed to `220,255,154` despite the primary color-array guard.
  VR overlay setup now disables color summation and the hand renderer disables
  inherited secondary-color client data while restoring the host state after
  drawing. The same host-tested batch ports Rework's direct physical hand nudge
  through the existing Black Plague body-update owner (`0.12 m` sphere,
  `0.03 m/s` threshold, bounded contact-velocity-aware mass impulse and nudge
  telemetry) and removes Black Plague's between-tick raw X/Z render prediction
  so horizontal presentation follows the reconciled anchor like Rework. Release
  build, 35/35 CTest, generated-hand determinism, `git diff --check` and the
  supported-image verifier pass. These changes still require one combined
  headset validation before promotion.

- 2026-09-16: Hardened Black Plague VR startup after PID 28872 crashed during
  probe initialization before any hook-install result was logged. Windows Error
  Reporting recorded `0xc0000005` faults in the game's SDL 1.2 DLL and a BEX in
  `nvoglv32.dll`. The launcher previously treated the exact initialized
  RenderWorld call bytes as sufficient runtime readiness even though those bytes
  can become available while SDL/OpenGL is still creating the render window.
  The first hardening attempt then prevented the crash but timed out because it
  required `GetPixelFormat(GetDC(hwnd)) != 0`; SDL 1.2 can own a private DC, so
  that cross-thread GDI query is not a reliable readiness signal. Startup now
  waits for a non-empty game-owned top-level client window to keep the same
  handle and dimensions for 20 consecutive 25 ms polls before injection. It no
  longer depends on visibility or pixel-format readback. Release launcher build,
  35/35 CTest and the exact-build verifier pass. This remains **host-tested**
  until the next `Start-BlackPlaguePalmCollisionValidation.ps1` run reaches VR.

- 2026-09-16: Added a host-tested presentation optimization/baseline before the
  next Rework graphics port. The fixed-function presentation state now caches
  context-owned OpenGL entry points, texture-unit count and rectangle-texture
  capability instead of resolving/querying them again for each eye/hand draw.
  The hostile-state WGL regression still proves VBO/color-array restoration and
  per-finger hand articulation. Black Plague render telemetry now also records
  wall-clock CPU time spent in the complete stereo pipeline, native eye-world
  rendering, tracked-hand drawing and compositor submission so future lighting,
  shadows and effects can be measured against a concrete baseline. Release
  builds, 35/35 CTest and the supported-image exact-build verifier pass. This is
  **host-tested** only; headset frame pacing and the real runtime gain remain to
  be measured in the next clean combined palm/room-scale run.

- 2026-09-16: Extracted two additional game-neutral interaction policies from
  the proven Rework `23c890f` implementation. `vr_magnetic_pickup_policy.hpp`
  now owns the item-only magnetic targeting ranges/biases, cone/scoring math,
  ranked-candidate limit, visibility sample and ray overshoot while Overture
  retains item classification, portal traversal and physics visibility queries.
  `vr_mechanism_policy.hpp` now owns the demonstrated unconstrained/slider/hinge
  servo math and velocity caps while Overture retains native joint selection,
  HPL application and its entity/mass-specific hinge-lightness profile. The
  Overture source host consumes both policies directly and `check-project.ps1`
  prevents those formulas from silently returning to product-local copies. The
  new host regression plus the full Overture Release rebuild/unit gate and the
  Framework suite pass (`289` Overture checks, `35/35` CTest). Black Plague does
  not consume magnetic pickup or jointed mechanism motion yet; those adapters
  still require its native item classifier/visibility and mechanism-state
  boundaries. This extraction is **host-tested** only.

- 2026-09-16: Investigated the PID 26940 Black Plague headset regression after
  importing the Rework hand rigs. The captured frame confirmed real hand-render
  corruption: mostly black surfaces with changing multicolored triangles. The
  cause was leaked HPL client-array/VBO state, especially an enabled
  `GL_COLOR_ARRAY`, which overrode the hand's constant white modulation and
  indexed unrelated host VBO colors. The shared Rework-hand renderer now
  isolates/restores client vertex-array state, disables inherited color/normal/
  index/edge arrays for its draw and makes the one-time texture upload
  deterministic across host pixel-store state. The real-driver WGL regression
  now enters from a hostile bound-VBO/color-array state, verifies restoration
  and verifies that an isolated index-finger curl changes the rendered image.
  PID 26940 also correlated the reported repeated wall-contact mini-jumps with
  Black Plague's native step-climb phase: X/Z-only physical requests produced
  repeated ~5 cm vertical body changes around blocked/partial solves. The exact
  backend now owns a second pinned boundary at `0xD7361` and skips only that
  native step-climb phase on physical-HMD-only ticks with no direct/native
  horizontal locomotion; the existing gravity/jump phase and sole `D6E00`
  update remain native. Telemetry exposes `physical_step_suppressed` plus both
  five-finger controller curl sets. Release build, 34/34 CTest, the exact-image
  verifier and generated-hand determinism check pass. Both fixes remain
  **host-tested** until one clean headset run verifies stable hand texture,
  visible five-finger articulation, wall-pressure vertical stability and normal
  stick stair/ledge behavior.

- 2026-09-16: Hardened and reduced the host-tested Rework hand renderer before
  its first headset gate. The generated mesh now deduplicates each authored
  position/UV pair (3,376 draw vertices instead of 18,102 expanded corners per
  hand) and uses the unchanged 18,102-index / 6,034-triangle topology. The
  client-array draw also explicitly clears and restores HPL1's current
  `GL_ARRAY_BUFFER` / `GL_ELEMENT_ARRAY_BUFFER` bindings so CPU pointers cannot
  be misread as VBO offsets; the real-driver WGL test now enters the hand draw
  with both sentinel VBO bindings active and verifies exact restoration.

- 2026-09-16: Replaced Black Plague's provisional procedural tracked-hand boxes
  with a renderer-owned import of the proven Rework `23c890f` right/left hand
  DAE rigs. A deterministic generator validates the 3,053-position, 6,034-
  triangle, 17-joint rigid skin and emits checked-in renderer data plus a
  reduced copy of the existing diffuse material, so runtime builds do not need
  a COLLADA/JPEG dependency. CPU skinning maps the existing richer
  `runtime::ArticulateVrHand` output onto Rework's proven bind hierarchy/axes,
  applies the original HUD transform, uses vertex arrays and reuses unchanged
  posed geometry across the two eye draws. The real-driver OpenGL test now
  exercises the mesh while preserving depth and caller GL state. This is
  host-tested only; the next clean palm/room-scale headset gate also checks hand
  scale, orientation, finger motion and frame pacing.

- 2026-09-16: Corrected the Black Plague palm validation composition after PID
  4720 showed `physical_displacement_validation=0`, `room_scale_validation=0`
  and `positional_translation_enabled=0` while palm collision was enabled. The
  focused palm helper now enables the existing physical-displacement and
  room-scale mutexes, keeps the mirror on and requires live room-scale plus
  tracked-crouch telemetry before the run can pass. Exact Rework `23c890f`
  comparison also found that target acquisition follows the raw controller by at
  most `0.18 m` beyond a collision-stopped visible palm. Black Plague now ports
  that bounded acquisition rule while visible hands, held-object motion and
  tools continue to consume the resolved palm. Release build, 34/34 CTest and
  the supported-image verifier pass; headset promotion remains pending.

- 2026-09-16: Synchronized the operational documentation with the current
  Black Plague evidence after the gameplay-palm integration. PID 25484 is now
  consistently recorded as positive headset evidence for tracked Y, current
  crouch/direct locomotion and the rejected-direction X/Z comfort filter;
  Hybrid release-hold, blocked-stand/low-ceiling and deliberate wall/slide stay
  open. The palm path is consistently marked host-tested after PID 28412/PID
  8644 closed the native query/lifecycle-resolver gates; PID 23000 remains
  inconclusive because severe FPS loss and a right-controller dropout occurred
  in the same headset run. No gameplay behavior changed in this documentation
  sync.

- 2026-09-16: Connected the Black Plague palm resolver to gameplay hands on the
  host-tested path and published each hand's currently owned body into the native
  `skip_body` query. Visible hands, physical interaction and tools consume the
  resolved grip while aim remains raw. Investigation of the headset report that
  many props floated away from the hand found a second native manipulation route:
  `Grab=6` already used the rigid palm-relative free-body adapter, but many props
  enter `Move=2` and retained Black Plague's distance manipulation. The exact
  image now pins that state separately. Free `Move=2` bodies preserve the picked
  surface point and follow the resolved palm with the Rework-derived force path;
  jointed/mechanism bodies remain native. Release 34/34 CTest, the SDK-less
  CI-equivalent 33/33 suite, metadata, the exact-image verifier and Overture
  `-Full` (Release/LAA + 289/289 tracking checks) pass. PID 23000 is not a
  headset-validation result:
  the same run had severe FPS loss and a right-controller dropout, so a clean
  rebooted A/B run remains required for performance, palm contact and placement.
  Fixed the focused palm-validation launcher so a successful PowerShell exact-build
  verifier is not misclassified by a stale native `$LASTEXITCODE` value from the
  caller; verifier failures still terminate the launcher through the script's
  terminating error path.

- 2026-09-16: Pinned the remaining Black Plague palm-query exclusion semantics
  in the initialized exact-build image. `CheckShapeWorldCollision` at `0xD4830`
  independently rejects character bodies when its `collideCharacter` argument
  is false and rejects the exact fourth-argument `skip_body`. The exact-image
  verifier now locks those branches, and the host palm-resolver test verifies
  the backend supplies Rework's `skipStatic=false`, `isCharacter=false`,
  `collideCharacter=false`, `debug=false` contract with the requested skip
  body. This closed the static/host exclusion boundary; the later gameplay-path
  entry above adds per-hand held-body publication while headset contact remains
  pending clean validation.

- 2026-09-16: PID 8644 live-tested the corrected default-off Black Plague
  owned-palm lifecycle/resolver gate. The run created one standalone box shape,
  reused it on the second sample, issued six native world queries, destroyed it
  once, preserved `user_count=0` throughout its standalone lifetime and left
  selected gameplay memory unchanged. No contacts were present at the sampled
  pose, so this closes the lifecycle/resolver real-process gate without claiming
  headset/contact-feel validation or gameplay hand integration.

- 2026-09-16: PID 30032 reached the new owned-palm live gate and failed at its
  creation-validation boundary before resolver execution. Review against the
  proven HPL1/Rework implementation exposed an invalid Framework assumption:
  `iCollideShape` starts with zero users and `CreateBoxShape` does not increment
  that count; a body increments it only when adopting the shape. The host fixture
  had incorrectly initialized owned shapes with one user, masking the mismatch.
  The owned-palm boundary now requires the exact box type, world and vtable with
  `user_count=0`, and the host fixture mirrors that native lifecycle. PID 8644
  subsequently passed the corrected live gate described above.

- 2026-09-16: PID 28412 live-tested the default-off Black Plague no-write palm
  query on the supported build: one callback, eight contacts, unchanged native
  memory and continued normal game ticking. The next isolated stage is now
  implemented and host-tested. Black Plague owns `CreateBoxShape`/`DestroyShape`
  through the pinned exact-build ABI, safely reuses or replaces the palm shape
  with physics-world ownership, and feeds native contacts into a game-neutral
  port of Rework `23c890f` sweep/refinement, slide, overlap recovery, reanchor,
  constrained recovery and rotation resolution. `--validate-palm-resolver`
  creates, queries, reuses and destroys the owned shape on the existing
  post-`D6E00` game-thread owner without publishing the resolved pose to
  gameplay. Synthetic tests cover clear sweep, wall slide, overlap recovery,
  rotation blocking, malformed contacts, same-world reuse, world replacement
  and native gameplay-memory guards. Release, Debug and SDK-less Release each
  pass 34/34 CTest; metadata, the supported-image exact-build verifier and the
  autonomous Overture `-Full` regression also pass. Owned palms/resolver remain
  host-tested until the new live gate succeeds.

- 2026-09-16: PID 25484 supplied focused Black Plague headset evidence for the
  current crouch/Y and short-range room-scale candidate. The run produced 12,858
  presentation frames with zero stereo failures, 2,477 meaningful body samples
  (`2407 free / 43 blocked / 27 partial`), three physical crouch entries/exits,
  final native move-state `0` with the `1.65 m` body, `0.961 m` tracked-Y range
  and 650 accepted direct-locomotion samples. The user reported that the session
  felt good. The run also captured stable button-only crouch and a combined
  Hybrid state, but not the later `physical=0 + latch=1 + state 4` release-hold
  sample; that subgate plus blocked-stand/low-ceiling and deliberate wall/slide
  edges remain open. Documentation was consolidated around the current roadmap,
  architecture, handoffs and validation checklist; superseded post-audit plans
  and tracking/body implementation snapshots were removed.

- 2026-09-14: PID 22096 supplied the first positive headset run for the
  presentation-sequence fix in `7f84235`: 15,990 logged frames, 15,887 gameplay
  frames and zero stereo/compositor failures after menu-to-gameplay transition.
  Re-analysis of the same run isolated the remaining short-X/Z comfort defect:
  2,060 meaningful physical samples included 215 blocked and 90 partial cases,
  while the render stream contained repeated prediction resets around rejected
  collision motion. Rework keeps raw horizontal tracking out of presentation
  until body reconciliation; Black Plague needs render-rate continuation between
  its ~60 Hz body ticks, so the Framework now carries the last physical
  reconciliation into render and removes only the prediction component that
  continues into the last rejected direction. Tangential slide and motion away
  from the obstacle remain untouched. Release, Debug and SDK-less Release pass
  34/34 CTest; metadata and the exact-build BP verifier pass; Overture `-Full`
  passes Release/LAA and 289/289 `VRTrackingTest`. This collision-comfort change
  is still **host-tested only** and needs a fresh headset run before the PID
  20520 pullback report can be closed.

- 2026-09-14: invalidated the follow-up Black Plague headset candidate
  `ca099ca` after PID 6016 reproduced OpenVR compositor error 108 on the
  menu-to-gameplay transition. The prior nested-visibility fix was necessary
  but incomplete: a valid presentation snapshot remained reusable after a
  successful world `Submit`, so a later `RenderWorld` without a newly published
  visibility sample could submit the same compositor sequence again. Presentation
  sequences are now single-consumption: sequence 0, an already-submitted
  sequence, or an older sequence is rejected before rendering/submission.
  Telemetry records stale-sequence rejects and now logs every acquisition,
  nested reuse and stale reject. Debug, Release and SDK-less Release each pass
  34/34 root tests, including the new presentation-sequence policy cases, and
  metadata validation passes. The exact-build BP verifier input and mapping are
  unchanged by this presentation-only patch; its script invocation was blocked
  by the local tool safety layer in this iteration, so no new verifier run is
  claimed. Headset validation is still required before this correction is
  promoted.

- 2026-09-14: invalidated headset candidate `3333be1` after Black Plague PIDs
  23656 and 21396 both failed their first gameplay stereo transition with
  OpenVR compositor error 108 (`VRCompositorError_AlreadySubmitted`). The new
  visibility-owned presentation sampling could reacquire compositor poses from
  `UpdateRenderList` callbacks reached inside the two eye renders, overwriting
  the pre-RenderWorld snapshot and breaking one-wait/one-submit ownership. The
  backend now validates the gameplay camera before sampling and reuses the
  owning presentation snapshot during nested eye callbacks. Debug, Release and
  SDK-less Release each pass 34/34 root tests; metadata, the supported-image BP
  verifier and Overture `-Full` also pass. A fresh headset run is still required
  before the presentation intervention can be promoted.
- 2026-09-14: completed the offline intervention checkpoint for tracking/body,
  crouch ownership, posture snapshots, interaction lifecycle and probe
  teardown. Added the real NativeInputBridge crouch contract harness, a
  partial-lifecycle install/rollback ledger test, shared hand-contact and play
  mode policies, same-tick body transaction coverage for 72/90/120 Hz render
  against a 60 Hz body tick, repeated samples, recenter, slide and jitter, and
  moved rel32 hook diagnostics outside the suspended-thread region. Debug,
  Release and SDK-less Release each pass 34/34 CTest tests; metadata, the
  supported-image BP verifier and diff checks pass. The Overture -Full gate
  was rerun after fixing one stale extracted helper call and passed the Release
  product build, Large Address Aware check and 289-check `VRTrackingTest`.
- Prepared the next Black Plague headset-validation candidate after the
  constrained Push/Move locomotion mapping. Release, Debug and SDK-less Release
  pass 34/34 CTest, metadata and the supported-image verifier pass, and the
  Overture `-Full` regression passes again. The candidate is launched through
  `tools/Start-BlackPlagueRoomScaleValidation.ps1`; this does not promote the
  new crouch/Y, short-X/Z comfort, constrained locomotion or lifecycle work
  beyond host-tested evidence.
- Mapped and pinned the supported Black Plague image boundary needed for a
  future palm adapter: `CheckShapeWorldCollision` at RVA `0xD4830` with its
  nine-argument x86 ABI, legacy callback/contact layout, physics-body shape and
  matrix accessors, `CreateBoxShape` vtable slot, shape user count and
  destruction route. Added a default-off `--validate-palm-query` diagnostic
  that reuses the current player-body shape on the existing post-`D6E00` game
  thread owner, writes only to DLL/stack output and rejects changes to selected
  native world/body/shape bytes. Its synthetic clear/contact/mutation harness
  is host-tested, bringing root CTest to 34 tests. PID 28412 subsequently
  live-tested that no-write query without selected native-memory changes. The
  separate owned-palm lifecycle/resolver gate described above remains
  disconnected from gameplay pending its own live validation.

- Extracted the demonstrated Overture/Rework palm collision policy into
  `src/runtime/vr_interaction_policy.hpp`: palm dimensions, contact tolerances,
  sweep/refinement limits and recovery predicates now have one Framework-owned
  source of truth. The autonomous Overture product keeps its legacy
  `VRHandCollisionPolicy` API through a compatibility namespace, leaving native
  physics/contact queries and body exclusions for each backend adapter.
- Extracted Rework's semantic haptic event profiles, strength scaling and
  per-event cooldown policy into `src/runtime/vr_haptics.hpp`. Overture now
  consumes that shared policy through its existing `cVRHaptics` boundary, and
  Black Plague's existing pickup/drop feedback uses the same proven profiles
  while device submission remains backend-owned.
- Extracted the demonstrated stable-panel anchor lifetime and transient overlay
  ownership handoff into `src/runtime/vr_panel_policy.hpp`. Black Plague's
  tracked menu anchor and Overture's radio/subtitle overlay now consume the
  shared policy while retaining their renderer/game-specific placement code.
- Completed the shared mine-gallery EFX reference with the remaining accepted
  Rework echo, modulation and room-rolloff fields and expanded its host test to
  lock the full preset.
- Fixed the Overture binding generator's ordered-map construction for Windows
  PowerShell; `-Check` again validates all eight generated controller bindings.

This project is pre-alpha. Entries distinguish implemented infrastructure from features validated live or in a headset.

## Unreleased

### Changed

- Split renderer-neutral eye/target types from the OpenVR session API, and
  narrowed the Black Plague body adapter to explicit owner-status, request and
  callback headers. RVAs, native ownership and runtime behavior are unchanged.
- Added an explicit Black Plague probe capability bitmap and launcher report so
  successful core initialization no longer hides unavailable input, body,
  adapter, ownership or interaction subsystems.
- VR settings updates now use a same-directory temporary copy and atomic
  replacement, preserving unrelated INI sections and avoiding partially saved
  profiles.
- Ignored the local `work/` research scratch directory and removed a historical
  root patch whose changes are already represented by tracked source and
  documentation.

### Fixed

- Reworked the Black Plague room-scale body reconciliation into one explicit
  pre/post native-tick transaction. The existing `D460A -> D6E00` owner now
  plans from the current B0 and latest tracking sample before the native update,
  `0xD7281` consumes the bounded physical request in that same tick, and the
  adapter reconciles once from B1 using matching tick/body/generation evidence.
  The old post-tick plan-for-next-tick path was removed. Host tests cover
  free-space ramps/stops, repeated presentation samples, 60 Hz physics with
  72/90/120 Hz presentation cadence, block/slide/jitter, recenter and body
  replacement. This is host-tested only; the prior short-motion pullback report
  still requires a fresh headset gate.
- Hardened Black Plague crouch and probe lifecycle ownership. Native crouch
  queries are preserved outside an active VR ownership window; VR-owned posture
  is tied to session/player generation and keeps `ChangeMoveState(4/0)` as the
  game-thread application boundary, while the shared crouch policy distinguishes
  desired/effective stance and blocked stand release. Probe startup/shutdown now
  retains an explicit partial-state component ledger, failed rollback/shutdown
  remains retryable, launcher remote-call timeout is reported as indeterminate,
  and rel32 hook setup allocates/opens thread resources before suspending peers.
  These changes are host-tested only; the bridge/lifecycle fault harnesses pass,
  while headset posture validation remains pending.

- Corrected Black Plague VR crouch ownership after PID 24948 showed that the
  previous release/press compensation could alternate the native `1.65/0.95 m`
  body while failing to hold the game's real crouch/stealth state. Static
  exact-build decoding identifies `cPlayer::ChangeMoveState` at `0x9C750`; the
  original normal-state handlers prove move-state `4` is crouch, `0` is walk
  and `3` remains jump. The existing game-thread owner now applies the shared
  Rework desired crouch directly through `ChangeMoveState(4/0)` rather than
  feeding it back through configurable `0x9CFA0/0x9CFD0` press/release
  callbacks. Probe telemetry and the focused helper now require native move
  state, collider shape, button-only latch and Hybrid composition to agree.
  Release build, exact-image verification and 30/30 host tests pass; headset
  validation remains pending.

- Fixed the Black Plague direct metric locomotion gate exposed by PID 17612.
  Full left-stick deflection reached OpenVR frame telemetry while every
  `locomotion_*` field remained zero. Exact-build decoding showed that native
  `MoveForward/MoveSideways` return on `amount == 0` before their later
  `cPlayer+0x264 = 1` write, so that byte cannot be a permission oracle after VR
  analog is removed from the native axes. The bridge now evaluates only the
  exact pre-Move state predicate, preserves real native-axis priority, queues
  accepted VR motion through the existing `0xD7281` owner and mirrors `+0x264`
  after successful direct publication. The exact-image verifier pins the
  relevant instructions and x86 Release compilation passes. Headset validation
  remains pending.
- Corrected the indexed-state layout used by that pre-Move predicate after PID
  28996 showed the first fix still could not publish locomotion. Full controller
  analog continued to reach the runtime, but the replicated lookup had dropped
  the `0x200` portion of both field pairs and swapped vector/index ownership.
  The bridge now follows the exact image: `vector +0x2C4 / index +0x2BC` and
  `vector +0x2D8 / index +0x2D0`; the exact-build verifier pins those loads.
  PID 8092 headset-validated the corrected path: stick locomotion worked in all
  directions, the helper reported `direct_locomotion=True`, all four physical
  outcome classes and `201/201/201/201` queue/consume/inject/match evidence,
  with native crouch-shape recovery also passing. Physical crouch by tracked
  height remains a separate feature gate.
- Replaced Black Plague's direction-dependent native VR acceleration during the
  transient room-scale gate with Rework's direct metric locomotion policy. PID
  11804 confirmed that tracking-only heading chose the correct visual direction
  but exposed slower backward/lateral native axes when the hidden body and HMD
  headings differed. Full-deflection stick now requests `1.5 m/s` walking or
  `2.25 m/s` sprinting in the current HMD world direction. The request is merged
  into the existing `0xD7281` collision owner, physical motion receives priority
  inside the existing `0.05 m` combined bound, and accepted physical/locomotion
  components are separated before anchor reconciliation. Keyboard and rejected
  native player states retain their native path. New probe fields expose both
  requested/injected/accepted components. This is host-tested only.
- Replaced Black Plague's camera-derived stick-heading remap with the proven
  Rework tracking boundary: horizontal locomotion yaw is now measured directly
  from the recenter HMD anchor to the current raw HMD orientation. Pitch/roll
  cannot steer walking, invalid/near-vertical samples fail closed, and recenter
  no longer needs to repair a stale rendered-camera basis. The shared tracking
  math has a host test. PID 11804 headset-exercised this correction and confirmed
  the requested direction; the axis-speed mismatch above remained.
- Corrected the Black Plague physical-displacement helper's blocked/slide
  classification after PID 13672 produced a false-negative blocked result even
  though the wall-block case had been performed. The helper now mirrors Rework
  `23c890f`: accepted displacement is projected onto the requested direction,
  and only that component reduces rejected distance. Native lateral collision
  correction can no longer hide a substantially rejected direct request. This
  changes validation classification only; gameplay collision behavior is
  unchanged.
- Corrected the Black Plague active room-scale presentation after PID 21548
  passed the technical gate but exposed continuous world shake. In stick-zero
  telemetry the reconciled X/Z offset changed by a median 15.77 mm between
  logged samples and repeatedly reversed direction. Unlike Rework `23c890f`,
  the backend was retaining a ~60 Hz body offset over Black Plague's native
  smoothed/bobbed camera. Rendering now derives horizontal camera placement
  directly from the fresh reconciled head anchor and advances it with the HMD
  delta since that body sample. Camera, visibility and controller space consume
  the same placement; discontinuities fail closed, Y/jump remain native and no
  body tick or hook is added. Separate telemetry records the reconciled offset,
  render prediction and final head anchor.
- Corrected Black Plague room-scale anchor carry after PID 24956 exposed a
  sequence difference from Rework `23c890f`. Black Plague's one owned native
  tick contains both native locomotion and the previously queued physical
  request; the shadow consumer was carrying the anchor with that whole combined
  displacement after already reconciling the physical component. It now removes
  the matched physical displacement before Rework's locomotion-only carry while
  retaining the actual whole-tick body position for camera space and telemetry.
  New telemetry reports the partition as `locomotion_carry`. The same session
  reported character walking during in-place head tilt; the correction has not
  yet been tested in a headset and is not claimed to resolve that symptom.
- Relaxed the room-scale helper's redundant post-crouch collision repetition.
  PID 24956 proved `1.65 -> 0.95 -> 1.65 m`, immediate room-scale recovery, 414
  free and 25 slide/partial samples after standing; requiring another blocked
  sample after the already proven global blocked case added no distinct shape
  evidence. The helper now requires meaningful post-recovery physical movement.
- Corrected the VR settings transaction flush. The documented all-null
  `WritePrivateProfileStringW` cache-flush call returns zero even when it
  performs the flush, so treating that value as failure rejected mirror/config
  saves and surfaced `ERROR_FILE_NOT_FOUND`. The transaction now performs that
  cache flush without interpreting its return and then durably flushes the
  temporary file handle before atomic replacement.
- Made IAT pointer replacement fail transactionally when page-protection
  restoration fails instead of reporting success with a writable hook page.
- Hardened OpenGL telemetry and Black Plague spatial-interaction hook lifecycle:
  partial installs are reported, rollback errors are retained and every hook is
  considered during teardown.
- Hardened shutdown after successful initialization: launcher capability-query
  and required-capability failures now perform compensating shutdown before
  returning failure. Black Plague spatial interaction and native input teardown
  now remove owned hooks before waiting for in-flight callbacks to quiesce.
- Reject signed `SettingsVersion` values and retain the OpenVR loader handle
  when `FreeLibrary` fails so shutdown can be retried accurately.

These maintenance changes and both room-scale corrections compile in both
Release configurations. No test executable, game, SteamVR or headset process was
run while preparing the presentation correction; PID 13672 later
headset-exercised it and the user reported the prior continuous world shake gone.
PID 11804 headset-exercised the heading correction and reported stable general
comfort plus correct visual direction, but did not capture fresh blocked or
slide/partial evidence. The subsequent direct metric locomotion change compiles
in Release and its combined-request/reconciliation host tests pass; it has not
yet been live/headset-tested in Black Plague.

### Added

- Added Rework-derived crouch ownership as shared runtime policy. It preserves
  `23c890f`'s single button latch, Hybrid OR composition, plausible
  `(0.90, 2.20) m` raw-HMD range, upward-settling standing baseline, configured
  crouch depth (`0.25 m` default) and `0.08 m` exit hysteresis. Black Plague
  exposes `CrouchMode`, `PhysicalCrouchDepth` and `HeightOffset`; its existing
  game-thread input owner applies the desired stance through Black Plague's
  exact `cPlayer::ChangeMoveState` boundary, retries a blocked stand and logs policy/body
  correlation without adding another hook. Rendering now uses shared
  `VrTrackingSpace` for continuous tracked Y from the reconciled feet anchor.
  PID 20520 exercised the previous edge-only integration and exposed a false
  helper pass: policy exits and the native `1.65/0.95 m` shape were not aligned.
  The corrected latch, desired/native synchronization, vertical placement and
  stricter validator are implemented and host-tested only.
- Added Black Plague probe telemetry for the fresh movement yaw consumed by the
  native-input stick remap. PID 13672 headset-exercised the render-rate anchor
  correction and the user reported the prior continuous world shake gone, but
  later testing found that forward stick direction can feel offset unless
  recentered. That evidence led to the tracking-only heading correction above.
  The next headset gate keeps `movement_yaw_valid` and `movement_yaw_rad` beside
  raw controller input to verify the new basis before further locomotion or turn
  ownership changes.
- Extracted the demonstrated tool attachment socket composition into shared
  `vr_grab_pose` policy: per-game model-to-hand orientation and measured model
  grip points now compose through one runtime helper. Black Plague's flashlight
  and glowstick consume that helper with their existing measured sockets, while
  exact matrix tests prove numerical parity with the previous backend-local
  implementation. Release build and all 30 root CTest tests pass; definitive
  hand/tool geometry and light direction remain headset validation gates.
- Re-audited the remaining Rework VR-only systems after that extraction and
  documented the current host-only frontier: dimming, staged loading, physical
  crouch, game UI flows and jointed mechanisms all still require either a real
  second backend consumer or target-specific live/native evidence before a new
  shared abstraction is justified.
- Extracted Rework's controller-to-finger curl conditioning into shared runtime:
  the measured `0.08` skeletal deadzone, grip/trigger fallback closing windows
  and ~70 ms exponential smoothing now have one host-tested implementation.
  Overture consumes the full policy while retaining its rig order, bind poses,
  handle geometry and forced-grab presentation. Black Plague applies the shared
  skeletal deadzone/smoothing before its richer per-finger articulation; its
  non-skeletal fallback remains unchanged until normalized grip/trigger analogs
  are exposed by the backend. The full Overture Release regression and all 30
  root CTest tests pass; no new live/headset validation is claimed.
- Extracted Rework's game-neutral tracked-menu pointer policy into shared runtime
  helpers and applied it to Black Plague: configured-hand ownership now falls
  back to the other tracked hand, controller aim falls back to grip when needed,
  panel-plane hits clamp to the nearest UI edge, and cursor motion uses Rework's
  `0.40` smoothing factor. Release build and all 30 root CTest tests pass; this
  is host-tested only and does not claim headset validation.
- Host-side SteamVR controller-profile parity gate: the shared action manifest
  and all eight functional binding graphs are checked against the preserved
  Overture mappings. The gate caught and restored the Rework-proven left PS VR2
  Sense `L2` UI-select route while leaving hardware behavior for live/headset
  validation.
- Imported the existing Spanish localization payloads for Black Plague and
  Requiem under `assets/localization`, preserving their original `leeme.txt`
  attribution notices. A small localization manifest fixes their exact hashes
  and install-relative destinations, and the metadata gate now verifies both
  payload integrity and attribution-file presence for future unified-installer
  deployment.
- Complete Framework persistence for the shared Rework-derived VR settings schema, plus a host-tested 18-row editor policy matching the Overture menu's ordering, formatting, step sizes, wrapping, clamps and snap/smooth row dependencies.
- An explicit Black Plague VR-setting capability map exposes only backend-wired editor controls; persisted-but-unwired settings remain unavailable to a future in-game page until their backend application exists.
- An offline Black Plague VR configuration surface is available through `PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`. It shows only backend-consumed Rework-derived controls plus monitor mirror, supports snap/smooth dependent rows, saves through the shared settings store and resets only supported controls so persisted-but-unwired values are preserved.
- Exact canonical/LAA build variants for the observed Black Plague and Requiem executables. Catalogue/manifests preserve the canonical semantic build identity while fingerprinting the one-bit transformed executable independently.
- One-shot Black Plague shadow-install telemetry records `disabled`,
  `environment` or transient `mutex` activation, making a live request
  distinguishable from a silent no-tick session. The synthetic default-off,
  environment and mutex paths are covered by the body-probe test.
- Shared stateless tracking/body planning, physical rejection correction and locomotion anchor carry, consumed by Overture without changing the tested sequence.
- Black Plague direct locomotion now maps exact-build action states `1` (Push) and `2` (Move) to the shared Rework-derived constrained `0.5 m/s` policy. The mapping is pinned by exact-image evidence (`MaxPushSpeed`, the Push `ChangeMoveState(2)` transition and the Move native body callback) and covered by the native-input contract harness; it remains host-tested only.
- Default-off Black Plague tracking/body shadow diagnostics (`PVR_BP_RECONCILIATION_SHADOW=1` or transient validation mutex), using existing tracking and native body callbacks; no physical request injection or positional camera translation. Portable/Windows host tests pass and PID 28172 live-tested the mutex path with positional translation still zero.
- Default-off Black Plague physical-displacement validation path at exact-build RVA `0xD7281`. It injects a one-shot bounded X/Z request (maximum `0.05 m`, Y zero) immediately before native horizontal collision comparison, preserves the original instructions and single `D6E00` owner, and records queue/injection/acceptance telemetry from a pre-injection baseline. PID 26144 live-tested the boundary with positional HMD translation still zero.
- `tools/Start-BlackPlaguePhysicalDisplacementValidation.ps1` verifies the supported initialized executable, launches through Steam, holds the transient physical-validation mutex and fails closed unless the fresh probe log proves a non-zero queued plan, consumed/injected pre-collision request, matched reconciliation, injected body telemetry, the native `dt~=1/60` body tick and sampled stationary/free/block/slide-or-partial cases. The stationary classifier now tolerates sub-2 mm real-HMD jitter instead of requiring zero injection; Rework `23c890f` reacts to any non-zero HMD delta. PID 18392 confirmed that activation alone is rejected, while PID 26144 supplies the completed live evidence.
- Default-off Black Plague active room-scale validation. A dedicated transient request is accepted only together with the live-tested physical-displacement owner; a fresh body-generation-matched shadow sample supplies the reconciled horizontal anchor. PID 21548 captured all physical classes and crouch recovery and confirmed that the carry correction removed locomotion from in-place head tilt, but continuous world shake prevented headset validation. The latest implementation places X/Z from that anchor and continues it with the current render pose; samples expire after 250 ms, Y/jump remain native and no additional body tick is introduced.
- `tools/Start-BlackPlagueRoomScaleValidation.ps1` enables mirror-on persistence, holds both transient validation mutexes and requires fresh evidence for camera application, non-zero reconciled X/Z offset, render-rate HMD continuation, mirror gameplay frames, the native `1.65 -> 0.95 -> 1.65 m` crouch shape sequence, and meaningful physical movement after standing again.
- `PenumbraVR.ProbeLauncher.exe --set-vr-mirror on|off` changes the persisted mirror choice without requiring a running game or an in-game VR settings page.
- Dedicated Windows CI regression job for the autonomous Framework-owned Overture Release pipeline. It forces a clean full product rebuild and runs the retained project/shader/visual/texture/LAA/`VRTrackingTest` gates after shared-runtime changes.
- Initial `pvr_overture_backend` gameplay core with a narrow HPL body/jump adapter boundary, ported from Rework revision `23c890f`.
- Source-level Overture integration that compiles the Framework backend into `Penumbra_vr.exe`, maps the existing HPL input/settings/tracking types, and implements `OvertureBodyAdapter` with the real `cPlayer` and `iCharacterBody` calls.
- Framework-owned Overture product host under `products/overture`, containing the required Penumbra/HPL/OAL source, pinned Win32 dependencies, resource inputs, build/package gates and retained upstream notices.
- Overture-specific OpenVR binding overlay under `assets/openvr/overture`; shared bindings remain available to the other backends.
- Shared tracking-space, height/seated calibration, world-yaw, room-scale collision reconciliation, fixed-displacement locomotion and interaction-reach policies.
- Shared `runtime::VrAcceptedBodyMotion`, a game-neutral observation of finite body-before/body-after positions and accepted displacement. Overture now consumes it without changing its proven reconciliation behavior; Black Plague exposes the same observation through its exact-build body boundary.
- Exact-build Black Plague player/character-body/native-shape/movement/collision mapping plus read-only telemetry for body/feet position, physics timestep, requested displacement, immediate solver output, final accepted displacement and unapplied HMD/body divergence.
- Live-characterized Black Plague sprint/crouch/jump ownership: native walk/sprint limits around `3.0/4.5 m/s`, physical crouch shape swap preserving feet height, and a separate native Jump-state vertical pipeline.
- First narrow `BlackPlagueBodyAdapter`, binding to the already-owned movement/body-update callsites without introducing another hook or second `D6E00` call. It dynamically re-resolves the current body, publishes existing native horizontal intent and observes accepted displacement after the one native update.
- One-step Black Plague `--launch-vr` / `Start-Black-Plague-VR.cmd` and read-only `--check-vr` preflight.
- Real OpenVR action/pose/skeleton/haptic reader, exact-build native input bridge and tracked desktop-menu panels consuming the shared Rework pointer policy.
- Provisional depth-tested procedural gloves, controller-directed native picking and palm-relative free-body grab/throw adapter. Native physics transitions are preserved; joints, palm collisions and definitive tool/light attachment remain pending.
- Per-eye light-scissor remapping at the main executable's `glScissor` import, scoped to the eye context, framebuffer and viewport, with per-frame counters.

### Fixed

- Hardened Black Plague exact-build hook lifecycle handling: partial installs are
  rejected as incomplete, published module identity is kept stable across
  reinstall attempts, and rollback/removal paths preserve callback targets that
  in-flight wrappers may still require. Release build and all 30 root CTest
  tests pass.
- When the Black Plague monitor mirror is disabled, continuous stereo clears the desktop gameplay backbuffer to black instead of leaving stale desktop contents that produced the reported growing white-point artifact. PID 19192 confirmed that gameplay is black while native 2D menus remain visible because they do not execute the suppressed world pass.
- `PenumbraVR.ProbeLauncher` now waits for the actual owner DLL of forwarded
  `LoadLibraryW` to appear in a freshly Steam-started process instead of failing
  immediately during the startup race. The existing process-exit and bounded
  timeout behavior is preserved; PID 28172 subsequently live-confirmed startup past this boundary.
- The Black Plague shadow validation helper now verifies the fresh probe log and
  fails closed unless the requested session reports
  `body_reconciliation_shadow enabled=1 source=mutex`, preventing a silent
  shadow-off launch from being counted as validation.
- Black Plague body-adapter installation now respects single-owner callsites. `NativeInputBridge` remains the sole owner of `MoveForward/MoveSideways`, `BodyCollisionProbe` remains the sole owner of `D460A -> D6E00`, and the adapter binds through their verified live status instead of re-validating pristine bytes or stacking another hook.
- Body-adapter mismatch diagnostics now identify concern, RVA, pristine/live instruction bytes, decoded targets and owner state.
- Generic Black Plague controller picking is capped to Rework's `0.18 m` direct physical reach instead of granting all props the native camera-ray distance.
- Shared snap-turn activation uses Rework's post-dead-zone `0.65` threshold; smooth and snap turning both require a neutral sample on gameplay entry.
- Interact ownership remains with the grabbing hand until it releases; a second controller press no longer transfers ownership or masks release.
- Launcher export relocation now uses the actual owner of forwarded Windows exports and rejects a probe loaded from a different build path.
- Eye bindings isolate and restore scissor enable/box state, preventing a previous desktop or light rectangle from clipping the next eye's clear.
- Black Plague stereo rescales desktop-pixel scissor bounds to eye pixels. This targets the reported medium-distance lamp illumination dropout; physical confirmation remains pending.

### Current integration state

- Overture now builds entirely from this repository. `pvr_overture_backend` preserves the proven Rework tracking-space, room-scale rejection/reconciliation and `1.5/2.25 m/s` locomotion policy behind an Overture-specific HPL body/jump adapter. The exact autonomous Release artifact has been deployed and functionally exercised in a headset; exhaustive equivalence remains a separate evidence gate. The autonomous Release pipeline is also now a dedicated CI regression job.
- The migration audit records the explicit `REWORK → FRAMEWORK → DIFFERENCE → CAUSE → SOLUTION` comparison and remains the authoritative reference for what was ported versus what still requires game-specific mechanism.
- Black Plague active positional HMD translation remains default-off outside its
  validation request. PID 21548 live-tested the carry partition, PID 13672
  headset-exercised stable render-rate horizontal placement, PID 11804 confirmed
  correct HMD-relative direction, and PID 8092 headset-validated the corrected
  direct Rework `1.5/2.25 m/s` locomotion plus the room-scale technical gate
  through the same single `0xD7281` collision owner. This is still a research
  validation state rather than a supported-release claim.
- Black Plague native jump ownership remains separate from shared horizontal intent. Physical crouch ownership and tracked-Y presentation are implemented and host-tested after PID 20520; final headset comfort plus camera/body/footstep bob remain separate milestones.
- The desktop monitor mirror remains experimental and is not a current supported gameplay feature. PID 19192 confirmed mirror-off gameplay black plus visible native menus. Mirror-on and Alt+Tab/focus recovery remain in the next headset batch because the tracked-menu capture path depends on the desktop framebuffer/focus.
- Rework revision `23c890f` remains the immutable Overture behavioral baseline. Its working tree is not a Framework build or packaging dependency.

### Validated

- The current OpenVR and no-OpenVR configurations compile under MSVC with warnings treated as errors at the validated checkpoints.
- The full root CMake configuration now registers **thirty** CTest tests. The established hosted Windows x86 suite still intentionally excludes only the real-driver `opengl_eye_targets` pixel test from the SDK-less runner; newer local tests remain host validation until a subsequent CI run covers them.
- Local offline validation on 2026-09-12 passed the Release build and all **30/30** root CTest tests, including the real-driver `opengl_eye_targets` test. No game, SteamVR or headset process was launched for this validation. The earlier full autonomous Overture Release regression passed the project, 16-shader, 8,752 visual-reference, 231-texture decode and Large Address Aware gates plus **289 `VRTrackingTest` checks with 0 failures**.
- GitHub Actions run `34616035023` host-validated feature commit `18c63ef` on Windows Server 2022: metadata/OpenVR assets, Visual Studio 2022 Win32 configuration, Debug build/CTest and Release build/CTest all passed.
- GitHub Actions run `34616820448` repeated the root Windows x86 gate after CI hardening and also passed the new `Overture Release regression` job using `Build-OvertureProduct.ps1 -Configuration Release -Full`.
- The Framework-owned Overture Release pipeline passes project checks, 16 shader compilations, 8,752 CPU visual checks, 231 texture selection/decode checks, 289 `VRTrackingTest` checks, Large Address Aware verification and package validation. This pipeline has now been revalidated after the shared tracking/body extraction by the dedicated Windows CI job.
- The Framework-owned Overture Debug full rebuild also passes Large Address Aware verification and all 289 `VRTrackingTest` checks; only the game project disables legacy `/Gm` to coexist with C++20.
- The user deployed the autonomous Overture Release overlay with `Install-PenumbraVR.bat` and ran the exact packaged executable (SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`) with SteamVR, a real headset and controllers. The first functional pass felt equivalent to the previously tested Rework behavior and exposed no evident regression. This is initial headset validation, not exhaustive feature/hardware coverage or a supported-release claim.
- Black Plague native stereo, yaw-aligned rotational tracking, keyboard/mouse preservation and HMD-aware visibility have previously been validated in the headset.
- Black Plague PID 30896 live-validated the mapped `0.70 x 1.65 m` player body, `dt=1/60`, free movement and collision rejection/slide behavior with positional HMD translation still disabled.
- Black Plague PID 24780 live-validated corrected sprint/jump/crouch action ownership and the native `1.65 m → 0.95 m` crouch shape swap preserving feet height.
- Black Plague PID 29672 completed a 240-tick jump burst, confirming separate native vertical ownership, about `+5.53 m/s` initial accepted vertical speed, ~`0.95 m` apex above baseline and native landing/state restoration.
- Black Plague PID 8628 live-validated the first `BlackPlagueBodyAdapter`: input/body owners and adapter installed together, free movement/block/slide remained intact, body replacement did not leave a stale cached pointer, and the body update remained about 60 Hz with no evidence of a second `D6E00` call.
- Black Plague PID 28172 live-validated the reconciliation shadow request through `source=mutex` with positional translation disabled. Stationary/small physical HMD deltas, native free/block/slide, recenter/body replacement and the existing ~60 Hz single body tick were observed without the shadow writing camera/body position.
- Black Plague PID 26144 live-validated the physical-displacement boundary at `0xD7281` with positional translation zero: queue/injection/matched reconciliation and the native `dt~=1/60` tick were observed. Corrected classification of the same log yields 12 stationary/jitter, 37 free, 2 blocked and 15 slide/partial samples.

### Earlier work in this release

- Continuous Black Plague stereo-presentation start/stop lifecycle using the already validated yaw-aligned rotation tracking.
- Runtime-derived per-eye resolution with proportional allocation fallback.
- Shared HPL1 camera-matrix transaction adapter.
- Host-independent references and tests for Enhanced visuals v4 calibration and the accepted spatial-audio behavior from Overture VR Rework.
- Pure, idempotent x86 PE32 Large Address Aware inspection/transformation with unit tests and exact host-verified Black Plague/Requiem transformed hashes. No installed executable is modified by this work.
- Shared OpenVR action manifest and controller bindings, including PSVR2 Sense.
- Exact-build LAA observations for the installed Black Plague and Requiem Steam executables.
- Unified installer transaction and rollback design.
- Repository metadata validation for exact-build manifests, the compiled build catalogue and OpenVR action/binding references.
- Windows x86 CI for metadata validation plus Debug and Release builds/tests without the external OpenVR SDK.
- Exact-build static mapping of Black Plague's `cScene::UpdateRenderList` call and `cRenderer3D::UpdateRenderList` target.
- A reversible visibility-camera transaction and conservative symmetric cull frustum covering both asymmetric eyes, with a five-degree pose-age guard.
- A Black Plague render-list hook that uses the previous valid tracked pose only during continuous stereo and reports update, failure and restoration telemetry.
- Rework-derived stereo scheduling that defaults continuous presentation to two eye world passes and permits an optional third monitor-mirror pass.
- Runtime launcher commands to enable or disable the monitor mirror while the probe is attached, plus per-frame pass and frame-time-ownership telemetry.
- A device-independent VR input router adapted from Rework, covering radial dead-zone scaling, context and handedness edge latching, pose-loss releases and the 500 ms SteamVR action-idle grace period.
- A runtime-owned VR settings model adapted from Rework, with shared defaults, ranges, enum text values and legacy smooth-turn migration, plus framework monitor-mirror state.
- Persistent monitor-mirror preference in `%LOCALAPPDATA%\PenumbraVR\settings.ini`.

### Changed

- Renamed the GitHub repository from `rubocopter/penumbra_vr` to `rubocopter/penumbra_vr_framework`, while retaining **Penumbra VR** as the public project name, to distinguish this trilogy framework from the original `veryjos/penumbra_vr` Overture mod.
- Moved camera override behavior out of the Black Plague backend into the shared HPL1 adapter; exact camera offsets remain backend-owned.
- Documented Enhanced visuals, audio, action, tracking and locomotion provenance against `rubocopter/penumbra_vr_rework` revision `23c890f`.
- Clarified the extraction rule: Rework remains the proven Overture reference, while a better demonstrated implementation from another backend may become the shared baseline when the reusable behavior is genuinely game-neutral.
