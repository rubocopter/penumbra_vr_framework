# Controls

This is the Framework quick reference for the bundled right-handed PS VR2 Sense layout. The action maps are included in all three games, but their presence does not establish that every game/action has passed current-release headset regression. The [trilogy ledger](TRILOGY_PARITY_PLAN.md) owns those validation states.

## PS VR2 Sense

| Control | Gameplay action |
| --- | --- |
| Left stick / L3 | Move / sprint |
| Right stick / R3 | Turn / toggle crouch in Button or Hybrid mode |
| R2 | Interact with an object or use the equipped tool/item |
| R1 | Inventory |
| Square | Notebook |
| Cross | Jump when the game allows it |
| L1 | Quick light |
| Triangle | Holster the equipped tool |
| Circle | Examine |
| Options | Pause |
| Create | Recenter horizontal view |

Aim the dominant-hand controller at menus or inventory. R2 selects, R3 supplies the alternate/drag action, and Circle supplies back/context actions. Release a button before pressing it again to close a screen it just opened. Holster an equipped tool before using Interact to target world objects.

Physical movement and crouching use headset tracking. Overture and Black Plague expose turn, handedness, height and crouch choices through their VR settings. Requiem currently uses development defaults; do not assume it has the same settings UI. See [VR configuration](VR_CONFIGURATION.md).

## Other controllers and custom bindings

SteamVR's controller binding interface can inspect and customize the active game profile. Both installation roots include eight default profiles; SteamVR selects one by controller type.

The [controller ledger](TRILOGY_PARITY_PLAN.md#controller-profiles-and-distribution) owns exact coverage and device-validation limits. Vive lacks assigned turn, button crouch, holster and pause; WMR/Holographic layouts lack holster and skeletal outputs. Non-Sense families still need physical-device validation.

The [generated controller reference](CONTROLLER_BINDINGS.md) lists the actual
default gameplay/UI bindings for all eight families and both handedness modes.
It is checked against the distributed graphs. Black Plague provides a shortcut
to the active layout under **Options → VR Settings → Controller bindings
(SteamVR)**. Overture and Requiem use the SteamVR dashboard binding editor.
Overture's v1.0.3 in-game shortcut was removed in v1.0.4 after a pre-menu crash
report. Saved custom bindings are shown by SteamVR and can differ from the
bundled defaults.

Index uses offhand grip squeeze for quick light, leaving the offhand trigger
for interaction. Its left force trackpad recenters; its right force trackpad
pauses. Touch/Pico recenter is deliberately unassigned, avoiding a duplicate
notebook/jump click and a nonexistent face button. WMR's mirrored pause no
longer recenters simultaneously. Vive/WMR retain the compatibility exceptions
listed in the generated reference.

If actions are missing or controllers do not respond, follow [input troubleshooting](TROUBLESHOOTING.md#vr-and-controller-input).
