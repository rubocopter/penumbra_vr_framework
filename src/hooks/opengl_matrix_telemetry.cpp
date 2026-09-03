#include "opengl_matrix_telemetry.hpp"

#include "iat_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstring>

namespace penumbra_vr::hooks {
namespace {

constexpr unsigned int kGlModelView = 0x1700;
constexpr unsigned int kGlProjection = 0x1701;
constexpr unsigned int kGlTexture = 0x1702;

using GlMatrixMode = void(APIENTRY*)(unsigned int mode);
using GlLoadMatrixf = void(APIENTRY*)(const float* matrix);
using GlOrtho = void(APIENTRY*)(double, double, double, double, double, double);

IatHook g_matrix_mode_hook;
IatHook g_load_matrix_hook;
IatHook g_ortho_hook;
std::atomic<unsigned int> g_current_matrix_mode{kGlModelView};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
OpenGlFrameTelemetry g_telemetry;

void APIENTRY HookedGlMatrixMode(unsigned int mode) noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.matrix_mode_calls;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_current_matrix_mode.store(mode, std::memory_order_relaxed);
    reinterpret_cast<GlMatrixMode>(g_matrix_mode_hook.original)(mode);
}

void APIENTRY HookedGlLoadMatrixf(const float* matrix) noexcept {
    const unsigned int mode = g_current_matrix_mode.load(std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    if (mode == kGlProjection) {
        ++g_telemetry.projection_loads;
        if (matrix != nullptr) {
            std::memcpy(
                g_telemetry.last_projection.data(),
                matrix,
                g_telemetry.last_projection.size() * sizeof(float));
            g_telemetry.has_projection = true;
        }
    } else if (mode == kGlModelView) {
        ++g_telemetry.model_view_loads;
    } else if (mode == kGlTexture) {
        ++g_telemetry.texture_loads;
    }
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    reinterpret_cast<GlLoadMatrixf>(g_load_matrix_hook.original)(matrix);
}

void APIENTRY HookedGlOrtho(
    double left,
    double right,
    double bottom,
    double top,
    double near_value,
    double far_value) noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.ortho_calls;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    reinterpret_cast<GlOrtho>(g_ortho_hook.original)(
        left, right, bottom, top, near_value, far_value);
}

} // namespace

bool InstallOpenGlMatrixTelemetry(std::string& error) noexcept {
    error.clear();
    g_current_matrix_mode.store(kGlModelView, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    if (!InstallIatHook(
            "OPENGL32.dll",
            "glMatrixMode",
            reinterpret_cast<void*>(&HookedGlMatrixMode),
            g_matrix_mode_hook,
            error)) {
        return false;
    }
    if (!InstallIatHook(
            "OPENGL32.dll",
            "glLoadMatrixf",
            reinterpret_cast<void*>(&HookedGlLoadMatrixf),
            g_load_matrix_hook,
            error)) {
        std::string ignored;
        static_cast<void>(RemoveIatHook(g_matrix_mode_hook, ignored));
        return false;
    }
    if (!InstallIatHook(
            "OPENGL32.dll",
            "glOrtho",
            reinterpret_cast<void*>(&HookedGlOrtho),
            g_ortho_hook,
            error)) {
        std::string ignored;
        static_cast<void>(RemoveIatHook(g_load_matrix_hook, ignored));
        static_cast<void>(RemoveIatHook(g_matrix_mode_hook, ignored));
        return false;
    }
    return true;
}

bool RemoveOpenGlMatrixTelemetry(std::string& error) noexcept {
    if (!RemoveIatHook(g_ortho_hook, error)) {
        return false;
    }
    if (!RemoveIatHook(g_load_matrix_hook, error)) {
        return false;
    }
    return RemoveIatHook(g_matrix_mode_hook, error);
}

OpenGlFrameTelemetry ConsumeOpenGlFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    OpenGlFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

} // namespace penumbra_vr::hooks
