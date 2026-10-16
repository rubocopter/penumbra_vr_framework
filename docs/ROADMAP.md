# Roadmap

This file owns the current work order. Capability state lives in
`TRILOGY_PARITY_PLAN.md`, exact-build identity and validation levels in
`SUPPORTED_BUILDS.md`, and stable ownership rules in `ARCHITECTURE.md` and
`DESIGN_DECISIONS.md`.

## Current priority — post-v1.0 validation and maintenance

Framework [v1.0.8](releases/1.0.8.md) packages focused BP prop/mechanism,
computer and Examine-text corrections. The maintainer accepts the reported
barrel/lever/bucket, PC reach/cursor/Accept and centered enlarged text cases;
exact local hashes and scope are in `releases/1.0.8-headset-acceptance.json`.
Prepare and verify the updated installer, matching committed source, checksums
and build report before publication. Exact new-installer headset and independent
clean-PC acceptance remain separate from publication. Historical trilogy
acceptance stays scoped to `releases/1.0.7-headset-acceptance.json`.
Use `CLOSURE_STATUS.md` for release provenance and the remaining deployment matrix.
Index finger feedback, Quest 3 pointer/cadence coverage, broader per-game
regression and Requiem startup reliability remain focused follow-up.
The Windows CI also runs synthetic installer configuration, prerequisites,
release identity, UI state and diagnostics/privacy checks without game binaries
or VR hardware; these checks do not establish clean-PC or headset acceptance.

The runtime milestones below retain their evidence but non-blocking polish,
broader parity and optional improvements do not displace this release task.
Reopen runtime work only for a concrete release blocker, including the documented
intermittent Requiem startup crash. Do not automatically launch games.

The newly reported BP dream-ship progression blocker is current focused runtime
work; see `TRILOGY_PARITY_PLAN.md` for scoped headset acceptance of arrival
message, kitchen save and consistent local Examine cues. Reporter saves are
available. Yellow-bucket acquisition by handle/container and lifting received
local headset acceptance after the internal dynamic Grab assembly adaptation.
Bucket throwing/reacquisition and correct-table egg/fire/dog appearance are
accepted. The articulated Grab candidate and the 36-entry human-ragdoll guard
correction are host-tested. The maintainer now confirms dog lift/altar placement;
initial acquisition remains irregular. The world-rooted arm puzzle now has
maintainer completion evidence. Focused host-tested corrections address barrel
release impulses, wall-lever COM velocity and bucket-handle separation; the
2026-10-16 maintainer report accepts those corrections. The new focused candidate
has maintainer acceptance for Swanson's fixed-computer activation at distance.
The ScreenGui cursor/Accept correction has subsequent maintainer acceptance.
The requested centered 2x Examine-message text now has maintainer acceptance.
Equipped-light menu visibility and flashlight socket confirmation remain
headset retests. Retest these,
other free/anchored Grab props and doors/chests/lockers
per `TRILOGY_PARITY_PLAN.md`.
Physical-only attribution correction did not close the
reported reach/low-obstacle blocking; investigate step eligibility and continued
world rejection against Rework. Quest 3 coverage remains an independent gate.

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

The representative interaction milestone is concluded at the user's request.
The final native-size, forward-facing flashlight has headset confirmation for
beam/model alignment. Broader coverage and known limitations remain regression
work; this is not full-game support.

The representative Push puzzle gate is closed: in headset the cube could be
pushed to the switch, tipped and used to solve the puzzle. Runtime telemetry
confirmed VR Push acquisition and applied hand force. This validates that Push
sequence only.

Representative headset evidence:

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
- [x] Validate tool/hand motion coherence: a new headset clip shows flashlight,
  glowstick and flare staying with the visible hand during stick locomotion.
  Its log records resolved-palm attachment throughout, with no raw-palm use.
- [x] Validate final flashlight orientation and beam/model alignment at native
  size. The user accepted the result and supplied a confirming headset image.

Known limitations carried into later regression:

- Requiem inventory-only magnetic pickup still needs exact body-list, bounds
  and item-subtype evidence; do not extend it to props or mechanisms.
