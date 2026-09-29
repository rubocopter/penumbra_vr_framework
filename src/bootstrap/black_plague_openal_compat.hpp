#pragma once

#include <cstring>

namespace penumbra_vr::black_plague {

inline const char* NormalizeOpenAlPlaybackDevice(const char* device_name) noexcept {
    if (device_name == nullptr || device_name[0] == '\0') return nullptr;
    if (std::strcmp(device_name, "Generic Software") == 0 ||
        std::strcmp(device_name, "Generic Hardware") == 0) {
        return nullptr;
    }
    return device_name;
}

} // namespace penumbra_vr::black_plague
