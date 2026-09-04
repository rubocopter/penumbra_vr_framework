#pragma once

#include "vr_math.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

struct CameraMatrixSnapshot {
    runtime::VrMatrix44 view;
    runtime::VrMatrix44 projection;
    std::array<std::uint8_t, 3> flags{};
};

[[nodiscard]] bool CaptureCameraMatrices(
    const void* camera,
    CameraMatrixSnapshot& snapshot,
    std::string& error) noexcept;

// Exact-build adapter for the camera layout observed in the whitelisted
// Black Plague executable. An active override always restores its snapshot.
class CameraMatrixOverride final {
public:
    CameraMatrixOverride() noexcept = default;
    ~CameraMatrixOverride() noexcept;
    CameraMatrixOverride(const CameraMatrixOverride&) = delete;
    CameraMatrixOverride& operator=(const CameraMatrixOverride&) = delete;

    [[nodiscard]] bool Apply(
        void* camera,
        const runtime::VrMatrix44& view,
        const runtime::VrMatrix44& projection,
        std::string& error) noexcept;
    [[nodiscard]] bool Restore(std::string& error) noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] const CameraMatrixSnapshot& snapshot() const noexcept;

private:
    void* camera_ = nullptr;
    CameraMatrixSnapshot snapshot_{};
};

[[nodiscard]] bool CameraMatchesSnapshot(
    const void* camera,
    const CameraMatrixSnapshot& snapshot) noexcept;

} // namespace penumbra_vr::backends::black_plague
