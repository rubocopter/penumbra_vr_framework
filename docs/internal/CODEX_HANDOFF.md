# Codex handoff — Penumbra VR Framework

This is the operational checkpoint for future coding sessions. Read `AGENTS.md` first. Read `DEBUG_HANDOFF.md` before changing any known regression boundary, use `docs/REWORK_PORTING_PLAN.md` for the extraction contract, and use `docs/TRILOGY_PARITY_PLAN.md` as the authoritative cross-game capability ledger and framework-readiness gate.

## Source-of-truth rule

`rubocopter/penumbra_vr_rework` revision `23c890f` is the proven behavioral reference for Overture VR. Do not invent a replacement for demonstrated Rework behavior before locating the original implementation and tests, separating game-neutral policy from game/HPL mechanism, and documenting why direct adaptation would be unsafe or impossible.

The framework is not a one-way Overture port. If another backend demonstrates a stronger game-neutral implementation, preserve the better behavior and move it toward shared runtime policy while keeping per-game layouts, rigs and native state behind adapters/profiles. Black Plague finger articulation is currently the clearest example.

Do not equate extraction with a completed port. A Rework capability remains open for Black Plague until the backend consumes the shared behavior, implements a documented target-specific equivalent, or records an evidence-backed not-applicable classification. Requiem gameplay is sequenced after the Black Plague framework-readiness gate in `docs/TRILOGY_PARITY_PLAN.md`; non-invasive exact-build reconnaissance may proceed earlier without displacing the active Black Plague parity work.

## Repository checkpoint

### Consolidated checkpoint — 2026-09-18

The current offline candidate is coherent and ready for the next combined
Black Plague headset gate. Release root validation is **36/36 CTest**, the
supported initialized-image verifier passes, and the complete Overture Release
regression remains green. No new live/headset promotion is implied by those
host-side results.

The working tree now includes the host-tested Black Plague consumers that should
be judged together in the next headset session: collision-resolved palms with
Rework interaction assist/recovery telemetry; corrected Grab/Move acquisition;
static-only physical stepping; imported Rework hand rendering/five-finger
articulation; the verified Rework glowstick profile; magnetic item targeting;
one-joint `cGameLever` hinge/slider plus hinge-only one-joint `cGameSwingDoor`
mechanism servo consumption; native inventory/notebook and gameplay 2D overlay
presentation; LightToggle/Damage/confirmed-contact MeleeImpact haptics; HRTF
startup configuration; and the transferable Enhanced Visuals per-eye final
stage. Each remains at its documented evidence level, generally host-tested,
until the corresponding runtime/headset gate is exercised.

The next primary action is **not more offline tuning** of those behaviors. After
a clean reboot, run `tools/Start-BlackPlaguePalmCollisionValidation.ps1` and use
its combined checklist. The session should first prove stable presentation, then
wall/table palm slide without abrupt snap, rapid small-prop Grab/Move,
bounce-free wall pressure plus physical low-static-step traversal, normal stick
stairs, corrected glowstick/hand/finger presentation, short X/Z + crouch before
and after interaction, and representative Lever/SwingDoor constrained motion.
Use the same run opportunistically for HUD/subtitles, haptics, HRTF and Enhanced
Visuals where the test area exposes them. Do not promote a row merely because
the helper records another unrelated subsystem.

After that combined gate, the priority order remains the parity ledger:

1. close Hybrid release-hold, blocked stand/low ceiling, constrained Push/Move,
   tracking-world-yaw/recenter/tracking-loss, VR footstep surface/bob evidence;
2. close interaction/presentation evidence for magnetic pickup, mechanisms,
   inventory/notebook, HUD/subtitles and definitive tool/light placement;
3. close output parity for haptics/HRTF and map only evidence-backed runtime
   boundaries for occlusion/reverb and the remaining HPL Enhanced Visuals
   material/light response;
4. complete representative Black Plague chapter-level validation and the
   framework-readiness gate;
5. only then make Requiem gameplay the active milestone.

- Current Black Plague palm/grab candidate (2026-09-17, after PID 1736): the
  interrupted follow-up is now complete and host-verified. The latest live log
  had shown the resolver active but continuously constrained during surface
  contact, very few acquisitions despite many interaction attempts, physical
  step suppression on every physical-only obstacle case, and live varying
  finger curls. Exact Rework comparison produced four concrete corrections.
  First, the VR ray proxy now returns `true` after each hit so HPL/Newton can
  enumerate all candidates before Framework ranks the bounded five-ray set;
  pending Grab/Move acquisition retains the originating hand and accepts the
  still-held interaction instead of requiring a second `just_pressed` edge.
  Direct nudge also skips that hand while selection is pressed or pending.
  Second, Rework's bounded interaction assistance is ported exactly at policy
  level: an ordinary nearby physical pick publishes its contact point and the
  shared resolver selects the `0.008 m` skin only when the raw controller is
  moving toward that target from the resolver's actual post-recovery/reanchor
  start. Recovery thresholds are unchanged; telemetry now distinguishes
  tracking reanchors, recovery anchors, pullback recoveries and assist samples.
  Third, the old BP `0xD7361` blanket physical-step suppression was wrong:
  Rework enables step for static geometry only. The current exact-build adapter
  observes the existing `cCharacterBodyRay` callback (`+0x21C`, vtable
  `0x67F7B0`, `OnIntersect=0xD4E00`), classifies its winning body through the
  proven mass field `+0x434`, and at `0xD7772` discards only a dynamic winner on
  physical-only ticks. Static beams/low steps therefore keep native step while
  stick/native locomotion remains untouched. Fourth, the BP glowstick primary
  cylinder position stream matches Rework, so its proven model profile is now
  reused exactly (`scale 1.55`, grip point `0,0.0078,-0.078`, X rotation
  `4.71`) and tool presentation uses Rework's authored long-finger grip pose.
  Release build, the six focused body/palm/spatial/hand/render tests and the
  supported-image verifier pass. Treat this complete candidate as
  **host-tested only** until the next headset run checks slide/snap, rapid
  pickups, physical low-step traversal, stick stairs, glowstick placement and
  visible five-finger articulation.

- Follow-up hand/interaction candidate (2026-09-17): the first PID 26940 GL
  hardening was incomplete. The real-driver WGL regression now reproduces a
  second fixed-function contamination path by entering the hand draw with a
  VBO-backed `GL_SECONDARY_COLOR_ARRAY` plus `GL_COLOR_SUM`; before the fix the
  sampled skin pixel became `220,255,154`. The shared presentation state now
  disables color summation while drawing VR overlays and `rework_hand_mesh.cpp`
  disables the inherited secondary-color client array, with the caller's state
  restored afterwards. The hostile-state regression now passes. The same
  candidate ports Rework `23c890f`'s direct physical hand nudge through the
  existing Black Plague game-thread body owner: one reusable `0.12 m` sphere,
  raw-hand velocity threshold `0.03 m/s`, exact-build shape query, held/player
  exclusions and bounded mass-scaled `AddImpulseAtPosition` with telemetry.
  This nudge boundary intentionally stays narrower than Rework's full object
  classifier: lock/breakable metadata and body-radius policy are still not
  consumed here. Joint type/axis ownership is now mapped separately by the
  spatial-interaction mechanism adapter rather than being inferred by nudge.
  PID 14212 then showed that presenting only
  the last reconciled horizontal head anchor makes rejected wall pressure feel
  like a force pulling the head back. The working tree therefore restores the
  PID 25484 headset-exercised rejected-direction filter: render-rate X/Z
  continuation is preserved between BP's ~60 Hz body ticks, while only the
  component continuing into the last rejected physical direction is removed.
  Release builds, **35/35** CTest, generated-hand determinism, `git diff --check`
  and the supported-image exact-build verifier pass. The new hand/nudge/tool
  changes remain **host-tested only** until one combined headset run checks hand
  collision/slide, Grab/Move alignment, deliberate wall/slide pressure,
  crouch/room-scale and normal stick stair stepping.

