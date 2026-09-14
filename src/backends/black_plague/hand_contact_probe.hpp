#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

enum class HandContactQueryResult : std::uint32_t {
    none = 0,
    clear = 1,
    collided = 2,
    invalid_boundary = 3,
    invalid_contact_data = 4,
    native_memory_changed = 5,
};

struct HandContactQueryTelemetry {
    HandContactQueryResult result = HandContactQueryResult::none;
    bool native_collided = false;
    bool callback_invoked = false;
    bool native_memory_changed = false;
    std::uint32_t callback_count = 0;
    std::uint32_t contact_count = 0;
    std::int32_t shape_type = -1;
    std::int32_t shape_user_count = 0;
    std::array<float, 3> shape_size{};
    std::array<float, 3> requested_position{};
    std::array<float, 3> corrected_position{};
    float maximum_contact_depth = 0.0F;
};

// Requests one exact-build CheckShapeWorldCollision call on the existing
// character-body update owner. The query reuses the current native body shape,
// writes only to stack/DLL-owned storage and verifies selected world/body/shape
// bytes did not change. It creates no shape and is never used by gameplay.
[[nodiscard]] bool RequestNoWriteHandContactQuery(
    HandContactQueryTelemetry& telemetry,
    std::string& error) noexcept;

[[nodiscard]] bool NoWriteHandContactQueryPending() noexcept;

// Called after the sole native D460A -> D6E00 update, on its game thread.
// This is fan-out from the existing owner, not another hook or body update.
void ServiceNoWriteHandContactQuery(
    std::uint8_t* image,
    void* character_body) noexcept;

// Exact MSVC7 cCollideData view proved from the supported image. Exposed only
// so the host harness can lock pointer/count/point-stride handling.
[[nodiscard]] bool AccumulateLegacyHandContacts(
    const void* collide_data,
    std::uint32_t& contact_count,
    float& maximum_depth) noexcept;

} // namespace penumbra_vr::backends::black_plague
