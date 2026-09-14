#pragma once

#include <array>

namespace penumbra_vr::runtime {

using VrHandContactVector = std::array<float, 3>;

struct VrHandContactSummary {
    float tolerance = 0.0F;
    float max_depth = 0.0F;
    float best_depth = 0.0F;
    float best_score = -9999.0F;
    VrHandContactVector motion_direction{};
    VrHandContactVector best_normal{};
    bool has_contact = false;
    bool has_blocking_normal = false;
};

// Game-neutral port of Rework 23c890f's cVRHandWorldCollisionCallback.
// Backends feed contact depths/normals from their own physics ABI.
class VrHandContactAccumulator final {
public:
    VrHandContactAccumulator(
        const VrHandContactVector& motion,
        float tolerance) noexcept;

    void Add(float depth, const VrHandContactVector& normal) noexcept;
    [[nodiscard]] const VrHandContactSummary& summary() const noexcept;

private:
    VrHandContactSummary summary_{};
};

struct VrHandCollisionDecision {
    bool blocking = false;
    VrHandContactVector normal{};
    float depth = 0.0F;
};

// Applies Rework's overlap/contact decision after a backend query. The backend
// owns the query and shape lifetime; corrected_position is only inspected.
[[nodiscard]] VrHandCollisionDecision ResolveVrHandCollision(
    bool world_collided,
    const VrHandContactVector& requested_position,
    const VrHandContactVector& corrected_position,
    const VrHandContactSummary& contacts) noexcept;

// Four recovery anchors from Rework's FindVRHandRecoveryPose. The caller keeps
// the orientation and asks its native collision adapter whether each candidate
// is clear, in this order.
[[nodiscard]] std::array<VrHandContactVector, 4> VrHandRecoveryCandidates(
    const VrHandContactVector& head,
    const VrHandContactVector& right,
    const VrHandContactVector& up,
    const VrHandContactVector& forward,
    bool left_hand) noexcept;

} // namespace penumbra_vr::runtime