- Headset regression checkpoint (2026-09-17): PID 14212 did not exercise the
  gameplay palm resolver. The log stayed at `palm_collision enabled=0
  source=disabled`; the validation helper had released its named-mutex request
  before gameplay reached the one-shot resolver sample. The helper now keeps the
  request alive until both palm and room-scale activation are observed or the
  game exits. Treat the reported wall/table hand pass-through as invalid resolver
  evidence for that run. Direct nudge was active while `grabs_acquired=0`, which
  matches the report that props were pushed away before grip acquisition; nudge
  now skips the active interact hand for the full held grip interval. The hand
  diffuse texture was stable. Thumb and little-finger curl telemetry reached
  approximately `0.648` and `0.90`, so the remaining poor visible articulation
  belongs to the imported mesh/skin/articulation path rather than missing OpenVR
  finger channels. PID 14212 also recorded hundreds of
  `physical_step_suppressed` events; its one large vertical burst was a real
  native jump (`jump=1`, move state `3`), so the smaller beam/obstacle hops remain
  a separate edge case instead of evidence that the historical blanket step
  suppression never ran. That checkpoint still used the provisional BP
  glowstick socket; the newer candidate above supersedes it with the exact
  transferable Rework glowstick profile and still requires headset placement
  evidence.

- Startup regression checkpoint (2026-09-16): PID 28872 failed during
  `PenumbraVR_Initialize` immediately after the input/presentation profile log,
  before the first OpenGL hook-install result. Windows Error Reporting recorded
  `0xc0000005` in the game's `SDL.dll` plus a BEX in `nvoglv32.dll`. The launch
  readiness gate had only proved that the protected RenderWorld call bytes were
  initialized; that can precede completion of SDL/OpenGL window setup. The first
  follow-up gate avoided the crash but never entered VR because the launcher
  required `GetPixelFormat(GetDC(hwnd)) != 0`. That assumption is invalid for
  SDL 1.2/private-DC fullscreen paths. The launcher now requires a non-empty
  game-owned top-level client window with stable handle and dimensions for 20
  consecutive 25 ms polls, without visibility or cross-thread pixel-format
  assumptions. Release launcher build, 35/35 CTest and the exact-build verifier
  pass. Treat this as **host-tested** startup hardening until a fresh combined
  palm/room-scale launch reaches VR; do not attribute PID 28872 to the new
  hand/render timing code without contrary evidence.

- Performance baseline checkpoint (2026-09-16): presentation-state setup now
  caches the current-context OpenGL function pointers, texture-unit count and
  rectangle-texture capability rather than repeating extension/capability
  discovery for each eye/hand draw. Keep the hand renderer's explicit
  client-array/VBO save/restore intact; that boundary is what fixed the PID
  26940 black/rainbow corruption. Render telemetry now exposes accumulated
  `stereo_cpu_ns`, `eye_world_cpu_ns`, `hand_draw_cpu_ns` and
  `compositor_submit_cpu_ns`, and the probe logs them as milliseconds. Use these
  as the baseline before importing Rework lighting/shadow/effect work so
  eye-independent updates remain once-per-game-frame and only view-dependent
  drawing repeats per eye. Release builds, 35/35 CTest, the hostile-state WGL
  regression and the supported-image exact-build verifier pass. This is
  **host-tested** only; do not infer a headset FPS improvement until the next
  clean combined run supplies timing/frame-pacing evidence.

- Latest regression/fix checkpoint (2026-09-16): PID 26940 reached the combined
  Black Plague room-scale + gameplay-palm path and produced two concrete
  regressions. The headset frame showed the imported Rework hands mostly black
  with changing multicolored triangles. The cause is inherited HPL client-array
  state: a VBO-backed `GL_COLOR_ARRAY` can remain enabled and override the hand
  renderer's `glColor4f`, so indexed hand vertices read unrelated host colors.
  `rework_hand_mesh.cpp` now isolates/restores client vertex-array state,
  disables inherited color/normal/index/edge arrays and normalizes/restores
  pixel-unpack state around the generated diffuse upload. The real-driver WGL
  regression enters from hostile VBO/color-array state and also proves that
  changing only the index-finger curl changes the rendered framebuffer. Black
  Plague still preserves the richer five-channel shared articulation; the probe
  now logs skeleton validity plus all ten finger curls for live diagnosis.
  PID 26940's large log also correlated the reported repeated wall-contact
  mini-jumps with native step climbing: X/Z-only physical requests produced
  repeated ~5 cm vertical body changes around blocked/partial solves. The BP
  exact-build adapter initially owned `0xD7361` in addition to `0xD7281` and
  skipped the native step-climb phase for physical-HMD-only ticks. Later Rework
  comparison proved that adaptation too broad: `23c890f` allows room-scale step
  climbing when the winning ray body is static. The current implementation is
  the static-only adaptation documented in the checkpoint above. The historical
  `0xD7361` candidate must not be restored. Release build, 34/34 CTest, the
  supported-image verifier and generated-hand determinism check passed that
  historical stage. Stable texture/articulation, wall pressure and ordinary stick
  stair/ledge stepping.

- Focused headset checkpoint (2026-09-16): PID 25484 exercised the current
  Black Plague crouch/Y and short-range room-scale candidate. Presentation
  analysis found 12,949 logged frames, 12,859 gameplay frames, 12,858
  presentation frames and **zero stereo failures**. Collision-comfort analysis
  found 2,477 meaningful body samples (`2407 free / 43 blocked / 27 partial`)
  across 5,488 active room-scale render samples. The run calibrated standing,
  recorded three physical crouch entries/exits, ended in native move-state `0`
  with the `1.65 m` body and VR ownership released, captured a stable
  button-only state-`4` crouch, reached `0.961 m` tracked render-Y range and
  contained 650 accepted direct-locomotion samples. The user reported that the
  session felt good. This is positive headset evidence for the current
  crouch/Y, direct-locomotion and rejected-direction comfort path. One automatic
  focused-gate requirement remains uncaptured: the run saw the Hybrid combined
  state (`physical=1`, latch=1, state `4`) but no later periodic sample with
  `physical=0`, latch=1 and state `4`. Keep that Hybrid release-hold subgate,
  blocked-stand/low-ceiling, yaw/bob and the other explicitly separate gates
  open; do not repeat the entire focused batch solely to re-prove evidence PID
  25484 already supplied.

- Current presentation/comfort checkpoint (2026-09-14): PID 22096 exercised
  `7f84235` from menu into sustained gameplay with 15,990 logged frames, 15,887
  gameplay frames, 15,885 presentation frames and **zero stereo failures**.
  This is positive headset evidence for the single-consumption presentation
  sequence fix. The same run supplied headset evidence for ordinary physical,
  button and Hybrid crouch plus direct locomotion in every tested direction; do not continue treating compositor error 108 as the current
  blocker unless a fresh run reproduces it. The same closed log was re-analysed
  with `Analyze-BlackPlaguePresentation.ps1 -CollisionComfort`: 2,060 meaningful
  physical samples contained 1,755 free, 215 blocked and 90 partial outcomes,
  and repeated render-prediction resets occurred around rejected body solves.
  Comparison with Rework confirms the remaining structural difference: BP
  predicts a newer HMD pose between its ~60 Hz body ticks. The current working
  tree carries the last physical reconciliation into render and removes only
  the prediction component that continues into the rejected direction, while
  preserving tangential slide and movement away from the obstacle. Release,
  Debug and SDK-less Release pass 34/34 CTest; metadata, the exact-build BP
  verifier and Overture `-Full` (Release/LAA + 289/289 tracking checks) pass.
  PID 25484 later supplied positive headset comfort evidence for this filter;
  retain PID 20520's specifically reported pullback symptom as a narrower edge
  until it is explicitly checked rather than inferring its absence from a
  general "felt good" report.

