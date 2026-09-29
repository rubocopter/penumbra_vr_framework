#include "black_plague_openal_compat.hpp"

#include <cstring>
#include <iostream>

namespace {

bool Same(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return lhs == rhs;
    return std::strcmp(lhs, rhs) == 0;
}

} // namespace

int main() {
    using penumbra_vr::black_plague::NormalizeOpenAlPlaybackDevice;

    if (NormalizeOpenAlPlaybackDevice(nullptr) != nullptr ||
        NormalizeOpenAlPlaybackDevice("") != nullptr ||
        NormalizeOpenAlPlaybackDevice("Generic Software") != nullptr ||
        NormalizeOpenAlPlaybackDevice("Generic Hardware") != nullptr) {
        std::cerr << "Legacy OpenAL playback aliases were not normalized to the default device\n";
        return 1;
    }

    const char* explicit_device = "OpenAL Soft on Headphones";
    if (!Same(NormalizeOpenAlPlaybackDevice(explicit_device), explicit_device)) {
        std::cerr << "An explicit modern OpenAL device name was changed\n";
        return 1;
    }

    std::cout << "Black Plague/Requiem OpenAL legacy-device normalization matches the proven Overture behavior\n";
    return 0;
}
