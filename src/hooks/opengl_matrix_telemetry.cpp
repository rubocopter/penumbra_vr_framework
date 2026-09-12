#include "opengl_matrix_telemetry.hpp"

#include "iat_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <array>
#include <cstring>
#include <string_view>

namespace penumbra_vr::hooks {
namespace {

constexpr unsigned int kGlModelView = 0x1700;
constexpr unsigned int kGlProjection = 0x1701;
constexpr unsigned int kGlTexture = 0x1702;
constexpr std::size_t kMaxTrackedModelViewMatrices = 64;

struct ModelViewObservation {
    std::array<float, 16> matrix{};
    std::uint32_t loads = 0;
};

using GlMatrixMode = void(APIENTRY*)(unsigned int mode);
using GlLoadMatrixf = void(APIENTRY*)(const float* matrix);
using GlOrtho = void(APIENTRY*)(double, double, double, double, double, double);

IatHook g_matrix_mode_hook;
IatHook g_load_matrix_hook;
IatHook g_ortho_hook;
std::atomic<void*> g_original_matrix_mode{nullptr};
std::atomic<void*> g_original_load_matrix{nullptr};
std::atomic<void*> g_original_ortho{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
std::atomic<unsigned int> g_current_matrix_mode{kGlModelView};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
OpenGlFrameTelemetry g_telemetry;
std::array<ModelViewObservation, kMaxTrackedModelViewMatrices> g_model_view_observations;
std::size_t g_model_view_observation_count = 0;
std::uint32_t g_dropped_model_view_matrices = 0;

class ActiveCall final {
public:
    ActiveCall() noexcept {
        g_active_calls.fetch_add(1, std::memory_order_acq_rel);
    }
    ActiveCall(const ActiveCall&) = delete;
    ActiveCall& operator=(const ActiveCall&) = delete;
    ~ActiveCall() {
        g_active_calls.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void PublishOriginalMatrixMode(void* original) noexcept {
    g_original_matrix_mode.store(original, std::memory_order_release);
}

void PublishOriginalLoadMatrix(void* original) noexcept {
    g_original_load_matrix.store(original, std::memory_order_release);
}

void PublishOriginalOrtho(void* original) noexcept {
    g_original_ortho.store(original, std::memory_order_release);
}

[[nodiscard]] bool AllHooksInstalled() noexcept {
    return g_matrix_mode_hook.installed() && g_load_matrix_hook.installed() &&
        g_ortho_hook.installed();
}

[[nodiscard]] bool AnyHookInstalled() noexcept {
    return g_matrix_mode_hook.installed() || g_load_matrix_hook.installed() ||
        g_ortho_hook.installed();
}

void AppendError(std::string& error, std::string_view context,
    const std::string& next) {
    if (next.empty()) {
        return;
    }
    if (!error.empty()) {
        error += "; ";
    }
    error.append(context);
    error += next;
}

[[nodiscard]] bool RemoveHooks(std::string& error) noexcept {
    bool success = true;
    for (auto* hook : {&g_ortho_hook, &g_load_matrix_hook,
             &g_matrix_mode_hook}) {
        std::string next;
        if (!RemoveIatHook(*hook, next)) {
            success = false;
            AppendError(error, "hook removal failed: ", next);
        }
    }
    return success;
}

void RecordModelViewMatrix(const float* matrix) noexcept {
    if (matrix == nullptr) {
        return;
    }

    constexpr std::size_t kMatrixBytes = 16 * sizeof(float);
    for (std::size_t index = 0; index < g_model_view_observation_count; ++index) {
        ModelViewObservation& observation = g_model_view_observations[index];
        if (std::memcmp(observation.matrix.data(), matrix, kMatrixBytes) == 0) {
            ++observation.loads;
            return;
        }
    }

    if (g_model_view_observation_count == g_model_view_observations.size()) {
        ++g_dropped_model_view_matrices;
        return;
    }

    ModelViewObservation& observation =
        g_model_view_observations[g_model_view_observation_count++];
    std::memcpy(observation.matrix.data(), matrix, kMatrixBytes);
    observation.loads = 1;
}

void APIENTRY HookedGlMatrixMode(unsigned int mode) noexcept {
    ActiveCall active_call;
    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.matrix_mode_calls;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_current_matrix_mode.store(mode, std::memory_order_relaxed);
    const auto original = reinterpret_cast<GlMatrixMode>(
        g_original_matrix_mode.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(mode);
    }
}

void APIENTRY HookedGlLoadMatrixf(const float* matrix) noexcept {
    ActiveCall active_call;
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
        if (g_telemetry.projection_call_stack_depth == 0) {
            std::array<void*, 8> frames{};
            const USHORT depth = RtlCaptureStackBackTrace(
                0,
                static_cast<ULONG>(frames.size()),
                frames.data(),
                nullptr);
            g_telemetry.projection_call_stack_depth = depth;
            for (USHORT index = 0; index < depth; ++index) {
                g_telemetry.projection_call_stack[index] =
                    reinterpret_cast<std::uintptr_t>(frames[index]);
            }
        }
    } else if (mode == kGlModelView) {
        ++g_telemetry.model_view_loads;
        RecordModelViewMatrix(matrix);
    } else if (mode == kGlTexture) {
        ++g_telemetry.texture_loads;
    }
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    const auto original = reinterpret_cast<GlLoadMatrixf>(
        g_original_load_matrix.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(matrix);
    }
}

void APIENTRY HookedGlOrtho(
    double left,
    double right,
    double bottom,
    double top,
    double near_value,
    double far_value) noexcept {
    ActiveCall active_call;
    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.ortho_calls;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    const auto original = reinterpret_cast<GlOrtho>(
        g_original_ortho.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(left, right, bottom, top, near_value, far_value);
    }
}

} // namespace

bool InstallOpenGlMatrixTelemetry(std::string& error) noexcept {
    error.clear();
    if (AllHooksInstalled()) {
        return true;
    }
    if (AnyHookInstalled()) {
        error = "OpenGL matrix telemetry is only partially installed";
        return false;
    }
    g_current_matrix_mode.store(kGlModelView, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    g_model_view_observation_count = 0;
    g_dropped_model_view_matrices = 0;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    if (!InstallIatHook(
            "OPENGL32.dll",
            "glMatrixMode",
            reinterpret_cast<void*>(&HookedGlMatrixMode),
            g_matrix_mode_hook,
            error,
            &PublishOriginalMatrixMode)) {
        return false;
    }
    if (!InstallIatHook(
            "OPENGL32.dll",
            "glLoadMatrixf",
            reinterpret_cast<void*>(&HookedGlLoadMatrixf),
            g_load_matrix_hook,
            error,
            &PublishOriginalLoadMatrix)) {
        const std::string install_error = error;
        std::string rollback_error;
        static_cast<void>(RemoveHooks(rollback_error));
        error = install_error;
        AppendError(error, "rollback failed: ", rollback_error);
        return false;
    }
    if (!InstallIatHook(
            "OPENGL32.dll",
            "glOrtho",
            reinterpret_cast<void*>(&HookedGlOrtho),
            g_ortho_hook,
            error,
            &PublishOriginalOrtho)) {
        const std::string install_error = error;
        std::string rollback_error;
        static_cast<void>(RemoveHooks(rollback_error));
        error = install_error;
        AppendError(error, "rollback failed: ", rollback_error);
        return false;
    }
    return true;
}

bool RemoveOpenGlMatrixTelemetry(std::string& error) noexcept {
    error.clear();
    if (!RemoveHooks(error) || AnyHookInstalled()) {
        if (error.empty()) {
            error = "OpenGL matrix telemetry remains partially installed";
        }
        return false;
    }

    constexpr std::uint32_t kQuiescenceTimeoutMilliseconds = 2000;
    for (std::uint32_t elapsed = 0;
         elapsed < kQuiescenceTimeoutMilliseconds;
         ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for active OpenGL telemetry calls to finish";
    return false;
}

OpenGlFrameTelemetry ConsumeOpenGlFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    OpenGlFrameTelemetry result = g_telemetry;
    result.unique_model_view_matrices =
        static_cast<std::uint32_t>(g_model_view_observation_count);
    result.dropped_model_view_matrices = g_dropped_model_view_matrices;
    for (std::size_t index = 0; index < g_model_view_observation_count; ++index) {
        const ModelViewObservation& observation = g_model_view_observations[index];
        if (observation.loads > result.dominant_model_view_loads) {
            result.dominant_model_view_loads = observation.loads;
            result.dominant_model_view = observation.matrix;
            result.has_dominant_model_view = true;
        }
    }
    g_telemetry = {};
    g_model_view_observation_count = 0;
    g_dropped_model_view_matrices = 0;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

} // namespace penumbra_vr::hooks
