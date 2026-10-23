#include "tracked_listener.hpp"

#include <cmath>

namespace penumbra_vr::audio {
bool BuildTrackedListenerPose(
    const runtime::VrMatrix44& head_world, TrackedListenerPose& listener) noexcept {
    listener = {};
    runtime::VrMatrix34 rigid;
    for (std::size_t i = 0; i < rigid.values.size(); ++i)
        rigid.values[i] = head_world.values[i];
    runtime::VrMatrix44 inverse;
    std::string error;
    if (!runtime::InvertRigidTransform(rigid, inverse, error)) return false;
    for (const float value : head_world.values)
        if (!std::isfinite(value)) return false;
    if (std::abs(head_world.values[12]) > 0.0001F ||
        std::abs(head_world.values[13]) > 0.0001F ||
        std::abs(head_world.values[14]) > 0.0001F ||
        std::abs(head_world.values[15] - 1.0F) > 0.0001F) return false;
    listener.position = {head_world.values[3], head_world.values[7], head_world.values[11]};
    listener.orientation = {-head_world.values[2], -head_world.values[6],
        -head_world.values[10], head_world.values[1], head_world.values[5], head_world.values[9]};
    return true;
}
} // namespace penumbra_vr::audio
