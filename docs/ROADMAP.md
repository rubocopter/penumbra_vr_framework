# Roadmap

This file tracks the remaining project work. Completed reverse-engineering and
session chronology are intentionally omitted; durable capability state lives in
`docs/TRILOGY_PARITY_PLAN.md`, exact-build evidence in manifests and validation
levels in `docs/SUPPORTED_BUILDS.md`.

## Foundation already in place

- [x] Framework-owned Overture source/build/package host independent of the
  Rework checkout.
- [x] Shared OpenVR session, tracking, settings, logical input, locomotion,
  accepted-motion, hand/contact, interaction, haptic and render-target policy.
- [x] Shared action manifest with eight controller binding graphs, including
  PS VR2 Sense.
- [x] Exact-build catalogue and manifests for the current Black Plague and
  Requiem Steam executables, including verified LAA transformed fingerprints.
- [x] Black Plague tracked stereo, rotational tracking and adaptive per-eye
  targets on the allowlisted build.
- [x] Black Plague native input bridge, room-scale/body boundary, direct metric
  locomotion, crouch/tracked Y and accepted-motion reconciliation.
- [x] Black Plague rendered hands, skeletal finger input, palm collision policy,
  free-body interaction path, haptics and mapped native mechanism boundaries.
- [x] Black Plague native `VR Settings` page and shared settings persistence.
- [x] Black Plague HRTF startup plus exact-build OpenAL/EFX reverb/bus-trim
  consumer.
- [x] Managed Black Plague normal-Steam bootstrap; ordinary Steam **Play** has
  reached gameplay VR on the allowlisted retail build.
- [x] Framework-owned localization payloads for Black Plague and Requiem.
- [x] Deployment payload metadata and maintainer-recommended settings metadata
  established as future unified-installer inputs.

## Deferred validation backlog — Black Plague framework readiness

The closure execution has delimited the remaining Black Plague items that need
headset/runtime evidence. `CLOSURE_STATUS.md` owns that decision. These items
remain release gates, but they do not displace the active Requiem integration
milestone while no new discriminating offline Black Plague work exists.

### Presentation and tracking

- [x] Audit Overture's optional render diagnostics: the legacy `PrintLog`
  command enabled synchronous per-draw `hpl.log` flushes. It is now gated off
  while VR is enabled; normal desktop diagnostics remain available. Compile-time
  update timing is disabled in the Release build. Carry the same invariant into
  Requiem as its renderer/backend instrumentation is implemented.
- [ ] Capture a full stack and eliminate the recurring Black Plague SDL mutex
  lifetime crash (`SDL_mutexP`/`SDL_DestroyMutex`, retail SDL RVAs `0x28C09`
  and `0x28BD6`). The existing stable-window and completed-swap bootstrap gates
  reduce the startup race but do not eliminate it.
- [ ] Headset-revalidate continuous presentation after the current per-loop pose
  acquisition fix; no alternating stale-pose/native frames.
- [ ] Validate map/door transitions after authored spawn-yaw compensation.
- [ ] Diagnose the messhall `_bb_blue_lightray_halo` family at its exact BP
  billboard/beam render boundary. The latest headset clip still shows rays
  crossing the view after eye-derived native camera position was published with
  each eye's view/projection matrices and the nested `UpdateRenderList` camera
  composition was corrected. The 2026-09-24 headset clip disproves that as a
  complete fix: both blue axis billboards and red laser beams still glitch.
  Probe the exact native transparent draw for per-eye camera/model-matrix and
  GL blend/depth state before changing the renderer. These effects are distinct
  from the particle refresh path.
- [ ] Validate per-eye particle refresh on camera-facing effects such as tunnel
  vapor. Probe telemetry now exposes `particle_updates`,
  `particle_eye_refreshes` and `particle_refresh_misses` so a remaining artifact
  can be classified before changing another render family.
- [ ] Complete focus/Alt+Tab and monitor-mirror regression coverage.

### Body, locomotion and comfort

- [ ] Revalidate wall/tunnel pressure with the current Rework-equivalent
  projection/clamp rule for accepted physical motion.
- [ ] Revalidate mixed stick + room-scale contact against low/dynamic props while
  retaining native pure-stick stair/ledge stepping.
- [ ] Headset-revalidate Hybrid release-hold and blocked-stand/low-ceiling
  recovery after the host-tested delayed native-feedback fix.
- [ ] Validate VR footstep cadence/surface selection and record any remaining
  native head-bob/body-animation comfort issue separately.