- Second presentation regression checkpoint (2026-09-14): PID 6016 reproduced
  `VRCompositorError_AlreadySubmitted (108)` on commit `ca099ca`. The fresh log
  proved the first gameplay transition still failed after a menu frame had
  submitted successfully. `ca099ca` fixed nested `UpdateRenderList`
  reacquisition, but presentation snapshots still lacked consumption state: a
  world frame could reuse a sequence that had already been submitted when no
  newer visibility sample was published. The current correction records the
  last successfully submitted presentation sequence and refuses sequence 0,
  equal or older samples before eye rendering. Telemetry now includes
  `presentation_pose_stale_rejects`, and presentation acquisition/reuse/reject
  activity forces a frame log entry. Debug, Release and SDK-less Release each
  pass 34/34 root tests with a pure sequence-policy regression test; metadata
  validation also passes. The Release probe DLL for this candidate has SHA-256
  `48489A2573BC96F56C55F0E0C2E3559450D8E4BD249767C349777B92F1BE1A76`.
  Treat both `3333be1` and `ca099ca` as failed headset candidates. The
  exact-build BP verifier script was later rerun successfully. This paragraph is
  retained as the failed-candidate history; PID 22096 above supersedes its
  pending-headset status for the presentation-sequence fix.

- Headset-candidate regression found on 2026-09-14: PIDs 23656 and 21396
  initialized the Black Plague probe and controller path, but the first gameplay
  stereo transition failed with OpenVR compositor error `108`
  (`VRCompositorError_AlreadySubmitted`). This invalidates commit `3333be1` as
  a headset candidate. The presentation/visibility intervention had allowed
  `UpdateRenderList` callbacks reached from the stereo eye passes to acquire a
  fresh compositor pose and overwrite the pre-RenderWorld presentation sample.
  The current fix validates the mapped gameplay camera before acquiring a pose
  and makes nested eye-pass visibility reuse the owning presentation snapshot,
  so the world pair remains associated with one compositor frame. Debug,
  Release and SDK-less Release each pass 34/34 root tests after the fix;
  metadata and the exact-build BP verifier pass, and Overture `-Full` passes
  its Release/LAA, 16-shader, 8,752 visual-check, 231-texture and 289/289 test
  gates. Fresh headset evidence is still required before restoring any
  live/headset claim for the new presentation contract.
- Release-candidate checkpoint (2026-09-14): keep the next Black Plague Release
  as a validation candidate, not a supported/public release. The authoritative
  headset gate is `docs/VR_HEADSET_TEST_CHECKLIST.md`; it now explicitly groups
  the remaining Hybrid/blocked-stand posture edges, deliberate wall/slide,
  constrained Push/Move locomotion, recenter/tracking-loss, hold-generation,
  gameplay palms, mirror/focus and yaw/bob checks. The
  palm no-write query is live-tested in PID 28412 and its successor,
  `--validate-palm-resolver`, is live-tested in PID 8644 with balanced
  create/reuse/query/destroy and unchanged selected gameplay memory. This still
  does not validate tracked gameplay palms or contact feel with a headset.
- The candidate is now rebuilt and offline-validated for the next headset
  session. Release, Debug and SDK-less Release each pass 34/34 root CTest;
  metadata and the exact-build Black Plague verifier pass; Overture `-Full`
  passes the Release build, Large Address Aware check and 289/289
  `VRTrackingTest` after its existing 16-shader, 8,752-visual-check and
  231-texture gates. At that checkpoint the headset entry point was
  `tools/Start-BlackPlagueRoomScaleValidation.ps1`, which uses the Release
  launcher from `build/bin/Release` and validates the supported game image
  before launch. This evidence remains host/offline evidence only.

- Latest offline checkpoint (2026-09-14): the intervention is implemented in
  the working tree and host-tested. The body path now plans before the sole
  native tick, consumes the combined request once, matches tick/body/generation
  and tracking identity, reconciles once and publishes the resulting anchor.
  Crouch ownership is session/generation-bound and applies native
  `ChangeMoveState(4/0)` on the game thread. Render visibility owns the common
  presentation pose sample and carries pose/yaw epochs into eyes, body and
  prediction. Interaction acquisition revalidates VR selection/contact and
  drops stale holds without dereferencing an old player. Probe lifecycle keeps
  an explicit cleanup ledger, partial state and retryable shutdown; rel32 hook
  error formatting occurs after peer threads resume.
- New host coverage includes the actual NativeInputBridge contract harness,
  every probe install/rollback and teardown point, 60 Hz body ticks with
  72/90/120 Hz render sampling, ramps, repeated poses, wall/slide, recenter,
  body replacement and sub-2 mm jitter. Debug, Release and no-OpenVR Release
  all pass 34/34 CTest tests after adding the no-write contact harness.
  Metadata, exact-image BP verification and
  `git diff --check` pass. Overture `-Full` reached the product build and
  exposed a stale extracted `VRHandNominalRecoveryAnchor` call; that call was
  fixed and the subsequent product gate passed the Release build, Large Address
  Aware check and 289-check `VRTrackingTest`.
- Lifecycle hardening and uncaptured transaction edge cases remain
  **host-tested only**. PID 22096 supplies headset evidence for
  presentation-sequence ownership, while PID 25484 supplies focused headset
  evidence for the current same-tick body path, explicit tracked-Y correlation,
  direct locomotion and the rejected-direction short-X/Z comfort filter.
  Remaining Hybrid release-hold, blocked-stand/low-ceiling and deliberate
  wall/slide edge cases remain open.
  Status remains below the final validation tier.

- Audit baseline (2026-09-14): implementation began from
  `73c70f0aad6fd2f33dc27983a44971ec44215f4a`. Black Plague
  room-scale reconciliation now uses one same-tick transaction around the
  existing `D460A -> D6E00` owner: plan from B0/latest tracking before the
  native update, consume the bounded `0xD7281` request once, observe B1, match
  tick/body/generation, reconcile once and carry locomotion once. The previous
  post-tick plan-for-next-tick epoch mixing is gone. The synthetic body harness
  was updated to prove same-tick injection/reconciliation, native movement before
  injection, recenter invalidation and combined physical/stick accounting.
  Debug and Release both pass 30/30 root CTest after rebuilding these sources;
  the SDK-less Release build also passes 30/30. The broader body-transaction
  extensions remain **host-tested only**, while PID 25484 later supplied fresh
  headset comfort evidence for the short physical-motion path.
- The same intervention hardens crouch ownership and lifecycle without changing
  exact-build owners. Crouch preserves native queries outside VR ownership,
  ties VR stance/pending legacy edges to session and player generation, keeps
  `ChangeMoveState(4/0)` on the game thread, and feeds blocked stand back into
  the shared Rework-derived policy. Probe lifecycle now distinguishes clean,
  initializing, ready, shutting-down and partial states; capability bits remain
  a teardown ledger, failed rollback/shutdown is retryable, callbacks are
  suppressed unless ready, and launcher timeout is explicitly indeterminate.
  `Rel32CallHook` prepares handles/storage before suspending peer threads. These
  changes are **host-tested only**. The real NativeInputBridge harness and
  systematic lifecycle install/remove fault injection are now covered by the
  34/34 suite and must not be promoted beyond host-tested evidence.
- Still open in this working tree: Hybrid release-hold and blocked-stand/
  low-ceiling posture edges, presentation pose/yaw epoch live correlation beyond
  the now-passing sequence-consumption gate, and clean headset validation of the
  host-tested Black Plague palm gameplay path. PID 25484 supplies positive
  headset evidence for short physical-motion comfort and tracked-Y correlation,
  but does not close every deliberate wall/slide or posture edge. Do not claim
  the remaining contracts complete.
