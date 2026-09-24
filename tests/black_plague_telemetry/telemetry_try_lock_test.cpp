#include "telemetry_try_lock.hpp"

#include <windows.h>

#include <iostream>

int main() {
    using penumbra_vr::backends::black_plague::TelemetryTryLock;

    SRWLOCK lock = SRWLOCK_INIT;
    {
        TelemetryTryLock guard(lock);
        if (!guard.acquired()) {
            std::cerr << "Telemetry try-lock did not acquire a free lock\n";
            return 1;
        }
    }

    AcquireSRWLockExclusive(&lock);
    {
        TelemetryTryLock guard(lock);
        if (guard.acquired()) {
            ReleaseSRWLockExclusive(&lock);
            std::cerr << "Telemetry try-lock blocked/acquired a busy lock\n";
            return 2;
        }
    }
    ReleaseSRWLockExclusive(&lock);

    std::cout << "Black Plague telemetry try-lock passed\n";
    return 0;
}