- [ ] Exercise seated/standing, explicit recenter and tracking-loss recovery.

### Hands and interaction

- [ ] Revalidate all five finger channels on the imported Rework hand mesh.
- [ ] Restore a lit, non-emissive presentation for the imported hand mesh. The
  Overture reference material is lit with zero emission; the current Black
  Plague compatibility draw uses the diffuse texture without that material
  response and has appeared white/fullbright in headset.
- [ ] Revalidate palm blocking/sliding and yaw-turn continuity with production
  palm collision enabled.
- [ ] Confirm reliable natural acquisition and stable `Grab=6` / free-body
  `Move=2` placement without proximity launching or stale contact ownership;
  the current host-tested candidate rejects negative native ray distances
  observed corrupting winner selection in live logs and keeps held `Grab=6`
  bodies active/enabled/non-autodisable as Rework does. Recheck after the
  host-tested guard that exits failed VR-origin acquisitions instead of letting
  native mouse-relative Grab/Move take over. The latest host-tested BP
  `Grab=6` candidate accepts a fresh VR winner within shared interaction reach
  and always anchors the body's local origin in the palm, independent of hit
  point; compare repeated contacts in headset. Verify that free-body `Grab=6`
  and `Move=2` stay held across snap turns, and that jointed mechanisms do not
  receive a force/servo impulse on the yaw epoch change.
- [ ] Headset-check recovery from a frozen controller action sample. The latest
  log held the same nonzero move axis and both finger-curl summaries for 33
  seconds; grip matrices were not logged. The host-tested 1.5-second guard
  releases actions if both tracked grip poses also remain bit-identical. The
  next probe reports unchanged-sample age and guard activation.
- [ ] Validate recognized sliders, hinges, swing doors, drawers and other mapped
  native mechanisms; keep unknown joint families fail-closed. The latest
  host-tested Move acquisition accepts a fresh selected contact within 0.40 m;
  check the electricity-room lever, door hinge behavior and reported player
  repulsion in the headset before adjusting joint servo or collision policy.
- [ ] Validate final flashlight/glowstick geometry, sockets and held-tool pose.
- [ ] Validate mapped LightToggle, Damage and real-contact MeleeImpact haptics in
  headset/controller hardware.

### UI, audio and visuals

- [ ] Headset-validate inventory/notebook, including drag, default-use,
  contextual item actions and the controller-aim `UseItem` red/green beam needed
  for progression. The context-action candidate now bypasses only BP's verified
  top-level right-button discard and forwards button `2` to the existing native
  context/widget route. The latest host-tested UI candidate keeps stereo world
  rendering under a transparent native inventory/notebook draw queue, places
  inventory at Rework's fixed world-panel scale and notebook at the off hand,
  and projects the pointer onto those same panels. The next candidate rotates
  the notebook as Rework does, shows the hand, and retains the panel pose on
  its first closing frame to prevent the inventory flash. Check the reported
  yellow context square and native action text with the new panel. Recheck
  door/environment messages on Rework's centered plane at UI distance;
  `SubtitleScale` remains adjustable and headset legibility is unverified.
- [ ] Headset-validate the native `VR Settings` page, persistence and
  restart-required labels. Enhanced Visuals is hidden and disabled for BP while
  its uncalibrated lighting stage produces dark/saturated headset output;
  the other 17 consumed Rework settings remain exposed.
- [ ] Reproduce death → main menu after the null-character-body native crouch
  guard; the 2026-09-24 dump located the dereference in native MoveState OnEnter.
- [ ] Headset/audio-device validate HRTF and environmental reverb/bus trim.
- [ ] Map a safe target boundary for the remaining distance/occlusion low-pass
  behavior.
- [ ] Complete the Black Plague side of Enhanced Visuals before final headset
  validation. The final eye pass already matches Rework v4 (`1.25` exposure,
  `1.12` saturation, `1.08` contrast and `0.94` gamma), but headset testing is
  currently too dark and saturated because Black Plague does not yet reproduce
  the corresponding pre-tone material/ambient/light preparation. Preserve the
  shared final calibration until the target-specific input signal is corrected;
  keep renderer-specific HPL behavior backend-owned unless reuse is proven.
- [ ] Validate representative chapter progression and loading transitions.
- [ ] Exercise available controller families and record per-device gaps without
  inferring support from JSON bindings alone.

## Requiem