- Palm-collision reference research is now pinned more narrowly. Rework
  `23c890f` creates player-owned hand collision shapes once per world, reuses
  them for overlap/sweep-style sampling, and destroys them on world teardown;
  its resolver uses `iPhysicsWorld::CheckShapeWorldCollision` with a collision
  callback, a held-body skip pointer, contact-depth tolerance, recovery anchors,
  stepped translation and rotation refinement. Supported-image analysis now
  proves BP `CheckShapeWorldCollision` RVA `0xD4830`, its nine stack arguments,
  callback/contact layout, body matrix/shape accessors, CreateBoxShape slot,
  shape user count and destruction route. `hand_contact_probe.*` implements a
  default-off no-write query on the existing post-`D6E00` owner and its
  synthetic clear/contact/mutation harness passes. PID 28412 live-tested that
  query with one callback, eight contacts and no selected native-memory change.
  The successor owned-shape/resolver path is live-tested in PID 8644 behind
  `--validate-palm-resolver`. The subsequent gameplay path is host-tested: it
  publishes the held body per hand, substitutes the resolved grip for hands,
  held-object motion and tools, and leaves aim on raw tracking. Target
  acquisition now matches Rework `23c890f`: it follows raw controller
  translation by at most `0.18 m` from the collision-resolved palm while the
  actual hold remains anchored to the resolved palm. PID 23000 is
  inconclusive for headset promotion because severe FPS loss and a right-hand
  controller dropout occurred during that same session. PID 26940 later reached
  the combined path but exposed hand GL-state corruption and physical
  step-climb bounce; those causes now have host-tested fixes and still require a
  clean headset gate before palm gameplay can be promoted.
- PID 4720 is not evidence that the previously good room-scale/crouch behavior
  regressed. Startup explicitly recorded `physical_displacement_validation=0`,
  `room_scale_validation=0` and `positional_translation_enabled=0` while the
  palm mutex was active. The focused palm helper now requests physical
  displacement + room-scale + palms together and requires active room-scale
  rendering plus tracked-crouch telemetry before accepting the run.
- PID 21548 reran the corrected active room-scale path and passed its complete
  automatic gate: queued/consumed/injected/matched were `201/201`, all four
  collision outcomes were observed, crouch/stand recovery passed and in-place
  head tilt no longer caused character locomotion. It still failed headset
  comfort validation because the world continuously shook and wall rejection
  felt aggressive. Analysis of the 625 logged room-scale frames found a 9.65 mm
  median X/Z offset and 15.77 mm median change between stick-zero samples, with
  direction reversal in 430/539 windows. Rework `23c890f` places the rendered
  camera at its VR world anchor and updates tracking at 90 Hz; the Black Plague
  path retained a ~60 Hz body offset on top of the native smoothed/bobbed camera.
  The renderer now places X/Z directly at the reconciled anchor and adds the HMD
  delta since the observed body sample for render-rate continuity. PID 13672
  subsequently headset-exercised that correction: the user reported the prior
  continuous world shake gone and comfort substantially improved. PID 11804 then
  headset-exercised the tracking-only heading correction: general stability was
  retained and stick direction followed current HMD heading without recenter.
  It exposed a second, separate difference: speed still depended on the hidden
  native body axis/sign, so visual forward could inherit Black Plague's slower
  backward response. The next build therefore uses Rework's shared direct
  `1.5/2.25 m/s` displacement and queues it through the existing exact-build
  `0xD7281` collision owner. Physical and stick components share one bounded
  request and are partitioned for reconciliation. PID 17612 then proved that
  full left-stick analog reached frame telemetry while direct locomotion never
  reached the queue. The first adaptation zeroed the VR analog before native
  `MoveForward/MoveSideways` and then waited for `cPlayer+0x264`; exact-image
  decoding shows those methods return on `amount == 0` before the later
  `+0x264` write. The corrected backend evaluates their exact pre-Move predicate
  (two indexed state vtable calls plus `+0x268/+0x26C`), keeps real native axes
  higher priority, queues only accepted VR components through `0xD7281`, and
  mirrors `+0x264` after successful direct publication. The verifier pins this
  boundary and x86 Release compilation passes. PID 28996 then proved the first
  indexed-state lookup was still wrong: it dropped the `0x200` high portion and
  swapped vector/index ownership. The bridge now follows exact
  `vector +0x2C4 / index +0x2BC` and `vector +0x2D8 / index +0x2D0` loads.
  PID 8092 headset-validated the corrected route: the user reports correct stick
  locomotion and substantially better overall feel, while the helper recorded
  `direct_locomotion=True`, all four physical outcomes and
  queue/consume/inject/match `201/201/201/201`. This is headset evidence for the
  technical stick/collision route. Positional comfort is still open because PID
  20520 later reported a pullback sensation during short physical X/Z motion.
- PID 20520 exercised the first tracked-height crouch integration. Physical
  entry worked, but standing physically left the native body at `0.95 m` until
  another crouching gesture. Six aggregate policy entry/exit counts and a
  non-correlated body-shape sequence caused the old helper to pass falsely.
  Rework `23c890f` owns a single persistent desired state; the failed build fed
  held/released policy back through Black Plague's configurable legacy toggle.
  Shared `VrPhysicalCrouchPolicy` now owns the Rework button latch, physical
  baseline/depth/hysteresis and Hybrid OR. The first backend integration applied
  that desired stance through exact entries `0x9CFA0/0x9CFD0`, adopted legacy
  edges, retried blocked stand and published desired/native shape correlation.
  The same physical control reported by OpenVR and legacy input was collapsed
  into one toggle. Rendering uses `VrTrackingSpace` to compose continuous HMD Y
  and `HeightOffset` from the reconciled feet anchor; physical crouch no longer
  adds the full native camera drop. PID 23260 and PID 24948 below show why the
  press/release backend owner itself had to be replaced.
- PID 23260 proved that the shared policy and button route are working but the
  native exit owner was still wrong. The log reached `physical entries/exits =
  10/10` and `button_latched` changed both directions, while the native body
  remained at `0.95 m`: `native_exits=0`, `vr_owned=1` and
  `stand_retries=17832`. The cause is now narrowed to Black Plague's native
  crouch mode: `0x9CFA0/0x9CFD0` are the existing pressed/released dispatches,
  not unconditional crouch/stand setters. In toggle mode the release dispatch
  intentionally leaves crouch latched. The first correction compensated with a
  release followed by another press when needed; PID 24948 disproved that as a
  stable ownership boundary.
- PID 24948 no longer stuck at `0.95 m`, but the user saw only a short crouch dip
  and no persistent native crouch/stealth behavior. The log confirms repeated
  body transitions instead of a stable state: `native_entries=14` and
  `native_exits=14`. Re-analysis of the exact initialized image found the direct
  native boundary Rework conceptually requires: `cPlayer::ChangeMoveState` at
  `0x9C750`. The original Black Plague normal-state crouch handlers prove state
  `4` is crouch and state `0` is walk; jump remains state `3`. The existing
  game-thread owner now applies the shared persistent desired state directly via
  `ChangeMoveState(4/0)`, without another hook or another body tick. Telemetry
  reports `+0x2D0`, and the focused validator requires state `4` plus the
  `0.95 m` collider for crouch, state `0` plus `1.65 m` for standing, a stable
  button-only latch, Hybrid composition and final released ownership. Native
  clearance/body replacement remain authoritative. Release build, exact-image
  verification and all 30 host tests pass. This correction is **host-tested
  only** and must replace the failed PID 24948 build in the next headset gate.
- PID 24956 live-exercised active Black Plague room-scale and exposed a real
  Rework-sequence regression. Its log captured 1159 body summaries, all four
  physical outcome classes, the native crouch/stand shape sequence and camera
  recovery. The user specifically reported that in-place head tilt made the
  character walk; stick then added to or opposed that unintended motion. Static
  comparison also found that Black Plague's one owned body tick combined native
  movement with the prior physical request, but `BodyReconciliationShadow`
  carried that whole vector after physical reconciliation. Rework `23c890f`
  carries only its later, separate stick update. The adapter now excludes the
  matched physical component from locomotion carry while retaining the actual
  final body for camera space. PID 21548 confirmed that this removed the
  tilt-induced walking; PID 13672 later exercised the presentation correction
  described above.
