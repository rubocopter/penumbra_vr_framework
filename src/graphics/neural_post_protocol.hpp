#pragma once

#include <cstddef>
#include <cstdint>
#include <cwchar>

// Experimental x86 OpenGL <-> x64 DirectNR companion contract. Native
// layouts are identical in both builds; no NVIDIA binaries are embedded.
namespace penumbra_vr::neural_post {
inline constexpr std::uint32_t kMagic = 0x524E5650; // PVNR
inline constexpr std::uint32_t kVersion = 2;
inline constexpr std::uint32_t kMaxWidth = 1280;
inline constexpr std::uint32_t kMaxHeight = 1280;

struct alignas(8) FrameHeader {
    std::uint32_t magic = kMagic;
    std::uint32_t version = kVersion;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t request_id = 0;
    std::uint64_t completed_id = 0;
    std::uint32_t result = 0; // 1: both eyes processed; others: bypass.
    std::uint32_t tracking_valid = 0;
    float head[12]{}; // OpenVR row-major 3x4, metres.
    float eye_to_head[2][12]{};
    float frustum[2][4]{}; // left, right, up, down (tangents).
    std::uint64_t settings_revision = 0;
    std::uint64_t completed_revision = 0;
    std::uint64_t history_epoch = 0;
    std::uint32_t scale_percent = 25;
    std::uint32_t intensity_percent = 60;
    std::uint32_t passes = 1;
    std::uint32_t style = 0;
    std::uint32_t fovea_percent = 100;
    std::uint32_t outer_enabled = 0;
    std::uint32_t outer_scale_percent = 25;
    std::uint32_t reserved = 0;
};
static_assert(sizeof(FrameHeader) == 272);

[[nodiscard]] constexpr std::size_t EyeBytes(
    std::uint32_t width, std::uint32_t height) noexcept {
    return static_cast<std::size_t>(width) * height * 4;
}
[[nodiscard]] constexpr std::size_t PairBytes(
    std::uint32_t width, std::uint32_t height) noexcept {
    return EyeBytes(width, height) * 2;
}
[[nodiscard]] constexpr std::size_t MappingBytes() noexcept {
    return sizeof(FrameHeader) + 2 * PairBytes(kMaxWidth, kMaxHeight);
}
[[nodiscard]] inline std::uint8_t* InputPixels(FrameHeader* header) noexcept {
    return reinterpret_cast<std::uint8_t*>(header + 1);
}
[[nodiscard]] inline std::uint8_t* OutputPixels(FrameHeader* header) noexcept {
    return InputPixels(header) + PairBytes(kMaxWidth, kMaxHeight);
}
inline void ObjectName(wchar_t (&buffer)[100], const wchar_t* suffix,
    std::uint32_t pid) noexcept {
    swprintf_s(buffer, L"Local\\PenumbraNeural-%lu-%ls", pid, suffix);
}
} // namespace penumbra_vr::neural_post
