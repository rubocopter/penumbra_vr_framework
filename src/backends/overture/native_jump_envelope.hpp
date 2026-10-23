#pragma once

#include <algorithm>
#include <cmath>

namespace penumbra_vr::backends::overture {

// BP/Requiem integrate Jump at 60 Hz; Overture's HMD-rate logic is incompatible
// with copying their one-tick launch/held forces directly. Sample that native
// vertical envelope without accessing body layouts or changing the scheduler.
// The source host applies travel through native forces/collision and abandons
// sampling whenever the solver or an external impulse changes its velocity.
class NativeJumpEnvelope {
public:
    void Begin(float launch_velocity, float gravity_y, float hold_seconds,
        float native_step_seconds) noexcept {
        gravity_y_ = gravity_y;
        hold_seconds_ = hold_seconds;
        native_step_ = native_step_seconds;
        remaining_ = native_step_seconds;
        native_hold_count_ = native_step_seconds;
        observed_hold_count_ = false;
        released_ = false;
        // Enter queues launch; the native first OnUpdate skips the held force.
        native_velocity_ = launch_velocity + gravity_y * native_step_seconds;
    }

    [[nodiscard]] float Advance(float seconds, bool held,
        float source_hold_count) noexcept {
        if (!std::isfinite(seconds) || seconds <= 0.0F || seconds > 0.25F ||
            !std::isfinite(native_step_) || native_step_ <= 0.0F ||
            !std::isfinite(native_velocity_) || !std::isfinite(gravity_y_) ||
            !std::isfinite(hold_seconds_) || hold_seconds_ <= 0.0F) return 0.0F;
        // SetJumpButtonDown(false) exhausts the native counter. Jump() resets
        // it even when already airborne, without entering Jump again. Observe
        // that reset separately from held recovery; it renews the hold window,
        // never the launch velocity or the current native interval.
        if (observed_hold_count_ && source_hold_count < previous_hold_count_) {
            native_hold_count_ = 0.0F;
            released_ = false;
        }
        previous_hold_count_ = source_hold_count;
        observed_hold_count_ = true;
        if (!held) released_ = true;
        double pending = seconds;
        double travel = 0.0;
        while (pending > 1.0e-9) {
            if (remaining_ <= 1.0e-9) {
                native_hold_count_ += native_step_;
                float acceleration = gravity_y_;
                if (!released_ && native_hold_count_ < hold_seconds_) {
                    const float factor = 0.4F + 0.5F *
                        (1.0F - native_hold_count_ / hold_seconds_);
                    acceleration -= gravity_y_ * factor;
                } else if (native_velocity_ > 0.0F) {
                    acceleration -= 20.0F;
                }
                native_velocity_ += acceleration * native_step_;
                remaining_ = native_step_;
            }
            const double step = (std::min)(pending, remaining_);
            travel += native_velocity_ * step;
            remaining_ -= step;
            pending -= step;
        }
        return static_cast<float>(travel);
    }

private:
    double remaining_{};
    float native_velocity_{};
    float native_hold_count_{};
    float gravity_y_{};
    float hold_seconds_{};
    float native_step_{};
    float previous_hold_count_{};
    bool observed_hold_count_{};
    bool released_{};
};

} // namespace penumbra_vr::backends::overture
