#include "audio_environment_policy.hpp"

#include <cstdlib>

using penumbra_vr::backends::black_plague::IsNativeDefaultReverb;
using penumbra_vr::backends::black_plague::NativeReverbState;
using penumbra_vr::backends::black_plague::kNativeOalReverbDefaults;

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

} // namespace

int main() {
    Require(IsNativeDefaultReverb(kNativeOalReverbDefaults));

    NativeReverbState active_environment = kNativeOalReverbDefaults;
    active_environment.density = 0.72F;
    Require(!IsNativeDefaultReverb(active_environment));

    NativeReverbState null_environment{};
    Require(!IsNativeDefaultReverb(null_environment));

    NativeReverbState tiny_rounding_delta = kNativeOalReverbDefaults;
    tiny_rounding_delta.decay_time += 0.0001F;
    Require(IsNativeDefaultReverb(tiny_rounding_delta));

    NativeReverbState meaningful_delta = kNativeOalReverbDefaults;
    meaningful_delta.air_absorption_gain_hf -= 0.01F;
    Require(!IsNativeDefaultReverb(meaningful_delta));
    return EXIT_SUCCESS;
}