- Slight finger/contact imperfections, monolith ring resistance and accidental
  stacked-block displacement remain visible. The climbing interval used
  `Grab=6` and jumps, with no VR Push acquisition/force.
- [ ] Classify Requiem's intermittent Steam Play startup failure. Nine
  captured dumps fault in `SDL_mutexP` from the same Requiem return site
  `0x5A9DA3` with varying invalid mutex arguments. A captured failure occurred
  after tracked menu submission; delaying OpenVR until two completed SDL swaps
  did not prevent it. The upstream cause remains unknown. Resume investigation
  only with new discriminating evidence; do not hook the mutex speculatively.

## Hito 3 — trilogy regression

After the Requiem interaction gate, run a small production-path regression for
all three games. Preserve the validation ladder; old headset evidence does not
automatically validate a newer candidate.

- [x] Audit all eight bundled controller graphs and their consumers in the
  three games. Host guards reject dropped profiles/actions and type drift;
  package/install/repair fixtures verify all eight by hash. Device hardware
  validation and the documented Vive/WMR layout limitations remain separate.
- [ ] Confirm the current update on the external Index setup when
  feedback is available. The maintainer has accepted local Overture gameplay
  and completed the BP tutorial without performance issues; those observations
  do not establish Index acceptance of the new installer. Preserve the deferred,
  non-blocking finger report and avoid requesting redundant gameplay tests.

The requested BP/Requiem feedback retest is closed by maintainer acceptance.
Do not request it again as a release prerequisite. The new BP excessive-blocking
report is a separate mixed stick/HMD eligibility gate; host regression passes,
and the maintainer accepted the changed behavior in the 2026-10-10 installed
trilogy checklist. Do not request that focused gate again without new evidence.

### Overture

- [x] Rebuild the Framework-owned product and complete the host regression
  against the current shared runtime. The Release build and 51-test auxiliary
  suite pass with the Overture interaction-sight and Touch aim-binding guards.
- [ ] Repeat the core headset/controller regression on Quest 3, including Touch
  pointer posture/alignment and crouched reach to the outside hatch wheel.
- [ ] On Quest 3 with Quest Link at 72 Hz, verify smooth-turn and locomotion
  cadence against the OpenVR-selected logic rate, and confirm objects no longer
  disappear only at the left-eye outer edge during head movement.
- [x] Accept the installed Overture checklist for menus, locomotion/turning,
  edge visibility, crouched hatch/door reach and input/focus recovery. Broader
  progression, transitions and shutdown coverage remain follow-up.

### Black Plague

- [x] Accept focus/Alt+Tab recovery, mixed stick/HMD obstacle contact and
  wall/step/crouch regression on the installed local checklist.
- [ ] Extend continuous presentation, mirror and map-transition coverage.
- [ ] Extend tunnel/low-ceiling pressure, Hybrid blocked-stand recovery and
  accepted-motion footsteps coverage. These distinct cases do not reopen the
  accepted physical-contact mini-hop or mixed-obstacle gates.
- [x] Accept free-body acquisition/hold/throw and snap-turn continuity on the
  installed local checklist. Broader slider/hinge/door coverage remains open.
  A fresh, bounded surface
  contact now anchors VR-origin Grab as it does in the headset-tested Requiem
  path; BP's stale/native-only origin fallback remains. A chair was grabbed
  successfully from different points in headset. Broader mechanisms remain
  separate regression cases. The latest candidate enables
  the vice's physical hinge through native Move and prevents fixed vice/locker
  frames from winning over their dynamic bodies; both have failing-before-fix
  host regressions and have maintainer acceptance of the focused device retest.
- [ ] Validate imported-hand fingers/material presentation, tool sockets and
  mapped haptics.
- [x] Accept inventory/notebook/context actions, `UseItem` and the shared
  native-menu extraction in English and Spanish on the installed local checklist.
  First-level settings and height calibration are accepted too.
- [ ] Validate subtitle scaling/legibility and broader UI hardware coverage.
- [ ] Keep the tutorial small-box reach report as a device-specific diagnostic:
  repeated PSVR2 grabs did not reproduce it. Do not retune reach, collision,
  body offsets or movement constants without a deterministic failing headset case.
