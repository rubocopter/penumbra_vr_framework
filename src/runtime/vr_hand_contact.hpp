#pragma once

#include "vr_math.hpp"

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

struct VrHandResolverFrame {
    VrHandContactVector head{};
    VrHandContactVector right{1.0F, 0.0F, 0.0F};
    VrHandContactVector up{0.0F, 1.0F, 0.0F};
    VrHandContactVector forward{0.0F, 0.0F, 1.0F};
    bool head_basis_valid = false;
    bool left_hand = false;
    bool interaction_assist = false;
    VrHandContactVector interaction_target{};
    bool interaction_target_valid = false;
};

struct VrHandResolveState {
    VrMatrix44 raw_pose{};
    VrMatrix44 resolved_pose{};
    bool valid = false;
    int constrained_frames = 0;
    // Diagnostic result of the latest resolver sample. These flags do not
    // participate in collision policy; backends can use them to distinguish a
    // normal slide from Rework's deliberate recovery/reanchor paths.
    bool last_tracking_reanchor = false;
    bool last_recovery_anchor = false;
    bool last_pullback_recovery = false;
    bool last_interaction_assist = false;
};

using VrHandCollisionQuery = bool(*)(
    void* context,
    const VrMatrix44& pose,
    const VrHandContactVector& motion,
    float tolerance,
    VrHandCollisionDecision& decision) noexcept;

void ResetVrHandResolveState(VrHandResolveState& state) noexcept;

// Game-neutral port of Rework 23c890f's per-hand collision resolver. Native
// shape ownership, exact-build collision calls and body exclusions stay in the
// backend-supplied query. A failed/malformed query never advances to raw input.
[[nodiscard]] bool ResolveVrHandPose(
    VrHandResolveState& state,
    const VrMatrix44& raw_pose,
    const VrHandResolverFrame& frame,
    void* query_context,
    VrHandCollisionQuery query,
    VrMatrix44& resolved_pose) noexcept;

} // namespace penumbra_vr::runtime