**Current milestone.** RQ-01 established live stereo gameplay presentation,
RQ-02 headset-validated the HMD visibility/culling boundary, and RQ-03
headset-validated boot/menu presentation plus handoff into gameplay. RQ-04
confirmed basic Sense movement and the gameplay monitor mirror. RQ-05 confirmed
native crouch transitions but exposed a physical-crouch camera jump. The
first Rework/BP tracked-height correction was tested in RQ-06: it still lowered
the view too far because it used the capsule centre. RQ-07 reports the feet
anchor now appeared correct, while lateral-speed perception and a late
cadence drop remain unresolved. Room-scale body motion and native button
actions were host/static-tested before RQ-08; its result and RQ-09 gate are below.

- [x] Map Requiem renderer and initial player/body movement boundaries from its
  own exact executable evidence.
- [x] Map the Requiem `cButtonHandler::Update`, player access, movement
  permission/state and native body-tick ownership required for VR locomotion.
- [x] Consume shared controller-input and locomotion capability families through Requiem-specific
  adapters/profiles.
- [x] Implement and headset-validate tracked stereo plus boot/menu presentation
  without copying Black Plague RVAs/layouts.
- [x] Implement and host-test VR input and HMD-relative body locomotion through Requiem's own
  exact boundaries, reusing the proven Black Plague/shared policy where valid.
- [x] RQ-04: validate basic Sense-left movement and gameplay monitor mirror in
  the headset/desktop. A separate HMD-versus-body heading check remains.
- [x] Map and host-test native Requiem crouch through its own move-state
  boundary and the shared physical/button policy.
- [x] RQ-05: capture native crouch/stand transitions and identify the camera
  height defect; speed and HMD heading reports remain inconclusive.
- [x] RQ-06: confirm over-deep physical crouch, stable lateral-versus-forward
  speed perception independent of crouch, and late capture-frame slowdown.
- [x] RQ-07: the user reports physical crouch seemed correct; stick-speed
  asymmetry remains subjective. The last five-second log block fell to 55.6
  world FPS while render duration stayed at 5.9 ms; cause remains open.
- [x] Host-test Requiem physical room-scale through its own body update and
  collision owner; map eleven native gameplay button queries for Sense.
- [x] RQ-08: room-scale/wall, mixed stick motion and crouch worked in the
  reported headset session; R1 opened invisible native UI and blocked movement,
  R2 had no observed interaction, and jump felt short. No persistent new pacing
  loss or crash was reported.
- [x] Prepare one RQ-09 probe: exact Requiem UI queries/pointer, deferred native
  panel presentation, recenter and controller-anchored Rework hand visuals.
- [x] RQ-09: the headset session showed visible hands, usable menu/inventory/
  notebook and working physical crouch; R2 arrived at the native query but
  could not acquire an object. Tools floated and world render time rose sharply.
- [x] Map Requiem's exact native selection ray, Grab and Move states, free-body
  methods and hinge/slider joint families; host-test the shared hand grab,
  mechanism policy, snap-held continuity, tool sockets and gameplay 2D overlay.
- [x] RQ-10: headset trial reached R2 and refreshed selection but entered no
  Grab/Move state. Flare floated, flashlight slid, and standing with inventory
  open left crouch active. No severe late pacing loss recurred in this session.
- [x] Prepare one RQ-11 probe: BP-ranked palm rays with selection diagnostics,
  Rework tool profiles including flare, spatial inventory/notebook and stance
  updates during UI. Stage a reversible Requiem-only jump-force trial.
- [ ] RQ-11: discriminate contact/selection/native entry on one free object;
  then test hold, release, snap while held, one mechanism, tool attachment,
  inventory stance, spatial panels and one known platform jump. Use the same
  log for pacing; stop if interaction still cannot be acquired.
- [ ] Complete representative headset validation while keeping Overture and
  Black Plague regression gates green.

## Unified installer and release

- [ ] Discover Steam installations plus manual folders and fingerprint exact
  builds before writes.
- [ ] Implement transactional backup, install, verify, repair and rollback.
- [ ] Apply the verified LAA transform only inside that known-build transaction.
- [ ] Consume `assets/deployment/manifest.json` for product payload ownership.
- [ ] Deploy/register OpenVR actions and bindings transactionally.
- [ ] Deploy/restore Black Plague and Requiem Spanish localization payloads.
- [ ] Consume `assets/settings/recommended.json` as an optional reversible preset
  without overwriting personal calibration silently.
- [ ] Package licenses/attribution and validate clean install, upgrade, repair and
  uninstall on all supported products.

Exit criterion: one package safely handles any supported combination of the
three games and restores original installations exactly.