- The first room-scale helper launch exposed a settings-store regression before
  game startup: the all-null private-profile cache flush was incorrectly treated
  as a Boolean success result and reported Win32 error 2. The store now follows
  the documented zero-return flush contract, flushes the temporary file through
  a writable file handle, and retains same-directory atomic replacement. This
  fix requires a rebuilt launcher before retrying the headset gate.
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
- Rework right/left hand meshes with Framework-owned five-finger articulation
  (host-tested presentation; headset scale/orientation/performance still open);
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

`positional_translation_enabled=0` throughout that validation. Therefore the adapter boundary is live-tested, while the later active room-scale consumer still requires its own validation. At that PID 8628 checkpoint, VR speed tuning, physical crouch, jump tuning and camera/bob comfort were still separate; the current direct-locomotion status is recorded in the repository checkpoint above.

## First shared body policy

`runtime::VrAcceptedBodyMotion` is the first body contract used by both Overture and Black Plague. It contains finite before/after positions and accepted displacement only. It must remain free of RVAs, HPL layouts, speed constants and native-update ownership.

Overture consumes it without changing its proven room-scale/locomotion behavior.
Black Plague produces it from the native tick; the live-tested validation path
feeds the matched physical observation into shared reconciliation. A separate
default-off active consumer can now expose the resulting horizontal anchor/body
offset to rendering during the next validation gate.

## Current milestone — physical crouch/Y validated; focused posture edges remain

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

The active room-scale path is implemented behind the separate
`PVR_BP_ROOM_SCALE_VALIDATION=1` or
`Local\PenumbraVR.BlackPlague.RoomScaleValidation` request. Activation fails
closed unless physical-displacement validation is also active. The body adapter
publishes a fresh, body-generation-matched
`predicted_anchor - body_after` horizontal offset; samples expire after 250 ms.
The renderer uses the sample's absolute reconciled X/Z head anchor. Between body
ticks it advances that anchor by the HMD delta since the sampled pose, then
places head view, HMD-aware visibility and controller game-view space at the
same horizontal point. This removes the retained 60 Hz stair step and the
native camera's horizontal smoothing/bob without bypassing the collision-owned
anchor. It does not apply Y and does not add a native body tick.

PID 24956 live-exercised this path. The log contains 90 stationary/jitter, 963
free, 10 blocked and 95 slide/partial samples; it also captured the native
`1.65 -> 0.95 -> 1.65 m` crouch sequence, 414 free and 25 slide/partial samples
after recovery, camera application after standing and mirror-on gameplay frames.
The final helper failure only lacked another blocked sample after standing; that
requirement was redundant with the already captured standing-shape block and is
now replaced by meaningful post-recovery physical movement.

PID 21548 confirmed that in-place head tilt no longer causes character
locomotion after the carry correction. The new blocker is continuous world
shake, with aggressive rejection near walls. The old reported stick
addition/cancellation described interaction with that unintended
motion, not an acceptable deliberate room-scale step. Static analysis found a
concrete contributor: Black Plague's single tick reported total accepted
movement including the prior physical request; after that request had been
reconciled, the shadow passed the same physical component into
`CarryHeadAnchorWithLocomotion`. Rework performs a
physical body update first and carries the anchor only with a second stick body
update. The corrected shadow receives the matched physical accepted vector,
subtracts X/Z from locomotion carry, keeps the real whole-tick `body_after` for
camera offset, and logs `locomotion_carry` separately. Log analysis then found
that horizontal presentation was still held at the 60 Hz body cadence and
relative to the native smoothed camera, unlike Rework's 90 Hz VR anchor. The
render-rate anchor continuation described above addresses that evidenced
difference and adds `room_scale_reconciled_offset_m`,
`room_scale_render_prediction_m` and `room_scale_head_anchor_m` telemetry. The
next headset run, PID 13672, exercised that correction. The user reported that
the continuous world shake was gone and the overall sensation had improved
substantially. The helper observed stationary, free and slide/partial but ended
with a blocked false negative despite the user having performed the wall-block
case. The classifier was using total accepted-vector magnitude; Rework `23c890f`
uses accepted displacement projected onto the request direction. The helper now
uses that same directional rule, so lateral native solver correction cannot mask
a direct block. Gameplay collision behavior is unchanged.

PID 11804 headset-exercised the tracking-only heading correction. The reported
direction now followed current HMD heading before recenter, after a physical
turn and after recenter, while the general presentation remained stable. The
run captured only stationary/free classes, so its helper failure for missing
blocked and slide/partial is valid evidence incompleteness rather than a shutdown
failure. The user also identified that visual forward retained the speed of the
underlying native axis: movement was slow when the corrected vector mapped to
native backward and normal when it mapped to the old body forward.

Static comparison explains the distinction. Black Plague's
`MoveForward/MoveSideways -> iCharacterBody::Move` path owns signed per-axis
acceleration and caps; rotating a world vector into those axes fixes heading but
cannot produce isotropic Rework speed. The transient room-scale path now builds
the movement vector directly from the current tracked head world pose using the
shared `HeadRelativeMoveDirection` and `LocomotionDisplacement` policy, then
queues `1.5 m/s` normal or `2.25 m/s` sprint displacement through the already
owned `0xD7281` pre-collision injection. PID 17612 disproved the first attempt to
use native player byte `+0x264` as the final movement-permission confirmation:
direct VR axes were deliberately zeroed before the native calls, and exact
decoding shows `amount == 0` returns before the later `+0x264` write. The
corrected path evaluates the same two native state gates and `+0x268/+0x26C`
condition that precede that zero check, without entering native acceleration;
keyboard/native axes still take priority. `+0x264` is mirrored only after the
direct metric request has been successfully queued.

Directly copying Rework's two sequential body updates is unsafe here because the
binary backend has one live-tested native `D6E00` owner per tick. The adaptation
merges physical tracking and stick displacement into that one request, preserves
physical reconciliation priority, bounds the combined X/Z step to `0.05 m`, and
partitions the accepted result into physical and locomotion telemetry/carry.
PID 28996 exposed and PID 8092 validated the corrected indexed-state mapping
described above. Directional stick locomotion and the combined room-scale
technical gate now have headset evidence. The current tree closes the missing
VR-footstep path at host level. Rework `23c890f` measures horizontal body travel
from its last VR footstep anchor and, after `>0.85 m`, calls native
`FootStep(0.8f)`. Shared `runtime::VrFootstepCadence` preserves that vector-anchor
cadence while Black Plague feeds it only the physical/direct-VR displacement
accepted by the sole native collision tick. The body observer now only queues a
cadence event; the existing `cButtonHandler::Update` owner dispatches native
`cPlayer::FootStep` after the original update returns, keeping surface/material
work outside the `D6E00` body-update call stack. Pending dispatch is bound to the
observed player/body generation and exports cadence/attempt/success/reject/ABI
telemetry. The exact-build adapter reproduces a native caller's MSVC 2003
empty-string lifecycle through ctor IAT `0x2721E0` and dtor IAT `0x2721D8`; the
modern Framework CRT never constructs that string. The cadence anchor is kept in
double precision internally so repeated small accepted float deltas cannot cross
the strict `>0.85 m` threshold through summation error alone.
Release, CTest `35/35` and the exact-image verifier pass. This remains
**host-tested only**; cadence/surface sound plus native bob/body animation still
need headset observation.

