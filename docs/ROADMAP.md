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

- [ ] Validate one representative free-body Grab/Move sequence: acquire a
  stone, diary or item from a surface/edge/floor, keep it held during short stick
  movement and one snap turn, then release/throw it.
- [ ] Exercise one representative native mechanism when available (door,
  drawer or lever) and confirm hand blocking/sliding remains stable.
- [ ] If the same route reaches one naturally, observe a transition/loading
  boundary and pacing. Do not broaden the headset pass before the interaction
  gate is usable.
- [ ] Port Rework/BP magnetic pickup only for inventory items after Requiem's
  exact body-list, bounds and item-subtype boundaries are demonstrated. Do not
  extend that policy to props or mechanisms.
- [ ] Validate tool alignment, shutdown and representative progression before
  any support claim.

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
  representative slider/hinge/door mechanisms.
- [ ] Validate imported-hand fingers/material presentation, tool sockets and
  mapped haptics.
- [ ] Validate inventory/notebook/context actions, `UseItem`, subtitles and the
  native VR Settings page.
- [ ] Validate HRTF/reverb on headset audio hardware. Keep the missing
  distance/occlusion low-pass consumer as implementation work until its exact
  target boundary is mapped.
- [ ] Revisit the known billboard/beam and particle scenes. Use existing
  telemetry to classify the failing render family before changing renderer
  policy.
- [ ] Reproduce death → main menu only as part of normal regression; the SDL
  mutex crash remains historical/unattributed unless a new discriminating dump
  appears.

The repeatable Black Plague hardware procedure is in
`VR_HEADSET_TEST_CHECKLIST.md`.

## Hito 4 — unified installer

Runtime work remains ahead of production release tooling. The current two-game
candidate is a host-tested prototype, not a release.

- [ ] Add a verified Requiem deployment transaction and complete Overture
  ownership/upgrade integration.
- [ ] Keep discovery and exact-build fingerprinting read-only until a complete
  transaction has been planned.
- [ ] Finish transactional install, repair, recovery, upgrade and uninstall for
  all supported products, including the verified LAA transform where applicable.
- [ ] Register/restore OpenVR assets, localization and optional recommended
  settings transactionally.
- [ ] Package licenses/attribution and validate clean install, upgrade, repair,
  interruption recovery and exact restore.

## Hito 5 — release

- [ ] Run the final automated suite and the required headset/controller matrix.
- [ ] Produce a reproducible release candidate from a clean checkout.
- [ ] Update the landing page, known issues and support table to match only the
  evidence actually obtained.

Exit criterion: one package safely handles any supported combination of the
three games and restores original installations exactly.