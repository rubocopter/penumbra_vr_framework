# Roadmap

This file owns the current work order. Capability state lives in
`TRILOGY_PARITY_PLAN.md`, exact-build identity and validation levels in
`SUPPORTED_BUILDS.md`, and stable ownership rules in `ARCHITECTURE.md` and
`DESIGN_DECISIONS.md`.

## Foundation already in place

- [x] Framework-owned Overture source/build/package product independent of the
  Rework checkout.
- [x] Shared OpenVR session, tracking, settings, logical input, locomotion,
  accepted-motion, hand/contact, interaction, haptic and render-target policy.
- [x] Shared action manifest with eight controller binding graphs, including
  PS VR2 Sense.
- [x] Exact-build catalogues/manifests for the active Black Plague and Requiem
  Steam executables, including verified LAA transformed fingerprints.
- [x] Black Plague tracked stereo, native input, room-scale/body integration,
  crouch, hands, physical interaction, tracked UI, VR settings and audio paths.
- [x] Normal Steam **Play** bootstrap has reached Black Plague gameplay VR on
  the allowlisted build.
- [x] Requiem exact-build backend reaches boot, menu and gameplay VR and consumes
  shared tracking, input, locomotion, crouch, hands, UI and interaction policy.
- [x] Framework-owned Black Plague/Requiem localization, deployment metadata and
  recommended-settings metadata exist as installer inputs.

## Hito 2 — Requiem interaction

The representative Push puzzle gate is closed: in headset the cube could be
pushed to the switch, tipped and used to solve the puzzle. Runtime telemetry
confirmed VR Push acquisition and applied hand force. This validates that Push
sequence only.

Current work is deliberately narrow:

- [x] Validate one representative free-body Grab sequence: a small stone block
  was acquired repeatedly from different contact points, carried with stick
  movement and one snap turn, then released without visible instability. Its
  matching level-01 asset declares `CanBeThrown=False`; throwing a throwable
  prop and the distinct free-body `Move=2` path remain unvalidated.
- [x] Exercise a representative native mechanism: the level-01 monolith's
  jointed `Move=2` rings were acquired, turned and released through the native
  puzzle. The rings felt somewhat resistant and could spring back; keep that
  comfort issue visible while preserving the script's joint stops.
- [x] Observe the naturally reached transition/loading boundary and pacing:
  the level-01 exit loaded the next area and gameplay returned to about 90 FPS.
  Portal effects showed conspicuous rectangular/striped artifacts.
- [x] Isolate the Requiem portal presentation defect. In one headset session,
  the rectangular/striped artifact appeared with native Refractions enabled
  and disappeared when disabled; the visible ring/scene returned. The
  `portal-fx` warning belongs to untextured map control geometry. Native HPL
  screen copies use 2560×1440 while VR eye targets are 3400×3468.
- [x] Validate the Requiem exact-build eye-sized refraction copy in visor with
  Refractions enabled. The portal ring rendered without the prior rectangular
  and striped artifact; the native copy was upgraded for both eyes, gameplay
  held about 90 FPS, and HPL exited successfully. The clip does not establish
  whether subtle motion ghosting remains in headset.
- [ ] Port Rework/BP magnetic pickup only for inventory items after Requiem's
  exact body-list, bounds and item-subtype boundaries are demonstrated. Do not
  extend that policy to props or mechanisms.
- [x] Validate tool/hand motion coherence: a new headset clip shows flashlight,
  glowstick and flare staying with the visible hand during stick locomotion.
  Its log records resolved-palm attachment throughout, with no raw-palm use.
- [ ] Validate the remaining tool presentation. The same clip shows slight
  finger penetration and the flashlight effect separated from its model.
  The installed BP/Requiem DAE puts the spotlight/ray on model -Y; Requiem's
  borrowed Rework socket turned that axis toward hand +Z. A target-owned
  socket now turns it toward hand -Z, matching the BP measured asset; the
  correction is host-tested only and needs one headset check. Do not change
  the shared installed DAE to mask the socket mismatch.
  One representative level progression is headset-tested. Stacked blocks
  could be displaced accidentally while climbing; telemetry shows `Grab=6`
  and jumps, with no VR Push acquisition or force in that interval.
