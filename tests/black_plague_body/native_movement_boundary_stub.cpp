#include "native_input_bridge.hpp"

namespace penumbra_vr::backends::black_plague {

// Synthetic verified owner; no native input calls are executed in this test.
bool g_test_movement_owner_ready = true;
NativeMovementBoundaryStatus ReadNativeMovementBoundaryStatus() noexcept {
    NativeMovementBoundaryStatus status;
    status.initialized = g_test_movement_owner_ready;
    return status;
}

} // namespace penumbra_vr::backends::black_plague
