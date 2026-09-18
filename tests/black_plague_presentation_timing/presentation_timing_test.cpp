#include "presentation_timing.hpp"

#include <iostream>

int main() {
    using penumbra_vr::backends::black_plague::PresentationTimingTracker;

    PresentationTimingTracker tracker;
    tracker.RecordAcquisition(100);
    auto window = tracker.ConsumeWindow();
    if (window.acquisition_interval_valid ||
        window.acquisition_jitter_valid) {
        std::cerr << "First acquisition unexpectedly produced interval timing\n";
        return 1;
    }

    tracker.RecordAcquisition(111);
    window = tracker.ConsumeWindow();
    if (!window.acquisition_interval_valid ||
        window.acquisition_interval_ms != 11 ||
        window.acquisition_jitter_valid) {
        std::cerr << "Second acquisition interval timing failed\n";
        return 2;
    }

    tracker.RecordAcquisition(121);
    tracker.RecordRenderAge(121, 128);
    tracker.RecordSubmitAge(121, 133);
    window = tracker.ConsumeWindow();
    if (!window.acquisition_interval_valid ||
        window.acquisition_interval_ms != 10 ||
        !window.acquisition_jitter_valid ||
        window.acquisition_jitter_ms != 1 ||
        !window.render_age_valid || window.render_age_ms != 7 ||
        !window.submit_age_valid || window.submit_age_ms != 12) {
        std::cerr << "Presentation age/jitter timing failed\n";
        return 3;
    }

    tracker.RecordAcquisition(132);
    window = tracker.ConsumeWindow();
    if (!window.acquisition_interval_valid ||
        window.acquisition_interval_ms != 11 ||
        !window.acquisition_jitter_valid ||
        window.acquisition_jitter_ms != 1) {
        std::cerr << "Interval history did not survive telemetry consumption\n";
        return 4;
    }

    tracker.ResetHistory();
    tracker.RecordAcquisition(200);
    tracker.RecordAcquisition(190);
    tracker.RecordRenderAge(210, 209);
    tracker.RecordSubmitAge(0, 220);
    window = tracker.ConsumeWindow();
    if (window.acquisition_interval_valid ||
        window.acquisition_jitter_valid ||
        window.render_age_valid ||
        window.submit_age_valid) {
        std::cerr << "Reset/invalid timing samples were not rejected\n";
        return 5;
    }

    std::cout << "Black Plague presentation timing diagnostics passed\n";
    return 0;
}
