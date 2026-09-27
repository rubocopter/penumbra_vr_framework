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

## Deferred validation backlog — Black Plague release regression

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

## Requiem — active Hito 2

- [x] Map exact renderer, player/body, input, UI and native movement boundaries
  from Requiem's initialized executable; consume common runtime through its
  own adapter.
- [x] Headset exercise boot/menu/gameplay, stereo head tracking, Sense-left
  movement, gameplay mirror, physical/stick locomotion, wall collision, crouch,
  visible hands, inventory and notebook in the reported routes.
- [x] Host-test Requiem-native Grab/Move hooks, shared hand/contact mechanics,
  snap continuity, tool sockets and world UI. These are not yet validated as
  usable interaction in the headset.
- [x] RQ-13 runtime: candidate rays found nearby entities but native Grab/Move
  entries stayed zero; stone/note pickup remained hard, combined text/diary
  overlay was too large. Do not retune grab forces from this result.
- [x] RQ-14 host: surface contact anchor conditional on native Grab, overlay
  reframe, post-R2 state trace and native accepted-body/type/range telemetry.
- [x] Port the proven BP palm ownership path into Requiem: exact-build palm box,
  collision-resolved palm, palm-overlap acquisition with native eligibility,
  held-body exclusion, fresh-generation refresh, shared yaw epoch and surface
  anchor for both Grab and Move. Release build and 44/44 host tests pass.
- [ ] One interaction-only headset session: stone plus diary/item pickup from a
  surface/edge/floor, short stick movement while held, one snap turn, release/
  throw, and one representative door/drawer/lever when available. Confirm hand
  blocking/sliding and stable attachment; collect the log on failed acquisition
  or propulsion. Do not expand the headset pass until this gate is usable.
- [ ] Port proven Rework/BP item-only magnetic pickup after exact Requiem
  body-list, bounding-volume and item-subtype boundaries are verified. Keep
  solid hand/head sight and native eligibility; do not extend props/mechanisms.
- [ ] Validate representative interaction, transitions/loading, pacing,
  tool alignment and shutdown before claiming support. Preserve Overture/BP
  regression gates for Hito 3.

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
