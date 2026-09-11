#pragma once

#include "vr_locomotion.hpp"
#include "vr_tracking_space.hpp"

#include <cstdint>

namespace penumbra_vr::backends::black_plague {

// Portable consumer of the adapter's observations. No native calls or writes.
struct BodyReconciliationShadowSample {
    bool valid = false;
    bool reset = false;
    bool physical_observation_available = false;
    runtime::VrBodyReconciliationPlan plan{};
    runtime::VrAcceptedBodyMotion native_motion{};
    std::array<float, 3> physical_delta{};
    std::array<float, 3> predicted_anchor{};
    std::array<float, 3> native_anchor_correction{};
    float separation = 0.0F;
};

class BodyReconciliationShadow final {
public:
    void Reset() noexcept;
    [[nodiscard]] BodyReconciliationShadowSample Observe(
        const runtime::VrMatrix34& head_tracking_pose,
        float tracking_world_yaw,
        std::uint64_t body_generation,
        const runtime::VrAcceptedBodyMotion& native_motion,
        const std::array<float, 3>& feet_after,
        float delta_seconds) noexcept;
private:
    bool initialized_ = false;
    std::uint64_t generation_ = 0;
    std::array<float, 3> previous_head_{};
    std::array<float, 3> previous_body_{};
    std::array<float, 3> anchor_{};
};

} // namespace penumbra_vr::backends::black_plague