Turn ownership was re-audited against Rework rather than redesigned. Rework
`ButtonHandler::UpdateVRTurn` adds snap/smooth turn to tracking world yaw, and
current Black Plague `NativeInputBridge::HookedUpdate` already calls
`AddTrackedWorldYaw(-turn)` before native Update. The earlier statement that BP
applied VR turn to native player yaw was stale. Keep directed headset validation
open, but do not rework this owner without contradictory evidence. PID 20520 then exposed the failed
edge-only physical-crouch ownership and remaining short-range X/Z discomfort
described in the repository checkpoint. PID 23260 confirmed that physical
policy exit and button latching were correct but the native toggle release did
not restore standing. PID 24948 then showed that release/press compensation
could restore the collider while repeatedly toggling the native state and never
holding the real crouch/stealth behavior. The current build applies the desired
state through exact `ChangeMoveState(4/0)` instead. PID 25484 then captured
three physical entry/exit cycles, final native standing/ownership release,
stable button-only state-4 crouch, `0.961 m` tracked render-Y range, direct
locomotion and positive subjective comfort. It also captured the Hybrid
combined state, but not the subsequent `physical=0 + latch=1 + state 4`
release-hold interval. The next focused posture test should therefore be short
and targeted at that interval plus blocked-stand/low-ceiling recovery rather
than replaying the entire PID 25484 batch.

Two presentation regressions from the earlier headset session remain separate.
PID 19192 refined the mirror-off evidence: gameplay frames reported
`monitor_mirror=0`, no monitor world pass and one suppressed world pass, while
native 2D menus remained visible because they do not call `RenderWorld`. This
matches the intended pass boundary. After Alt+Tab/focus loss, main menu/inventory
can still render black; that issue is not patched. The
current tracked-menu capture still depends on the desktop framebuffer/focus and
needs a better evidenced ownership boundary before changing behavior.

## Camera/bob/footsteps

`D790C -> D5F00` and `D7913 -> D6120` are gravity-disabled synchronization calls, not the general active-player camera composition path. Do not reuse them as a camera boundary.

The VR footstep cadence is no longer unmapped: Rework's `>0.85 m` horizontal
body-anchor rule is shared and Black Plague consumes collision-accepted VR
movement, queues the cadence result after collision resolution, then calls native
`FootStep(0.8)` through the exact legacy ABI from the existing game-thread input
owner after native `cButtonHandler::Update` returns. The probe exposes dispatch
attempt/success/reject/ABI counters for the next headset run.
This deliberately does not drive or suppress `cPlayerHeadMove::Update` at
`0xA59C0`, so native bob/body-animation presentation remains a separate comfort
observation. Do not reopen body ownership or invent a second cadence to address
that presentation gate.

## Hands/fingers

Black Plague provides the better game-neutral articulation semantics: five independent curls, per-joint curves, spread and thumb opposition. Rework's game-neutral controller conditioning has now been extracted into `runtime::vr_hand_pose`: the measured `0.08` skeletal deadzone, grip/trigger fallback closing windows and ~70 ms smoothing are host-tested shared policy.

Overture consumes the complete shared conditioning path while keeping its rig-specific bind poses, bone axes, hand-chain order, handle geometry and forced-grab presentation local. Black Plague applies the shared skeletal deadzone/smoothing once per native input update before its richer articulation output. Its Framework input frame does not yet expose normalized grip/trigger analogs, so the existing non-skeletal digital visual fallback remains unchanged rather than synthesizing missing analog data.

Current boundary:

```text
shared input conditioning -> shared articulation output
        ↓
per-game rig/profile adapter
```

Black Plague's rig/profile adapter now consumes the Rework right/left DAE hand
geometry through generated renderer data while preserving the richer shared
articulation. The host path deduplicates 18,102 authored triangle corners to
3,376 position/UV draw vertices plus uint16 indices per hand. Its client-array
draw explicitly isolates/restores HPL1 array and element VBO bindings; the WGL
gate reproduces both non-zero bindings before drawing. This remains host-tested
until the combined headset gate checks scale, orientation, articulation and
frame pacing.

Release validation is host-only: the full autonomous Overture Release regression passes, including 289 `VRTrackingTest` checks. The current root suite has 34 tests. Do not mark this extraction live/headset validated until controller hardware is exercised. Do not degrade Black Plague articulation to match Overture merely because Overture is the historical reference.

## Interaction priorities after body reconciliation

The game-neutral Rework palm collision policy (dimensions, contact skin,
sweep/refinement and recovery thresholds/predicates) is now Framework-owned in
`vr_interaction_policy.hpp`; Overture consumes that exact policy through its
legacy `VRHandCollisionPolicy` namespace. Black Plague now has the exact native
query/callback boundary and PID 28412 live-tested its no-write diagnostic. The
backend-owned palm shape lifecycle and the game-neutral Rework resolver are
live-tested in PID 8644 behind `--validate-palm-resolver`; the gate creates,
reuses and destroys the shape without publishing a resolved gameplay hand pose.
The initialized exact-build image now also pins the two exclusion branches used
by Rework: `collideCharacter=false` skips every body whose character byte is set,
while the fourth argument independently skips exactly one `skip_body`. The host
resolver harness verifies Black Plague calls that ABI with character collision
disabled and the supplied skip body. The gameplay integration now publishes the
current held body per hand and feeds the resolved grip to visible hands,
held-object motion and tools. Rework `23c890f` was rechecked after the reported
pickup difficulty: target acquisition deliberately follows the raw controller a
bounded `0.18 m` beyond the collision-stopped hand. Black Plague now mirrors
that rule for selection/revalidation while keeping the resolved palm
authoritative once the body is owned.
Supported-image work also separated action-state `Move=2` from `Grab=6`: free
Move bodies retain the native picked contact and follow the resolved palm using
Rework's physical-force behavior, while jointed/mechanism bodies remain native.
The exact-image verifier and synthetic interaction tests pin that ownership. The
gameplay path remains host-tested pending a clean headset run.

Rework's semantic haptic profiles are also Framework-owned in
`vr_haptics.hpp`, including strength clamping/scaling and per-event cooldowns.
Overture consumes them through its existing `cVRHaptics` API. Black Plague's
pickup/drop feedback uses the same proven profiles; tracked UI selection now
uses `UISelect` on the resolved pointer hand, and successful direct hand nudges
use `Interaction` with bounded contact strength. The backend keeps actual
OpenVR submission target-specific and rejects unfocused, disconnected or
invalid-pose requests while applying the shared per-event cooldown. `LightToggle`
is now consumed at the existing native button-update owner: BP snapshots the
exact flashlight/glowstick active bytes before and after native update and emits
the shared event to the off hand only when native state actually changes.
`Damage` is also consumed through one exact-build owner covering all ten direct
`cPlayer::Damage (0x9BB80)` callsites; the native method remains authoritative,
feedback is emitted to both hands only when `cPlayer+0x310` health decreases,
and strength uses BP's observed easy/normal/hard pre-application scaling before
the shared Rework profile. `MeleeImpact` is now consumed at the demonstrated
native post-contact boundaries inside Black Plague's melee attack: enemy
`cGameEntity::Damage` at callsite `0x603D5`, plus `HitBody (0x5F000)` at
`0x60747/0x608CF` after their native collision gates. Framework calls each
native owner first and emits the shared Rework event to the configured dominant
hand; generic-body feedback mirrors the native helper's entity-type `7` enemy
exclusion so it cannot duplicate the enemy path. The exact-image verifier pins
all three sites and the exclusion, and the native-input contract harness checks
dominant-hand/body-eligibility policy. LightToggle, Damage and MeleeImpact are
host-tested only. None of this completes the comfort/haptics milestone or
constitutes headset validation.

Black Plague now also records per-event haptic attempts, successful OpenVR
submissions, policy rejections, backend submission failures and left/right
submission counts. `Start-BlackPlaguePalmCollisionValidation.ps1` consumes this
telemetry together with spatial diagnostics. The same headset run can therefore
collect evidence for tracked UI select, pickup/drop, direct hand nudge,
`LightToggle`, `Damage`, confirmed-contact `MeleeImpact`, `Grab=6`, opportunistic `Move=2`, flashlight/glowstick
attachment and the full tracked-crouch entry/exit cycle while preserving the
required room-scale/palm composition. `Grab=6`, crouch and palm/room-scale
telemetry are hard gates; Move/tool/low-ceiling and individual haptic events are
explicit optional coverage so an unavailable scene object cannot produce a
false regression.

