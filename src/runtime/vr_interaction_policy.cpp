#include "vr_interaction_policy.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime::vr_interaction_policy {

float ClampPhysicalInteractionReach(float native_reach) noexcept {
    if (!std::isfinite(native_reach) || native_reach <= 0.0F) {
        return 0.0F;
    }
    return (std::min)(native_reach, kMaximumCollisionInteractionReach);
}

} // namespace penumbra_vr::runtime::vr_interaction_policy
