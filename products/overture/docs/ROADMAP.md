# Overture — historical roadmap and maintenance boundary

This is the Framework-owned Overture product. The historical v0.1.0
standalone release and its experiments remain documented in
[RELEASE-v0.1.0.md](RELEASE-v0.1.0.md),
[VALIDATION-v0.1.0.md](VALIDATION-v0.1.0.md) and
[RELEASES.md](RELEASES.md). Technical research stays in
[LIGHTING.md](LIGHTING.md), [TEXTURES.md](TEXTURES.md) and
[INPUT_ARCHITECTURE.md](INPUT_ARCHITECTURE.md).

## Current state

Overture ships inside the Penumbra VR Framework. The intended trilogy feature
set is complete; only scoped fixes for reproducible blocking failures remain
active. The [Framework roadmap](../../../docs/ROADMAP.md) owns current
maintenance policy, [trilogy parity](../../../docs/TRILOGY_PARITY_PLAN.md)
owns per-game evidence and
[supported builds](../../../docs/SUPPORTED_BUILDS.md) owns exact releases.

The older proposals for OpenXR, two-hand interactions, hand-model replacement,
ambient effects, new audio features and universal controller certification
are historical exploration, not planned development.

Retain the accepted visuals/texture baseline and credits. Reopen profiling,
rendering or input only for a concrete regression with an exact device/build
reproduction. Host tests do not imply new headset acceptance.