Stable panel anchoring and transient overlay ownership handoff are now also
Framework-owned in `vr_panel_policy.hpp`. Black Plague's tracked menu consumes
the stable-anchor lifetime plan, while Overture's radio/subtitle path consumes
the overlay handoff. Placement transforms, renderer calls and game menu state
remain backend/product-specific. The new policy is host-tested only.

The common tool/attachment composition is Framework-owned in `vr_grab_pose.*`.
Black Plague retains its measured flashlight socket, while the glowstick now
uses Rework's exact `VrScale=1.55`, `VrGripPoint=(0,0.0078,-0.078)` and X
rotation `4.71` because its primary grip-cylinder geometry was verified
identical. The model transform is composed onto the resolved hand together with
Rework's authored long-finger attachment pose. This is host-tested; definitive
placement and light direction still require headset evidence.

The shared spatial-audio reference contains the complete accepted Overture
mine-gallery EFX parameter set, including echo, modulation and room-rolloff
fields. Black Plague now consumes Rework `23c890f` HRTF startup semantics at a
safe launcher-owned boundary: before a fresh Steam launch Framework writes the
exact shared `alsoft.ini` text beside the game executable, while an already
running process is accepted only when that startup file already matches the
saved HRTF mode. The focused file/config harness, BP settings capability test
and shared spatial-audio test pass; this is **host-tested only** and still needs
real OpenAL/headset evidence before promotion. Static analysis of
`black-plague-22000-live.bin` confirms a native OpenAL/EFX environment stack and
the environmental setup around RVA `0x15C267`, but its existing route attaches
the effect with environment volume `1.0` rather than Rework's added
mine-gallery parameters plus `0.32` bus trim. The Rework-only `0.45`
distance-HF factor also has no audio use in the BP image. Do not classify native
BP EFX as full parity: occlusion/reverb still require a demonstrated narrow
runtime boundary.

1. run `tools/Start-BlackPlaguePalmCollisionValidation.ps1` after a clean reboot;
   do not interpret any hand wall/table result until the probe log explicitly
   reports `palm_collision enabled=1 source=mutex` (or another enabled source);
   verify normal FPS, both controllers and that short physical X/Z movement plus
   crouch/stand still feel like the PID 25484 room-scale baseline before and
   after palm contact;
2. verify palms stop on a static wall/table and slide tangentially; then hold
   grip on a nearby prop and confirm acquisition no longer loses the object to
   the nudge path. The imported Rework hands must keep their diffuse texture and
   remain on the resolved palms. Record visible finger quality separately from
   the already-valid five curl channels;
3. with no stick input, apply gentle physical pressure into a wall for several
   seconds and confirm the PID 14212 forceful pullback is gone. There must be no
   repeated vertical mini-jumps; `physical_step_suppressed` should appear. Walk
   physically over a representative beam/low obstacle without jumping, then use
   stick locomotion over an ordinary stair/ledge to prove native step behavior
   is still retained there;
4. verify several small/free props that previously floated, long wooden bars or
   tables, and one jointed mechanism across the separate `Grab=6` / `Move=2`
   ownership paths;
5. with the glowstick available, verify that it sits inside the long-finger grip
   cylinder instead of cutting through the palm; flashlight coverage remains
   optional when the current save does not provide one;
6. headset-check the host-mapped inventory/notebook tracked-menu route, then map
   HUD/subtitle presentation and any note-specific UX still outside the native
   notebook surface;
7. comfort/haptics and representative chapter-level validation.

Long bars and mechanisms must not be fixed by arbitrary springs or rigid palm offsets. Map native joint/slider/hinge state.

The remaining Rework VR code was re-audited host-only on 2026-09-16. Two more
pure policies are now Framework-owned and consumed by Overture:
`vr_magnetic_pickup_policy.hpp` carries the item-only range/bias, cone/scoring,
top-five visibility-sample and sight-overshoot rules; `vr_mechanism_policy.hpp`
carries the free/slider/hinge servo math and motion caps. Overture still owns its
item enum, portal/physics queries, native joint selection and entity/mass-specific
hinge lightness. The isolated interaction-policy test, `check-project.ps1`, the
full Overture Release rebuild/unit gate (`289` checks) and Framework CTest
(`35/35`) pass. These policies are **host-tested** only.

The host-only frontier has advanced at both interaction boundaries. Exact-image
analysis now pins the real Black Plague `cGameItem`: loader call `3554E ->
35040`, entity type `5` at `+0xC0`, native ItemType conversion `34AE0` stored at
`+0x250`, and native `IsInView=35140` with the 43-degree cone, `SkipRayCheck`
`+0x27C`, item ray callback `+0x280` and PhysicsWorld `CastRay` slot `+0x68`.
Do not revive the earlier `item/pickup` icon-parser shortcut or classify entity
type `6` as an item. The BP-owned broad enumeration is now implemented and
**host-tested** through the existing normal-player `HookedRay` owner. It walks
the exact `PhysicsWorld +0x14` body list (`node +0x08` payload), filters body and
entity state through the pinned exact-build fields, classifies Item subtypes
0-11 into the three demonstrated shared profiles, and leaves BP-only gasmask 12
and collectable 13 unsupported. It ranks at most five candidates through
`vr_magnetic_pickup_policy`, then requires solid LOS from both controller aim
and HMD using the existing `CastRay` boundary and the exact embedded
`cBoundingVolume +0xB4` getters. Magnetic selection is a fallback only after the
bounded physical/palm route finds nothing; its winner is published through the
same native callback and is not exposed to nearby `interaction_assist`.
`Test-BlackPlagueInputMap.ps1` pins the body-list, body/entity filter and BV ABI,
the Release build passes and Framework CTest remains `35/35`. This is not live
or headset evidence; validate inventory pickup behavior in a later headset run.

The mechanism boundary now consumes two exact one-joint Black Plague families.
`cGameLever` remains verifier-pinned at loader `3D70C -> 3D500`, size `0x324`,
vtable `0x676608`, type `0x12` and `Update=3C2B0`; it accepts the demonstrated
hinge/slider forms. Exact Rework `23c890f` also shows `cGameSwingDoor`
explicitly entering `Move`, setting `PauseControllers/PauseGravity=true` and
treating every authored door joint as a hinge. The BP image independently pins
its `0x2EC` allocation/constructor, vtable `0x6791F0`, type `8`, both lifecycle
flags and its base joint collection, so the adapter now accepts only the
one-joint hinge form. A slider-shaped SwingDoor fails closed. The concrete
Newton hinge/slider type, pin `+0xB8`, pivot `+0xC4` and body velocity setters
remain backend-owned and verifier-pinned. Native `Move::Enter/Leave` retain
scripts, controller pause/resume, gravity and state transitions; only recognized
held updates consume shared `vr_mechanism_policy`. Wheel (`type 0x13`) remains
native because static analysis shows dedicated joint/state fields and a
substantial entity-specific update. The supported-image verifier pins its vtable
`0x6793E8`, `Update=5ABB0`, base joint vector, dedicated joint `+0x23C` and
state `+0x244/+0x250`, preventing silent type-only generalization. Unknown and
multi-joint mechanisms also remain native. Synthetic coverage now exercises Lever and SwingDoor hinge
consumers plus SwingDoor slider rejection and cap restoration. Release build,
exact-image verification and Framework CTest `36/36` pass. This integration is
**host-tested only** and still requires representative headset evidence.

