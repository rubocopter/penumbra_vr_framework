# Roadmap — maintenance

As of 2026-10-26, the intended Penumbra trilogy VR feature set is complete.
This is a **maintenance-only** project. There is no active feature backlog,
controller-family expansion, visual-polish programme or universal certification
commitment. The latest public release is [v1.0.12](releases/1.0.12.md)
(2026-10-27), with automated host evidence only. The developer has not tested
this version in-game or in a VR headset.

Consider changes for reproducible failures of installation, launch, essential
input, saving/loading or progression, or destructive repair/uninstall.
Investigate a known but unattributed crash only with new evidence that
distinguishes its cause. Do not launch games as a routine step.

## Current maintenance gates

These are **reported failures or pending acceptance**, not promises of new
functionality or proof that the candidate already works in headset.

| Report | Current evidence (2026-10-27) | Condition for closing |
| --- | --- | --- |
| Requiem level04 sewer: crouching underwater traps player on bottom | **Unresolved.** v1.0.11 logs physical capsule height; water/crouch cause not proven. | Actual stand/jump recovery at orb and battery with cause evidence. |
| Requiem level05 generator, hatch and door buttons | **v1.0.12, host-tested** adds the omitted Rework head-to-hand native area query and consistent-origin short-ray arbitration to v1.0.11 selection. | Repeated activations on reported level, with obstructions, in a pinned headset build. |
| Requiem lever displacement, trembling and perceived reversed direction | **v1.0.12, host-tested** consumes BP's now-shared hinge COM correction; fixture reproduced pivot drift. No global sign inversion demonstrated. | Attached pivot, stable travel and intended direction on the reported mechanisms. |
| Requiem filing cabinets | **v1.0.11, host-tested** suspends native sticky slider endpoints during owned VR Move and restores them on release. | Open/close/regrab at both endpoints without breaking scripts, native constraints or release. |
| Requiem level03 bottle throw and Examine/ladder cues | Published in v1.0.10 with host regressions. | Observed puzzle completion and cue/aiming usability; further code changes are not implied. |
| Requiem level07 cogwheel sphere / B aiming | **v1.0.12, host-tested:** Push used Enter's camera heading; guarded cancellation called launch. Free Move aiming and stale Grab fallback also corrected. | Predictable holding-controller launch before/after ladder travel and sphere entering the goal; physical lip stoppage remains unattributed. |
| Requiem intermittent startup crash | Captured SDL mutex failures but no identified cause or fix. | New discriminating evidence, verified correction and scoped live validation. |

Level04 water, level05 buttons and sliders are **independent acceptance gates**.
Host tests do not establish Quest 3 behavior. Publishing v1.0.12 does not close
these in-game/headset gates. Follow the
[focused procedure](VR_HEADSET_TEST_CHECKLIST.md#trilogy-feedback-candidate--independent-acceptance)
and the [Requiem evidence](TRILOGY_PARITY_PLAN.md#requiem-level0405-progression-feedback--2026-10-25).

Other limitations (finger/contact presentation, mechanism comfort, non-Sense
hardware and broader progression) are non-committed follow-up. Consider a
specific reproducible blocker on its merits.

## Release condition and documentation ownership

A maintenance release requires bounded demonstrated fixes and clean versioned
source/installer verification. The maintainer authorized v1.0.12 publication
with all current corrections and an explicit statement that the developer has
not tested this version in-game or in a VR headset. New
game/device acceptance remains open and must be stated in the notes; publication
does not imply it. Preserve validation states separately: planned, implemented,
host-tested, live-tested, headset-validated, supported. The water and SDL-crash
investigations remain unresolved.

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
[CHANGELOG.md](CHANGELOG.md), [versioned release notes](releases/1.0.12.md),
the [original installer acceptance](releases/1.0.0-installer-ui-candidate.md)
and the [Rework porting contract](REWORK_PORTING_PLAN.md).
Old work checklists remain in Git history and are not active milestones.
