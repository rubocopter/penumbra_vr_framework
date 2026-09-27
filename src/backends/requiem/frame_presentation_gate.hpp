#pragma once

#include <atomic>

namespace penumbra_vr::backends::requiem {

class FramePresentationGate final {
public:
    void MarkWorldPresented() noexcept {
        world_presented_.store(true, std::memory_order_release);
    }

    [[nodiscard]] bool ConsumeWorldPresentedAtSwap() noexcept {
        return world_presented_.exchange(false, std::memory_order_acq_rel);
    }

    void Reset() noexcept {
        world_presented_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> world_presented_{false};
};

} // namespace penumbra_vr::backends::requiem
