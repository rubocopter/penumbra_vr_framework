#include "room_scale_camera.hpp"
#include "vr_math.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>

namespace bp = penumbra_vr::backends::black_plague;
namespace vr = penumbra_vr::runtime;

int main() {
    vr::VrMatrix34 physical{{1,0,0,0, 0,1,0,1.2F, 0,0,1,0}};
    vr::VrMatrix44 native_view{{1,0,0,0, 0,1,0,-1.65F, 0,0,1,0, 0,0,0,1}};
    bp::BlackPlagueRoomScaleCameraSample body;
    body.enabled = body.valid = true;
    body.observed_tracking_pose = physical;
    body.predicted_head_anchor = {0, 0, 0};
    body.tracking_identity = {1, 100, 1, 1};
    bp::BlackPlagueRoomScaleViewSettings settings;
    settings.play_mode = vr::VrPlayMode::seated;
    settings.player_height = 1.7F;
    settings.posture_offset = -0.25F;
    vr::VrPlayModePolicy play_mode;
    std::string error;
    std::array<float, 3> translation{}, prediction{}, anchor{};
    bool available = false;
    const auto resolve = [&](vr::VrTrackingSampleIdentity identity) {
        return bp::ResolveBlackPlagueRoomScalePlacement(native_view, physical,
            physical, identity, body, settings, play_mode, available,
            translation, prediction, anchor, error);
    };
    if (!resolve({2, 110, 1, 1}) || !available) return 1;
    const auto baseline = anchor;
    if (std::abs(baseline[1] - 1.45F) > 0.00001F) return 8;
    // Smooth turn changes the yaw epoch every update before the next body tick.
    // It must suppress stale horizontal prediction without reverting eye height
    // to the native standing camera or discarding the accepted world anchor.
    for (std::uint64_t yaw = 2; yaw != 15; ++yaw) {
        if (!resolve({yaw + 1, 110 + yaw, 1, yaw}) || !available ||
            std::abs(anchor[1] - baseline[1]) > 0.00001F ||
            prediction != std::array<float, 3>{}) {
            std::cerr << "Turning discarded calibrated/crouched eye height\n";
            return 2;
        }
        vr::VrMatrix44 translated, rendered;
        vr::VrMatrix34 translated_rigid;
        if (!vr::ApplyWorldTranslationToView(native_view, translation, translated, error)) return 3;
        std::copy_n(translated.values.begin(), 12, translated_rigid.values.begin());
        if (!vr::InvertRigidTransform(translated_rigid, rendered, error) ||
            std::abs(rendered.values[7] - baseline[1]) > 0.00001F) return 3;
    }
    physical.values[3] = 0.05F;
    if (!resolve({19, 140, 1, 15}) || !available ||
        std::abs(anchor[0] - baseline[0]) > 0.00001F ||
        prediction != std::array<float, 3>{}) return 9;
    physical.values[3] = 0.0F;
    if (!resolve({20, 150, 2, 15}) || available) {
        std::cerr << "A tracking-origin discontinuity reused an old body sample\n";
        return 4;
    }
    // Matching epochs retain the proven between-tick physical prediction.
    physical.values[3] = 0.05F;
    if (!resolve({21, 160, 1, 1}) || !available ||
        std::abs(prediction[0] - 0.05F) > 0.00001F) return 5;
    physical.values[3] = 2.0F;
    if (!resolve({22, 170, 1, 1}) || available) return 6;
    body.valid = false;
    if (!resolve({23, 180, 1, 1}) || available) return 7;
    body.valid = true;
    physical.values[3] = 0.0F;
    if (!resolve({24, 190, 1, 0}) || available) return 10;
    settings.play_mode = vr::VrPlayMode::standing;
    settings.posture_offset = 0.0F;
    physical.values[7] = 0.90F;
    if (!resolve({25, 200, 1, 20}) || !available ||
        std::abs(anchor[1] - 0.7455F) > 0.00001F) return 11;
    std::cout << "Black Plague turn/height continuity and tracking guards passed\n";
    return 0;
}
