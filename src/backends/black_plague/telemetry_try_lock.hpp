#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace penumbra_vr::backends::black_plague {

class TelemetryTryLock final {
public:
    explicit TelemetryTryLock(SRWLOCK& lock) noexcept
        : lock_(&lock), acquired_(TryAcquireSRWLockExclusive(lock_) != 0) {}

    TelemetryTryLock(const TelemetryTryLock&) = delete;
    TelemetryTryLock& operator=(const TelemetryTryLock&) = delete;

    ~TelemetryTryLock() {
        if (acquired_) {
            ReleaseSRWLockExclusive(lock_);
        }
    }

    [[nodiscard]] bool acquired() const noexcept {
        return acquired_;
    }

private:
    SRWLOCK* lock_ = nullptr;
    bool acquired_ = false;
};

} // namespace penumbra_vr::backends::black_plague
