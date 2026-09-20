#include "particle_stereo_refresh.hpp"

#include <cmath>
#include <cstdint>

namespace {

using penumbra_vr::backends::black_plague::ParticleStereoRefreshState;

struct UpdateObservation {
    void* emitter = nullptr;
    void* camera = nullptr;
    float frame_time = 0.0F;
    void* render_list = nullptr;
    std::uint32_t calls = 0;
} g_observation;

void __fastcall FakeUpdateGraphics(
    void* emitter,
    void*,
    void* camera,
    float frame_time,
    void* render_list) {
    g_observation.emitter = emitter;
    g_observation.camera = camera;
    g_observation.frame_time = frame_time;
    g_observation.render_list = render_list;
    ++g_observation.calls;
}

} // namespace

int main() {
    ParticleStereoRefreshState state;
    int emitter_storage = 0;
    int native_camera_storage = 0;
    int left_camera_storage = 0;
    int right_camera_storage = 0;
    int render_list_storage = 0;

    void* const emitter = &emitter_storage;
    void* const native_camera = &native_camera_storage;
    void* const left_camera = &left_camera_storage;
    void* const right_camera = &right_camera_storage;
    void* const render_list = &render_list_storage;

    if (state.Refresh(
            emitter,
            left_camera,
            reinterpret_cast<void*>(&FakeUpdateGraphics))) {
        return 1;
    }
    if (g_observation.calls != 0) return 2;

    state.Capture(native_camera, 0.016F, render_list);
    if (!state.Refresh(
            emitter,
            left_camera,
            reinterpret_cast<void*>(&FakeUpdateGraphics))) {
        return 3;
    }
    if (g_observation.calls != 1 || g_observation.emitter != emitter ||
        g_observation.camera != left_camera ||
        std::abs(g_observation.frame_time - 0.016F) > 0.000001F ||
        g_observation.render_list != render_list) {
        return 4;
    }

    if (!state.Refresh(
            emitter,
            right_camera,
            reinterpret_cast<void*>(&FakeUpdateGraphics))) {
        return 5;
    }
    if (g_observation.calls != 2 || g_observation.camera != right_camera ||
        std::abs(g_observation.frame_time - 0.016F) > 0.000001F ||
        g_observation.render_list != render_list) {
        return 6;
    }

    state.Reset();
    if (state.Refresh(
            emitter,
            left_camera,
            reinterpret_cast<void*>(&FakeUpdateGraphics)) ||
        g_observation.calls != 2) {
        return 7;
    }
    return 0;
}
