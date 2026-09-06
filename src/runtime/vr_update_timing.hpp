#pragma once
#include <cmath>
#include <cstdint>

namespace penumbra_vr::runtime {
struct VrUpdateTimingSample {
    double simulated_seconds = 0;
    double wall_seconds = 0;
    std::uint64_t ticks = 0;
    double ratio = 0;
    bool ready = false;
};
// Observes the native update clock; never changes dt, gravity or movement.
// UI, long stalls and invalid timestamps start a fresh measurement window.
class VrUpdateTiming final {
public:
    VrUpdateTimingSample Update(float dt, std::uint64_t now, bool gameplay) noexcept {
        if (!gameplay || !std::isfinite(dt) || dt<=0 || dt>0.25F ||
            (started_ && (now<previous_ || now-previous_>500))) {
            Reset(); return {};
        }
        if (!started_) { started_=true; start_=previous_=now; return {}; }
        previous_=now; simulated_+=dt; ++ticks_;
        if (now-start_<2000) return {};
        VrUpdateTimingSample sample;
        sample.simulated_seconds=simulated_;
        sample.wall_seconds=static_cast<double>(now-start_)/1000;
        sample.ticks=ticks_;
        sample.ratio=sample.simulated_seconds/sample.wall_seconds;
        sample.ready=true;
        start_=now; simulated_=0; ticks_=0;
        return sample;
    }
    void Reset() noexcept { *this={}; }
private:
    bool started_=false;
    std::uint64_t start_=0, previous_=0, ticks_=0;
    double simulated_=0;
};
}
