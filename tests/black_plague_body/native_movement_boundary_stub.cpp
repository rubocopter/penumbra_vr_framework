#include "native_input_bridge.hpp"

namespace penumbra_vr::backends::black_plague {

// body_collision_probe_test includes the probe implementation directly and
// deliberately does not link the native input bridge. The adapter is inactive
// in that isolated probe test; this supplies only its read-only status import.
NativeMovementBoundaryStatus ReadNativeMovementBoundaryStatus() noexcept {
    return {};
}

} // namespace penumbra_vr::backends::black_plague
