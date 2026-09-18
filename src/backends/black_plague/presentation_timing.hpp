#pragma once

#include <cstdint>

namespace penumbra_vr::backends::black_plague {

// Backend-local diagnostics for the existing single-consumption presentation
// contract. "Jitter" here is the absolute first difference between consecutive
// compositor-pose acquisition intervals; it is intentionally observational and
// does not change frame pacing or pose ownership.
struct PresentationTimingWindow {
    bool acquisition_interval_valid = false;
    std::uint64_t acquisition_interval_ms = 0;
    bool acquisition_jitter_valid = false;
    std::uint64_t acquisition_jitter_ms = 0;
    bool render_age_valid = false;
    std::uint64_t render_age_ms = 0;
    bool submit_age_valid = false;
    std::uint64_t submit_age_ms = 0;
};

class PresentationTimingTracker final {
public:
    void Reset() noexcept {
        ResetHistory();
        window_ = {};
    }

    void ResetHistory() noexcept {
        last_acquisition_timestamp_ms_ = 0;
        last_acquisition_interval_ms_ = 0;
        has_last_acquisition_interval_ = false;
    }

    void RecordAcquisition(std::uint64_t timestamp_ms) noexcept {
        if (timestamp_ms == 0) {
            return;
        }

        if (last_acquisition_timestamp_ms_ != 0 &&
            timestamp_ms >= last_acquisition_timestamp_ms_) {
            const std::uint64_t interval_ms =
                timestamp_ms - last_acquisition_timestamp_ms_;
            window_.acquisition_interval_valid = true;
            window_.acquisition_interval_ms = interval_ms;
            if (has_last_acquisition_interval_) {
                window_.acquisition_jitter_valid = true;
                window_.acquisition_jitter_ms =
                    interval_ms >= last_acquisition_interval_ms_
                        ? interval_ms - last_acquisition_interval_ms_
                        : last_acquisition_interval_ms_ - interval_ms;
            }
            last_acquisition_interval_ms_ = interval_ms;
            has_last_acquisition_interval_ = true;
        } else if (last_acquisition_timestamp_ms_ != 0) {
            // A backwards clock/sample boundary invalidates interval history.
            last_acquisition_interval_ms_ = 0;
            has_last_acquisition_interval_ = false;
        }

        last_acquisition_timestamp_ms_ = timestamp_ms;
    }

    void RecordRenderAge(
        std::uint64_t timestamp_ms,
        std::uint64_t now_ms) noexcept {
        RecordAge(timestamp_ms, now_ms,
            window_.render_age_valid, window_.render_age_ms);
    }

    void RecordSubmitAge(
        std::uint64_t timestamp_ms,
        std::uint64_t now_ms) noexcept {
        RecordAge(timestamp_ms, now_ms,
            window_.submit_age_valid, window_.submit_age_ms);
    }

    [[nodiscard]] PresentationTimingWindow ConsumeWindow() noexcept {
        const PresentationTimingWindow result = window_;
        window_ = {};
        return result;
    }

private:
    static void RecordAge(
        std::uint64_t timestamp_ms,
        std::uint64_t now_ms,
        bool& valid,
        std::uint64_t& age_ms) noexcept {
        if (timestamp_ms == 0 || now_ms < timestamp_ms) {
            return;
        }
        valid = true;
        age_ms = now_ms - timestamp_ms;
    }

    std::uint64_t last_acquisition_timestamp_ms_ = 0;
    std::uint64_t last_acquisition_interval_ms_ = 0;
    bool has_last_acquisition_interval_ = false;
    PresentationTimingWindow window_{};
};

} // namespace penumbra_vr::backends::black_plague
