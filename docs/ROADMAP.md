# Roadmap — maintenance

Core trilogy VR is maintained for reproducible installation, input and progression
failures. The current candidate is [v1.0.15](releases/1.0.15.md), dated 2026-10-29,
including the previously local 1.0.14 work. Neural is an explicitly requested,
optional experimental research path, not a core visual-quality/support promise.
Automated evidence and scoped maintainer reports are recorded separately;
new seated/render-scale/narrative headset acceptance remains open.

Consider changes for reproducible failures of installation, launch, essential
input, saving/loading or progression, or destructive repair/uninstall.
Investigate a known but unattributed crash only with new evidence that
distinguishes its cause. Do not launch games as a routine step.

## Current maintenance gates

These are **reported failures or pending acceptance**, not promises of new
functionality or proof that the candidate already works in headset.

| Report | Current evidence (2026-10-29) | Condition for closing |
| --- | --- | --- |
| Requiem level04 sewer: crouching underwater traps player on bottom | **Unresolved.** v1.0.11 logs physical capsule height; water/crouch cause not proven. | Actual stand/jump recovery at orb and battery with cause evidence. |
| Requiem level05 generator, hatch and door buttons | **v1.0.12, host-tested** adds the omitted Rework head-to-hand native area query and consistent-origin short-ray arbitration to v1.0.11 selection. | Repeated activations on reported level, with obstructions, in a pinned headset build. |
| Requiem lever displacement, trembling and perceived reversed direction | **Scoped positive Caelus feedback, 2026-10-27:** sphere throwing and levers now work perfectly; latest version context, no artifact hash supplied. | Broader mechanism coverage remains separate; preserve the observed working owners. |
| Requiem filing cabinets | **v1.0.11, host-tested** suspends native sticky slider endpoints during owned VR Move and restores them on release. | Open/close/regrab at both endpoints without breaking scripts, native constraints or release. |
| Requiem level03 bottle throw and Examine/ladder cues | Published in v1.0.10 with host regressions. | Observed puzzle completion and cue/aiming usability; further code changes are not implied. |
| Requiem level07 cogwheel sphere / B aiming | **Scoped positive Caelus feedback, 2026-10-27:** sphere throwing now works perfectly; no installed hash supplied. | Preserve working launch; no separate evidence establishing the cause of physical lip stoppage. |
| Requiem level08 Steam: orb beyond shack hatch and unusable keypad | **v1.0.13 candidate, host-tested:** bounded native Normal Item pickup with dual visibility; ScreenGui active context and native cGui controller pointer, release/cancel ownership. | Collect `key0801` through the original puzzle route; operate/cancel keypad and return to gameplay without pausing. |
| Requiem level09 cable and rope | **2026-10-29:** power cable and corrected rope stability/reach have scoped maintainer acceptance. Anchored topology and release remain covered by real-Newton tests. | Actual oil ignition and remaining progression; preserve working cable/rope. |
| Seated reach and render multiplier consistency | **2026-10-29:** seated height baseline ratchet reproduced in all three consumers and corrected in shared policy. Requiem live render resizing rejected by headset feedback; multiplier now restart-applied like Overture/BP. | Headset seated reach without world push; startup multipliers without distortion. |
| Trilogy narrative text and Requiem computer parity | **v1.0.15, host-tested:** centered/enlarged transient messages, narration and native intro/ending text in their mapped consumers; long ending pages fit the HUD. Requiem retains the configurable HUD geometry, terminal pointer and subtitle/UI scale/distance controls carried from 1.0.14. | Pinned headset readability across all three games, terminal reach, focused GUI cursor/Accept and live setting edits. |
| Remaining Requiem progression wiring | **v1.0.15 carries the previously local 1.0.14 work:** cable/rope and compatible articulated props, fixed-Lamp activation, inert contact eligibility, script-disabled player ownership and credits Escape are connected with consumer/real-Newton tests. Static callback/StickArea routes traced for levels09–11. | Pinned headset completion of the rope/oil/barrel route, connections, moving bridges and both endings. Water recovery remains independent and unresolved. |
| Requiem intermittent startup crash | Captured SDL mutex failures but no identified cause or fix. | New discriminating evidence, verified correction and scoped live validation. |

Level04 water, level05 buttons and sliders are **independent acceptance gates**.
Host tests do not establish Quest 3 behavior. Publishing a maintenance candidate does not close
these in-game/headset gates. Follow the
[focused procedure](VR_HEADSET_TEST_CHECKLIST.md#trilogy-feedback-candidate--independent-acceptance)
and the [Requiem evidence](TRILOGY_PARITY_PLAN.md#requiem-level0405-progression-feedback--2026-10-25).

Other limitations (finger/contact presentation, mechanism comfort, non-Sense
hardware and broader progression) are non-committed follow-up. Consider a
specific reproducible blocker on its merits.

## Release condition and documentation ownership

A maintenance release requires bounded demonstrated fixes and clean versioned
source/installer verification. The maintainer authorized v1.0.15 publication
with the current fixes and experimental Neural included in Setup. Publication
does not close headset, full-progression or clean-PC gates. Keep implemented,
host-tested, live-tested, headset-validated and supported states distinct.
Water and SDL-crash investigations remain unresolved.

[TRILOGY_PARITY_PLAN.md](TRILOGY_PARITY_PLAN.md) owns per-game capabilities and
evidence; [SUPPORTED_BUILDS.md](SUPPORTED_BUILDS.md) owns exact builds;
[VR_HEADSET_TEST_CHECKLIST.md](VR_HEADSET_TEST_CHECKLIST.md) owns reproduction
steps; [CLOSURE_STATUS.md](CLOSURE_STATUS.md) owns released-artifact status;
[ARCHITECTURE.md](ARCHITECTURE.md) and
[DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) own architectural constraints.

## Completed milestones and history

The Framework-owned Overture product, shared OpenVR/runtime/input policy, two
binary backends, controller profiles, settings, localization and three-game
transactional installer have shipped. For provenance use
[CHANGELOG.md](CHANGELOG.md), [versioned release notes](releases/1.0.15.md),
the [original installer acceptance](releases/1.0.0-installer-ui-candidate.md)
and the [Rework porting contract](REWORK_PORTING_PLAN.md).
Old work checklists remain in Git history and are not active milestones.
