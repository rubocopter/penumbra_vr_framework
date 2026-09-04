#pragma once

#include "vr_math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace penumbra_vr::adapters::hpl1 {

struct CameraLayout {
    std::size_t view_matrix_offset = 0;
    std::size_t projection_matrix_offset = 0;
    std::size_t flags_offset = 0;
    std::size_t view_updated_flag_index = 0;
    std::size_t projection_updated_flag_index = 0;
};

struct CameraMatrixSnapshot {
    runtime::VrMatrix44 view;
    runtime::VrMatrix44 projection;
    std::array<std::uint8_t, 3> flags{};
};

[[nodiscard]] bool CaptureCameraMatrices(
    const void* camera,
    const CameraLayout& layout,
    CameraMatrixSnapshot& snapshot,
    std::string& error) noexcept;

class CameraMatrixOverride final {
public:
    explicit CameraMatrixOverride(CameraLayout layout) noexcept;
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
    CameraLayout layout_{};
    void* camera_ = nullptr;
    CameraMatrixSnapshot snapshot_{};
};

[[nodiscard]] bool CameraMatchesSnapshot(
    const void* camera,
    const CameraLayout& layout,
    const CameraMatrixSnapshot& snapshot) noexcept;

} // namespace penumbra_vr::adapters::hpl1
