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
    runtime::VrAcceptedBodyMotion physical_motion{};
    runtime::VrPhysicalReconciliationResult physical_reconciliation{};
    std::array<float, 3> physical_delta{};
    std::array<float, 3> locomotion_carry_displacement{};
    std::array<float, 3> predicted_anchor{};
    std::array<float, 3> native_anchor_correction{};
    float separation = 0.0F;
};

struct BodyReconciliationTickPlan {
    bool valid = false;
    bool reset = false;
    std::uint64_t sequence = 0;
    std::uint64_t body_generation = 0;
    runtime::VrTrackingSampleIdentity tracking_identity{};
    runtime::VrMatrix34 tracking_pose{};
    std::array<float, 3> body_before{};
    std::array<float, 3> head_tracking_position{};
    std::array<float, 3> physical_delta{};
    runtime::VrBodyReconciliationPlan reconciliation{};
};

class BodyReconciliationShadow final {
public:
    void Reset() noexcept;
    [[nodiscard]] BodyReconciliationTickPlan PrepareTick(
        const runtime::VrMatrix34& head_tracking_pose,
        float tracking_world_yaw,
        std::uint64_t body_generation,
        const std::array<float, 3>& body_before,
        float delta_seconds,
        runtime::VrTrackingSampleIdentity tracking_identity = {}) noexcept;
    [[nodiscard]] BodyReconciliationShadowSample CompleteTick(
        const BodyReconciliationTickPlan& tick,
        const runtime::VrAcceptedBodyMotion& native_motion,
        const std::array<float, 3>& feet_after,
        bool physical_observation_available = false,
        const runtime::VrAcceptedBodyMotion& physical_motion = {}) noexcept;
private:
    bool initialized_ = false;
    bool tick_pending_ = false;
    std::uint64_t generation_ = 0;
    std::uint64_t next_sequence_ = 0;
    std::uint64_t pending_sequence_ = 0;
    std::array<float, 3> previous_head_{};
    std::array<float, 3> previous_body_{};
    std::array<float, 3> anchor_{};
};

} // namespace penumbra_vr::backends::black_plague