- [ ] Validate HRTF/reverb on headset audio hardware. Keep the missing
  distance/occlusion low-pass consumer as implementation work until its exact
  target boundary is mapped.
- [x] Validate a representative BP special-effects scene after the port. The
  exact initialized BP image independently maps its two renderer screen-copy
  sites and the installed shaders sample in eye pixels. BP now consumes the
  full-eye copy proven in Requiem; native call/copy/resize counters and real
  OpenGL tests protect the port. The user reports that the observed effects
  now render correctly, and the matching log records full-eye copy/resize
  activity. This does not establish coverage of every smoke/material variant
  or subtle ghosting.
- [ ] Reproduce death → main menu only as part of normal regression; the SDL
  mutex crash remains historical/unattributed unless a new discriminating dump
  appears.

The repeatable Black Plague hardware procedure is in
`VR_HEADSET_TEST_CHECKLIST.md`.

### Requiem

- [x] Maintainer accepted intro height without the inventory workaround,
  physical/button crouch, death/continue/main-menu pointer, uphill slope stability
  and persisted turn/crouch settings on the feedback candidate.
- [x] Confirm the new first-level English/Spanish native VR menu, live setting
  refresh, calibration and persistence on the installed local headset checklist.
- [x] Accept installed BP/Requiem input/focus recovery and retained stick
  response; host regressions separately cover binding loss and invalid data.
- [x] Record successful Requiem startup checklist feedback. Its intermittent
  crash remains unresolved until discriminating evidence identifies the cause.

## Hito 4 — unified installer

The user requested this work alongside the remaining runtime gates. The
shared Black Plague/Requiem redist transaction, combined selector and graphical
front end are shipped in v1.0.0. Broader runtime regression and hardware breadth
remain separate evidence tracks.

- [x] Host-test the shared Black Plague/Requiem deployment transaction,
  three-game selector and Overture ownership/upgrade integration.
- [x] Host-test read-only discovery and exact-build fingerprinting before writes.
- [x] Host-test transactional install, repair, recovery and exact restore for
  Overture and the shared root, including the verified BP LAA transform.
- [x] Package and verify all eight controller defaults for each root and both
  Spanish localizations under the shared transaction.
- [x] Finish production versioning/rollback, dependency verification and optional
  recommended settings while preserving user calibration and custom bindings.
- [x] Complete the release provenance/source-notice audit and maintainer
  install/update/repair/uninstall acceptance. Independent clean-PC breadth remains
  tracked separately.
- [x] Provide and host-test the graphical launcher with a hidden PowerShell
  console, exact-build detection, manual folders, install, repair and uninstall.
  Packaged discovery includes pristine and managed Overture and the shared BP/
  Requiem root. The published EXE has maintainer real-install acceptance.

## Hito 5 — release

- [x] Register the 2026-10-10 installed trilogy acceptance with exact hashes.
- [x] Build and publish v1.0.7 from committed source: 59 Framework tests,
  298 Overture checks, final bilingual Setup smoke, A-F topology and upgrade
  from 1.0.6. Exact artifact/build identities are recorded in the validation
  record; games were not launched automatically.

- [x] Prepare v1.0.6 from clean committed source; verify 59 Framework tests,
  298 Overture checks, final-EXE English/Spanish smoke, topology A-F and update
  from v1.0.5. Publish the same tested artifacts with hashes and provenance.

- [x] Run final automated release verification for the published artifact/tree.
- [x] Produce the versioned v1.0.0 installer/source/checksum/report artifact set
  from a clean committed checkout and promote the tested EXE unchanged.
- [x] Update the landing page, known issues and support table to match the
  evidence actually obtained.
- [x] Publish `v1.0.0` and the matching release artifacts.
- [ ] Extend independent clean-PC, headset and non-Sense controller coverage
  after release without retroactively inflating v1.0 evidence.

Exit criterion: one package safely handles any supported combination of the
three games and restores original installations exactly.
