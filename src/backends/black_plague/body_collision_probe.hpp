#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

struct BodyCollisionTelemetry {
    std::uint64_t character_updates = 0;
    std::uint64_t horizontal_collision_requests = 0;
    bool valid = false;
    bool collision_sample_valid = false;
    std::uintptr_t player = 0;
    std::uintptr_t character_body = 0;
    std::uintptr_t physics_body = 0;
    std::uintptr_t physics_world = 0;
    float delta_seconds = 0.0F;
    std::array<float, 3> character_size{};
    float shape_radius = 0.0F;
    bool native_shape_is_cylinder = false;
    std::array<float, 3> body_position_before{};
    std::array<float, 3> body_position_after{};
    std::array<float, 3> feet_position_before{};
    std::array<float, 3> feet_position_after{};
    std::array<float, 3> requested_displacement{};
    std::array<float, 3> collision_resolved_displacement{};
    std::array<float, 3> accepted_displacement{};
};

struct BodyJumpBurstSample {
    std::uint64_t body_update_sequence = 0;
    BodyCollisionTelemetry body{};
    std::int32_t player_268 = 0;
    std::uint8_t player_26c = 0;
    std::uint8_t jump_button_down_1fc = 0;
    float jump_count_200 = 0.0F;
    float max_jump_count_204 = 0.0F;
    std::int32_t move_state_index_2d0 = -1;
    std::uintptr_t move_state = 0;
};

struct BodyJumpBurstTelemetry {
    static constexpr std::size_t kCapacity = 240; // Four seconds at 60 Hz.
    std::array<BodyJumpBurstSample, kCapacity> samples{};
    std::size_t count = 0;
};

// Exact-build, read-only observation hooks for the initialized FD316F... image.
// They do not enable positional HMD translation or alter native movement.
[[nodiscard]] bool InstallBodyCollisionProbe(std::string& error) noexcept;
[[nodiscard]] bool RemoveBodyCollisionProbe(std::string& error) noexcept;
[[nodiscard]] BodyCollisionTelemetry ConsumeBodyCollisionTelemetry() noexcept;
// Called only after the observed native cPlayer::Jump dispatch has returned.
// It changes no game memory and records the following player-body updates.
void RequestBodyJumpBurst() noexcept;
[[nodiscard]] bool IsBodyJumpBurstComplete() noexcept;
[[nodiscard]] BodyJumpBurstTelemetry ConsumeBodyJumpBurstTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
