#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>

namespace penumbra_vr::backends::black_plague {

using ParticleUpdateGraphics = void(__thiscall*)(
    void* emitter,
    void* camera,
    float frame_time,
    void* render_list);

// Stock Black Plague updates camera-dependent particle vertices once while
// building the render list. Rework refreshes those vertices from
// ParticleEmitter3D::GetModelMatrix so each stereo eye receives geometry made
// for that eye camera. The hook only needs the last render-list arguments;
// UpdateRenderList supplies the same frame time/list to the emitters rendered
// by the following world pass.
class ParticleStereoRefreshState final {
public:
    void Capture(void* camera, float frame_time, void* render_list) noexcept {
        if (camera == nullptr || !std::isfinite(frame_time)) return;
        AcquireSRWLockExclusive(&lock_);
        frame_time_ = frame_time;
        render_list_ = render_list;
        valid_ = true;
        ReleaseSRWLockExclusive(&lock_);
    }

    [[nodiscard]] bool Refresh(
        void* emitter,
        void* eye_camera,
        void* original_update_graphics) noexcept {
        if (emitter == nullptr || eye_camera == nullptr ||
            original_update_graphics == nullptr) {
            return false;
        }
        float frame_time = 0.0F;
        void* render_list = nullptr;
        AcquireSRWLockShared(&lock_);
        const bool valid = valid_;
        if (valid) {
            frame_time = frame_time_;
            render_list = render_list_;
        }
        ReleaseSRWLockShared(&lock_);
        if (!valid) return false;
        reinterpret_cast<ParticleUpdateGraphics>(original_update_graphics)(
            emitter, eye_camera, frame_time, render_list);
        return true;
    }

    void Reset() noexcept {
        AcquireSRWLockExclusive(&lock_);
        frame_time_ = 0.0F;
        render_list_ = nullptr;
        valid_ = false;
        ReleaseSRWLockExclusive(&lock_);
    }

private:
    SRWLOCK lock_ = SRWLOCK_INIT;
    float frame_time_ = 0.0F;
    void* render_list_ = nullptr;
    bool valid_ = false;
};

} // namespace penumbra_vr::backends::black_plague