- [ ] Classify Requiem's intermittent Steam Play startup failure. Nine
  captured dumps fault in `SDL_mutexP` from the same Requiem return site
  `0x5A9DA3` with varying invalid mutex arguments. In the latest attempt,
  the probe completed OpenVR, installed the hooks and submitted the first
  tracked menu frame about 40 seconds before the fault. Delaying OpenVR until
  two completed SDL swaps did not prevent it; the upstream cause remains
  unknown. Do not hook the mutex without exact initialized-image evidence.

## Hito 3 — trilogy regression

After the Requiem interaction gate, run a small production-path regression for
all three games. Preserve the validation ladder; old headset evidence does not
automatically validate a newer candidate.

### Overture

- [ ] Rebuild the Framework-owned product and repeat the core headset/controller
  regression against the current shared runtime.
- [ ] Confirm boot/gameplay, stereo/tracking, locomotion/crouch, hands,
  interaction, UI, transition and shutdown.

### Black Plague

- [ ] Revalidate continuous presentation, focus/Alt+Tab, mirror and map
  transitions on the current candidate.
- [ ] Revalidate wall/tunnel pressure, mixed stick + room-scale contact, Hybrid
  crouch recovery and accepted-motion footsteps.
- [ ] Revalidate free-body acquisition/hold/throw, snap-turn continuity and
  representative slider/hinge/door mechanisms. A fresh, bounded surface
  contact now anchors VR-origin Grab as it does in the headset-tested Requiem
  path; BP's stale/native-only origin fallback remains. The port is host-tested
  and requires BP headset validation.
- [ ] Validate imported-hand fingers/material presentation, tool sockets and
  mapped haptics.
- [ ] Validate inventory/notebook/context actions, `UseItem`, subtitles and the
  native VR Settings page.
- [ ] Validate HRTF/reverb on headset audio hardware. Keep the missing
  distance/occlusion low-pass consumer as implementation work until its exact
  target boundary is mapped.
- [ ] Validate BP refractive particle scenes with Refractions enabled. The
  exact initialized BP image independently maps its two renderer screen-copy
  sites and the installed shaders sample in eye pixels. BP now consumes the
  full-eye copy proven in Requiem; native call/copy/resize counters and real
  OpenGL tests protect this host-tested port. Ordinary smoke/billboard defects
  and subtle ghosting still need their own runtime classification.
- [ ] Reproduce death → main menu only as part of normal regression; the SDL
  mutex crash remains historical/unattributed unless a new discriminating dump
  appears.

The repeatable Black Plague hardware procedure is in
`VR_HEADSET_TEST_CHECKLIST.md`.

## Hito 4 — unified installer

The user requested this work alongside the remaining runtime gates. The
shared Black Plague/Requiem redist transaction, combined selector and graphical
front end have host evidence. The candidate remains a prototype; the current
runtime still needs its own visor validation.

- [x] Host-test the shared Black Plague/Requiem deployment transaction,
  three-game selector and Overture ownership/upgrade integration.
- [ ] Keep discovery and exact-build fingerprinting read-only until a complete
  transaction has been planned.
- [ ] Finish transactional install, repair, recovery, upgrade and uninstall for
  all supported products, including the verified LAA transform where applicable.
- [ ] Register/restore OpenVR assets, localization and optional recommended
  settings transactionally.
- [ ] Package licenses/attribution and validate clean install, upgrade, repair,
  interruption recovery and exact restore.
- [x] Provide and host-test the graphical launcher with a hidden PowerShell
  console, exact-build detection, manual folders, install, repair and uninstall.
  Packaged discovery includes pristine and managed Overture and the shared BP/
  Requiem root. Final user acceptance remains part of the release gate.

## Hito 5 — release

- [ ] Run the final automated suite and the required headset/controller matrix.
- [ ] Produce a reproducible release candidate from a clean checkout.
- [ ] Update the landing page, known issues and support table to match only the
  evidence actually obtained.

Exit criterion: one package safely handles any supported combination of the
three games and restores original installations exactly.
