#pragma once
#include <cstdint>
#include <string>
namespace penumbra_vr::backends::black_plague {
struct MovementOwnershipTelemetry {
 std::uint64_t sequence=0, jump=0, jump_hold=0, sprint_start=0, sprint_stop=0, crouch_pressed=0, crouch_release_or_not_held=0, gravity_disabled_camera_sync=0, gravity_disabled_entity_sync=0;
 std::uintptr_t player=0, body=0, camera=0; bool valid=false;
};
[[nodiscard]] bool InstallMovementOwnershipProbe(std::string&) noexcept;
[[nodiscard]] bool RemoveMovementOwnershipProbe(std::string&) noexcept;
[[nodiscard]] MovementOwnershipTelemetry ConsumeMovementOwnershipTelemetry() noexcept;
}
