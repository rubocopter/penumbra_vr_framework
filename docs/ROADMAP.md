# Roadmap — maintenance

As of 2026-10-26, the intended Penumbra trilogy VR feature set is complete.
This is a **maintenance-only** project. There is no active feature backlog,
controller-family expansion, visual-polish programme or universal certification
commitment. The latest public maintenance candidate is [v1.0.13](releases/1.0.13.md)
(2026-10-27), with automated host evidence; scoped earlier
tester feedback is tracked below. The developer has not tested
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
| Requiem lever displacement, trembling and perceived reversed direction | **Scoped positive Caelus feedback, 2026-10-27:** sphere throwing and levers now work perfectly; latest version context, no artifact hash supplied. | Broader mechanism coverage remains separate; preserve the observed working owners. |
| Requiem filing cabinets | **v1.0.11, host-tested** suspends native sticky slider endpoints during owned VR Move and restores them on release. | Open/close/regrab at both endpoints without breaking scripts, native constraints or release. |
| Requiem level03 bottle throw and Examine/ladder cues | Published in v1.0.10 with host regressions. | Observed puzzle completion and cue/aiming usability; further code changes are not implied. |
| Requiem level07 cogwheel sphere / B aiming | **Scoped positive Caelus feedback, 2026-10-27:** sphere throwing now works perfectly; no installed hash supplied. | Preserve working launch; no separate evidence establishing the cause of physical lip stoppage. |
| Requiem level08 Steam: orb beyond shack hatch and unusable keypad | **v1.0.13 candidate, host-tested:** bounded native Normal Item pickup with dual visibility; ScreenGui active context and native cGui controller pointer, release/cancel ownership. | Collect `key0801` through the original puzzle route; operate/cancel keypad and return to gameplay without pausing. |
| Requiem level09 power cable | **2026-10-28 source correction:** Caelus reports the next level's cable stays on the floor. Requiem rejected jointed native Grab; the BP connected-body contact route is adapted with host regressions. | Lift both cable ends, attach/detach through native StickAreas and complete the power/bridge sequence in a pinned headset build. |
| Remaining Requiem progression wiring | **Local 1.0.14 candidate:** cable/rope and compatible articulated props, fixed-Lamp activation, inert contact eligibility, script-disabled player ownership and credits Escape are connected with consumer/real-Newton tests. Static callback/StickArea routes traced for levels09–11. | Pinned headset completion of the rope/oil/barrel route, connections, moving bridges and both endings. Water recovery remains independent and unresolved. |
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
source/installer verification. The maintainer authorized v1.0.13 candidate publication after final installer
and package/hash verification, with both level08 corrections explicitly pending
headset validation and no claim that level08 can be completed. The developer has
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
[CHANGELOG.md](CHANGELOG.md), [versioned release notes](releases/1.0.13.md),
the [original installer acceptance](releases/1.0.0-installer-ui-candidate.md)
and the [Rework porting contract](REWORK_PORTING_PLAN.md).
Old work checklists remain in Git history and are not active milestones.
