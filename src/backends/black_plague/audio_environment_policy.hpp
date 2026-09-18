#pragma once

#include <cmath>

namespace penumbra_vr::backends::black_plague {

// Layout-independent snapshot of the OALWrapper reverb state. The runtime
// adapter reads these values from the exact-build object, while tests exercise
// the decision without depending on process memory.
struct NativeReverbState {
    float density = 0.0F;
    float diffusion = 0.0F;
    float gain = 0.0F;
    float gain_hf = 0.0F;
    float gain_lf = 0.0F;
    float decay_time = 0.0F;
    float decay_hf_ratio = 0.0F;
    float decay_lf_ratio = 0.0F;
    float reflections_gain = 0.0F;
    float reflections_delay = 0.0F;
    float late_reverb_gain = 0.0F;
    float late_reverb_delay = 0.0F;
    float echo_time = 0.0F;
    float echo_depth = 0.0F;
    float modulation_time = 0.0F;
    float modulation_depth = 0.0F;
    float air_absorption_gain_hf = 0.0F;
    float hf_reference = 0.0F;
    float lf_reference = 0.0F;
    float room_rolloff_factor = 0.0F;
};

inline constexpr NativeReverbState kNativeOalReverbDefaults{
    1.0F, 1.0F, 0.32F, 0.89F, 0.0F,
    1.49F, 0.83F, 1.0F, 0.05F, 0.007F,
    1.25F, 0.011F, 0.25F, 0.0F, 0.25F, 0.0F,
    0.994F, 5000.0F, 250.0F, 0.0F,
};

[[nodiscard]] inline bool NearlyEqualReverbValue(
    float left, float right, float epsilon = 0.0005F) noexcept {
    return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] inline bool IsNativeDefaultReverb(
    const NativeReverbState& state) noexcept {
    const auto& d = kNativeOalReverbDefaults;
    return NearlyEqualReverbValue(state.density, d.density) &&
        NearlyEqualReverbValue(state.diffusion, d.diffusion) &&
        NearlyEqualReverbValue(state.gain, d.gain) &&
        NearlyEqualReverbValue(state.gain_hf, d.gain_hf) &&
        NearlyEqualReverbValue(state.gain_lf, d.gain_lf) &&
        NearlyEqualReverbValue(state.decay_time, d.decay_time) &&
        NearlyEqualReverbValue(state.decay_hf_ratio, d.decay_hf_ratio) &&
        NearlyEqualReverbValue(state.decay_lf_ratio, d.decay_lf_ratio) &&
        NearlyEqualReverbValue(state.reflections_gain, d.reflections_gain) &&
        NearlyEqualReverbValue(state.reflections_delay, d.reflections_delay) &&
        NearlyEqualReverbValue(state.late_reverb_gain, d.late_reverb_gain) &&
        NearlyEqualReverbValue(state.late_reverb_delay, d.late_reverb_delay) &&
        NearlyEqualReverbValue(state.echo_time, d.echo_time) &&
        NearlyEqualReverbValue(state.echo_depth, d.echo_depth) &&
        NearlyEqualReverbValue(state.modulation_time, d.modulation_time) &&
        NearlyEqualReverbValue(state.modulation_depth, d.modulation_depth) &&
        NearlyEqualReverbValue(
            state.air_absorption_gain_hf, d.air_absorption_gain_hf) &&
        NearlyEqualReverbValue(state.hf_reference, d.hf_reference, 0.01F) &&
        NearlyEqualReverbValue(state.lf_reference, d.lf_reference, 0.01F) &&
        NearlyEqualReverbValue(
            state.room_rolloff_factor, d.room_rolloff_factor);
}

} // namespace penumbra_vr::backends::black_plague
