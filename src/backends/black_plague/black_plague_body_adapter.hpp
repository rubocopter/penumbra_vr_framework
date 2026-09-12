#pragma once

#include "black_plague_body_callbacks.hpp"
#include "vr_locomotion.hpp"
#include "body_reconciliation_shadow.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

// Exact-build boundary for the native Black Plague character body.  It only
// forwards horizontal intent through cPlayer::MoveForward/MoveSideways and
// observes the result around the game's existing D460A -> D6E00 tick.  It
// never invokes iCharacterBody::Update itself.
struct BlackPlagueBodyMotion {
    bool valid = false;
    bool intent_published = false;
    std::uint64_t native_tick_sequence = 0;
    std::array<float, 2> native_horizontal_intent{};
    std::array<float, 3> feet_after{};
    runtime::VrAcceptedBodyMotion accepted{};
};

[[nodiscard]] bool InstallBlackPlagueBodyAdapter(std::string& error) noexcept;
[[nodiscard]] bool RemoveBlackPlagueBodyAdapter(std::string& error) noexcept;

[[nodiscard]] BlackPlagueBodyMotion ConsumeBlackPlagueBodyMotion() noexcept;

// Optional diagnostics, enabled only by PVR_BP_RECONCILIATION_SHADOW=1 at
// adapter installation. The renderer publishes raw tracking; the existing
// native body callback consumes it. Neither path writes player/camera state.
void PublishBlackPlagueShadowTracking(const runtime::VrMatrix34& pose,
    float world_yaw, bool recentered) noexcept;
void InvalidateBlackPlagueShadowTracking() noexcept;
struct BlackPlagueShadowTelemetry {
    std::uint64_t observed_ticks = 0;
    std::uint64_t resets = 0;
    BodyReconciliationShadowSample latest{};
};
[[nodiscard]] BlackPlagueShadowTelemetry ConsumeBlackPlagueShadowTelemetry() noexcept;

// Installation-time state for the default-off shadow diagnostic. This is
// intentionally observation-only: callers can prove how the one-shot request
// was received without changing body or camera ownership.
enum class BlackPlagueShadowRequestSource : std::uint8_t {
    disabled,
    environment,
    mutex,
    physical_validation,
};
struct BlackPlagueShadowStatus {
    bool enabled = false;
    BlackPlagueShadowRequestSource source = BlackPlagueShadowRequestSource::disabled;
};
[[nodiscard]] BlackPlagueShadowStatus ReadBlackPlagueShadowStatus() noexcept;

enum class BlackPlaguePhysicalValidationRequestSource : std::uint8_t {
    disabled,
    environment,
    mutex,
};

enum class BlackPlaguePhysicalValidationResult : std::uint8_t {
    none,
    queued,
    reconciled,
    invalidated,
    queue_failed,
};

struct BlackPlaguePhysicalValidationStatus {
    bool enabled = false;
    BlackPlaguePhysicalValidationRequestSource source =
        BlackPlaguePhysicalValidationRequestSource::disabled;
};

struct BlackPlaguePhysicalValidationTelemetry {
    std::uint64_t queued_plans = 0;
    std::uint64_t matched_observations = 0;
    std::uint64_t invalidated_plans = 0;
    std::uint64_t queue_failures = 0;
    bool pending = false;
    BlackPlaguePhysicalValidationResult latest_result =
        BlackPlaguePhysicalValidationResult::none;
    std::uintptr_t expected_character_body = 0;
    std::uint64_t expected_generation = 0;
    std::array<float, 3> requested_displacement{};
    runtime::VrAcceptedBodyMotion physical_motion{};
    runtime::VrPhysicalReconciliationResult reconciliation{};
};

[[nodiscard]] BlackPlaguePhysicalValidationStatus
ReadBlackPlaguePhysicalValidationStatus() noexcept;
[[nodiscard]] BlackPlaguePhysicalValidationTelemetry
ConsumeBlackPlaguePhysicalValidationTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
