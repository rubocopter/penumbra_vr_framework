# Spanish localization payloads

This directory stores the Spanish translation overlays that the unified
installer will deploy for the binary Penumbra games. Paths below the per-game
directory mirror their destination below the game's installation root.

| Game | Repository payload | Installer destination |
|---|---|---|
| Black Plague | `black_plague/redist/config/Espanol.lang` | `redist/config/Espanol.lang` |
| Requiem | `requiem/redist/expansion01/config/Espanol_exp.lang` | `redist/expansion01/config/Espanol_exp.lang` |

`manifest.json` records the exact payload hashes and destinations. The metadata
gate verifies those hashes so accidental translation changes are detected.

The original `leeme.txt` supplied with each translation is retained verbatim
next to its game payload. Black Plague credits Oscar `darkpadawan` Rodríguez / Clan
DLAN for the improved translation, based on the official Spanish translation
adapted by DeM for later digital releases. Requiem identifies DeM's translation
as the underlying work and describes the supplied version as a continuity
adaptation for the improved trilogy translations.

These files are deployment inputs only. The production installer still has to
copy them through the same backup, verification and rollback transaction as
other owned files. Overture keeps its Spanish language file in
`products/overture/data/config/Espanol.lang` and packages it through its existing
source-product overlay.

When Clan DLAN was dismantled, its translation archive was released so the
community could host and redistribute the translations freely. These copies may
therefore be packaged with the Framework installer; keep each original
`leeme.txt` with the project and preserve its author/community attribution. This
records redistribution permission and does not assign a new license to the
translation text.