The presentation frontier is now narrower. Exact-image evidence pins Black
Plague's existing inventory action at `0x5096 -> cPlayer::StartInventory
(0x9D020)` and notebook action at `0x50BE -> cNotebook::SetActive(true)
(0x96540)`. Inventory activation reaches `cInventory::SetActive(true)` at
`0x6C6B0`. Their native active bytes are `inventory+0x5C` and
`notebook+0x44`, exactly the fields already read by `UiContext`; the existing
input owner republishes that context after native `cButtonHandler::Update`, so
an overlay opened on the current tick is visible to rendering immediately. The
tracked-menu path then captures the complete desktop framebuffer, which already
includes those native screens, and presents it through the shared stable-panel
and pointer policy. The exact-build verifier and synthetic native-input harness
now gate this contract. Treat inventory/notebook presentation as
**consumed / host-tested**, not headset-validated.

The gameplay HUD/subtitle presentation owner is now mapped and **host-tested**.
Exact-image evidence pins `cScene::Render`'s native sequence at `0xEE01B ->
cUpdater::OnPostSceneDraw (0xE1C40)`, `0xEE03B -> cGraphics::GetDrawer
(0xDF730)` and the sole queue-consumer callsite `0xEE042 ->
cGraphicsDrawer::DrawAll (0xF3920)`. The updater body calls `iUpdateable` slot
`+0x04`, matching Rework's `OnPostSceneDraw` order, while the BP `DrawAll` ABI
is the original no-argument form that sets ortho and clears its gfx buffer.
This is the material DIFFERENCE from Rework `23c890f`, whose source added
`DrawAll(bool dontClear, bool drawVr)` specifically so each VR eye can preserve
the queue and its matrices.

The narrow BP adaptation keeps HPL generation and queue consumption in that
native owner. Persistent gameplay renders both world eyes first but defers their
OpenVR submit. At the existing `DrawAll` call, Framework redirects the one
native 800x600 draw into a transparent render target, restores the caller FBO,
alpha-composites that texture into both eye targets with Rework's authored
`x=(0..800-400)/450`, `y=-(0..600-250)/450`, `z=-0.75 m` placement, then submits
the stereo pair. Inventory/notebook bypass this path and retain the complete
desktop tracked-menu capture. `gameplay_overlay_frames`, failures, deferred
submits, CPU time and last error are logged for the first live run. The real-WGL
host test proves alpha blending/no-clear plus GL-state restoration; Release,
exact-image verification and root CTest are 35/35. This remains **host-tested**:
do not promote HUD/subtitles until headset evidence shows native content in the
eyes without a presentation regression.

Black Plague `cGameMessage::Draw` is separately mapped at `0x3F380`. Rework
scales its analogous message line height/font size through `GetSubtitleScale()`;
BP still only persists that setting. Keep `SubtitleScale` unwired until a headset
run proves the newly captured gameplay surface actually carries subtitles, then
apply scale at the product-owned message boundary. Dimmer/transient overlays and
staged loading/fade lifecycle remain open. Physical crouch also still needs the
remaining headset/blocked-stand evidence.

The host-only Rework comparison now narrows the dimmer gap. Exact `23c890f`
draw order places `cPlayerVRDimmer` after the world and before inventory/
subtitles, with the demonstrated `0 <-> 0.75` target at `1.75/s`. Black Plague's
tracked inventory/notebook presentation instead captures the complete desktop
UI and projects it over a dark VR background. A literal dimmer port would either
darken the captured panel or duplicate the background, so that part is
classified as not directly applicable to the current BP menu presentation.
Do not drive dimming from broad `UiContext`; gameplay message/transient-overlay
dimming remains open until an exact native active-state owner is mapped.

A Framework-native radial/quick-access menu is now explicitly planned, but it
is sequenced after the current headset/body/palm validation and the immediate
interaction/presentation boundaries above. Do not start it while those gates
remain the active milestone. When reached, implement one optional shared VR UX
policy for hold/open/close, analog sector selection, handedness, cancellation
and selection haptics; keep product-specific actions/items and activation behind
narrow Overture/Black Plague/Requiem adapters, and preserve the original menu/
inventory route as a fallback. The authoritative ordering is recorded in
`docs/TRILOGY_PARITY_PLAN.md` and `ROADMAP.md`.

## Requiem

Requiem remains a future exact-build binary backend. Gameplay implementation becomes the active milestone after Black Plague closes the framework-readiness gate in `docs/TRILOGY_PARITY_PLAN.md`. Exact-build reconnaissance may proceed earlier, but do not assume Black Plague RVAs, layouts, calling conventions or lifecycle boundaries transfer without evidence. Requiem is the third-backend reuse proof: consume the validated shared capability families through narrow target adapters/profiles and repeat target-specific live/headset validation.

## VR settings/menu extraction

The Framework now persists the complete shared `VrSettings` schema through
`vr_settings_store.*`, including settings not yet consumed by Black Plague.
`vr_settings_editor.*` owns the demonstrated Overture Rework 18-row editor
semantics: row order/labels, formatting, edit increments, enum wrapping, clamps,
snap/smooth dependent visibility and crouch-depth label behavior.

Black Plague now has an explicit backend capability map. It marks only the
currently applied editor settings as available: handedness, turn mode and its
snap/smooth/dead-zone controls, move speed/dead-zone, `PlayMode`,
`PlayerHeight`, `HeightOffset`, `CrouchMode`, `PhysicalCrouchDepth`, UI
distance/scale and render scale. `PlayMode`/`PlayerHeight` are backed by the
tracked presentation path through shared `VrPlayModePolicy`; exposing them in
the offline editor is therefore host-tested runtime plumbing, not a claim that
seated-mode comfort has been headset-validated. Monitor mirror remains a
separate setting. HRTF is now a Black Plague functional control because the
launcher applies it before native audio-device creation. Enhanced Visuals now
routes the persistent BP eye pair through the shared Rework-derived RGBA16F +
2x MSAA/final-treatment stage when the host GL feature set supports it, and
falls back to the existing direct targets otherwise. This path is host-tested
only; Rework's HPL ambient/light material response remains open. Subtitle scale
remains persisted but intentionally unwired pending headset proof of the native
subtitle surface.

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

Current root validation count is 36 CTest tests in the full configured suite.
The hosted SDK-less CI still excludes the real-driver `opengl_eye_targets` test;
the local SDK-less configuration below is broader than that hosted gate.
Overture retains its 289 historical `VRTrackingTest` checks plus
shader/visual/texture/LAA gates, now also exercised by the dedicated
`Overture Release regression` CI job. The latest complete local offline result
on 2026-09-17 passed all **36/36** Release root CTest tests, including the
real-driver `opengl_eye_targets` test. The SDK-less Release configuration had
already passed **35/35** before the newest host-only test coverage was added.
Metadata validation passed with 6
catalogue entries, 2 exact-build manifests, 42 actions, 6 action sets and 8
controller bindings; the initialized Black Plague exact-image verifier also
passed the Grab/Move and palm-query boundaries. `Build-OvertureProduct.ps1
-Configuration Release -Full` passed its project, 16-shader, 8,752
visual-reference, 231-texture decode, Large Address Aware and `VRTrackingTest`
gates; the latter reported **289 checks, 0 failures**. These were host-side
checks only: no new headset validation is implied.

The current Black Plague Enhanced Visuals checkpoint is likewise host-only.
Exact Rework `23c890f` source establishes a separable per-eye pipeline:
RGBA16F scene target, 2x MSAA, resolve, then bounded 5-tap sharpen and v4 final
tone/saturation/contrast/gamma treatment. Framework now owns that transferable
OpenGL stage in `src/graphics/opengl_enhanced_eye_stage.*` and BP consumes it
behind the existing `enhanced_visuals` setting. A real-driver WGL test renders a
known linear sample through the stage and matches the shared CPU calibration
reference within byte tolerance. The Release suite is 36/36, metadata and the
BP exact-image verifier pass. Rework's separate `Ambient_Hemisphere`/VR light
material programs are not yet consumed by the original BP HPL renderer, so do
not promote the whole Enhanced Visuals family beyond partial host-tested
consumption.

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
