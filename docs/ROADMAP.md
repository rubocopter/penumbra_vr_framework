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

## Current milestone — Black Plague framework readiness

Black Plague remains the active target. The milestone closes when every
applicable Overture capability is either consumed, implemented through a
documented target-specific equivalent, classified not applicable, or explicitly
deferred as release tooling.

### Presentation and tracking

- [ ] Capture a full stack and eliminate the recurring Black Plague SDL mutex
  lifetime crash (`SDL_mutexP`/`SDL_DestroyMutex`, retail SDL RVAs `0x28C09`
  and `0x28BD6`). The existing stable-window and completed-swap bootstrap gates
  reduce the startup race but do not eliminate it.
- [ ] Headset-revalidate continuous presentation after the current per-loop pose
  acquisition fix; no alternating stale-pose/native frames.
- [ ] Validate map/door transitions after authored spawn-yaw compensation.
- [ ] Revalidate the messhall `_bb_blue_lightray_halo` family after the current
  host-tested camera transaction began publishing an eye-derived native camera
  position together with each eye's view/projection matrices. These effects are
  billboards/beams rather than evidence of a particle-only regression.
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
- [ ] Close Hybrid crouch release-hold and blocked-stand/low-ceiling recovery.
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
  bodies active/enabled/non-autodisable as Rework does.
- [ ] Validate recognized sliders, hinges, swing doors, drawers and other mapped
  native mechanisms; keep unknown joint families fail-closed.
- [ ] Validate final flashlight/glowstick geometry, sockets and held-tool pose.
- [ ] Validate mapped LightToggle, Damage and real-contact MeleeImpact haptics in
  headset/controller hardware.

### UI, audio and visuals

- [ ] Headset-validate inventory/notebook, including drag, default-use,
  contextual item actions and the controller-aim `UseItem` red/green beam needed
  for progression. The context-action candidate now bypasses only BP's verified
  top-level right-button discard and forwards button `2` to the existing native
  context/widget route. Also validate the captured native HUD/subtitle surface
  before enabling `SubtitleScale` as a Black Plague control.
- [ ] Headset-validate the native `VR Settings` page, persistence and
  restart-required labels.
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

Gameplay work starts after the Black Plague framework-readiness gate closes.
Non-invasive exact-build reconnaissance may continue without displacing the
active milestone.

- [ ] Map Requiem renderer, body, input, UI, interaction and audio boundaries
  from its own exact executable evidence.
- [ ] Consume applicable shared capability families through Requiem-specific
  adapters/profiles.
- [ ] Implement tracked stereo, input, body/interaction and presentation without
  copying Black Plague RVAs/layouts.
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
