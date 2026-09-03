#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace penumbra_vr::launcher {

struct TextSectionInspection {
    std::uint32_t rva = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t raw_size = 0;
    std::size_t compared_bytes = 0;
    std::size_t different_bytes = 0;
    std::uint32_t first_difference_rva = 0;
    double disk_entropy = 0.0;
    double memory_entropy = 0.0;
};

[[nodiscard]] bool InspectRemoteTextSection(
    HANDLE process,
    std::uintptr_t module_base,
    const std::filesystem::path& executable_path,
    const std::filesystem::path* reconstructed_image_path,
    TextSectionInspection& inspection,
    std::wstring& error);

} // namespace penumbra_vr::launcher
