#include "render_world_probe.hpp"

#include "rel32_call_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <atomic>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr std::array<std::uint8_t, 5> kExpectedCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);

hooks::Rel32CallHook g_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
RenderWorldFrameTelemetry g_telemetry;
std::atomic<bool> g_capabilities_initialized{false};
FramebufferApi g_framebuffer_api = FramebufferApi::unavailable;
std::array<char, 64> g_open_gl_version{};
std::array<GLint, 2> g_max_viewport_dimensions{};
GLint g_max_texture_size = 0;
GLint g_max_renderbuffer_size = 0;

[[nodiscard]] bool HasOpenGlProcedure(const char* name) noexcept {
    const PROC procedure = wglGetProcAddress(name);
    return procedure != nullptr &&
        procedure != reinterpret_cast<PROC>(1) &&
        procedure != reinterpret_cast<PROC>(2) &&
        procedure != reinterpret_cast<PROC>(3) &&
        procedure != reinterpret_cast<PROC>(-1);
}

[[nodiscard]] bool HasFramebufferProcedures(const char* suffix) noexcept {
    constexpr std::array<const char*, 10> kNames{
        "glGenFramebuffers",
        "glDeleteFramebuffers",
        "glBindFramebuffer",
        "glCheckFramebufferStatus",
        "glFramebufferTexture2D",
        "glFramebufferRenderbuffer",
        "glGenRenderbuffers",
        "glDeleteRenderbuffers",
        "glBindRenderbuffer",
        "glRenderbufferStorage",
    };

    std::array<char, 64> procedure_name{};
    for (const char* base_name : kNames) {
        if (sprintf_s(procedure_name.data(), procedure_name.size(), "%s%s", base_name, suffix) < 0 ||
            !HasOpenGlProcedure(procedure_name.data())) {
            return false;
        }
    }
    return true;
}

void InitializeOpenGlCapabilities() noexcept {
    if (g_capabilities_initialized.load(std::memory_order_acquire) ||
        wglGetCurrentContext() == nullptr) {
        return;
    }

    FramebufferApi framebuffer_api = FramebufferApi::unavailable;
    if (HasFramebufferProcedures("")) {
        framebuffer_api = FramebufferApi::core;
    } else if (HasFramebufferProcedures("EXT")) {
        framebuffer_api = FramebufferApi::ext;
    }

    std::array<char, 64> version{};
    const GLubyte* version_bytes = glGetString(GL_VERSION);
    if (version_bytes != nullptr) {
        strncpy_s(
            version.data(),
            version.size(),
            reinterpret_cast<const char*>(version_bytes),
            _TRUNCATE);
    }

    std::array<GLint, 2> max_viewport_dimensions{};
    GLint max_texture_size = 0;
    GLint max_renderbuffer_size = 0;
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_dimensions.data());
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(kGlMaxRenderbufferSize, &max_renderbuffer_size);

    AcquireSRWLockExclusive(&g_telemetry_lock);
    if (!g_capabilities_initialized.load(std::memory_order_relaxed)) {
        g_framebuffer_api = framebuffer_api;
        g_open_gl_version = version;
        g_max_viewport_dimensions = max_viewport_dimensions;
        g_max_texture_size = max_texture_size;
        g_max_renderbuffer_size = max_renderbuffer_size;
        g_capabilities_initialized.store(true, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

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

void __fastcall HookedRenderWorld(
    void* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) noexcept {
    ActiveCall active_call;
    InitializeOpenGlCapabilities();
    ProcessEyeTargetRequestsOnRenderThread();

    std::array<GLint, 4> viewport{};
    GLint framebuffer_binding = 0;
    const bool has_current_gl_context = wglGetCurrentContext() != nullptr;
    if (has_current_gl_context) {
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetIntegerv(kGlFramebufferBinding, &framebuffer_binding);
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.calls;
    g_telemetry.renderer = reinterpret_cast<std::uintptr_t>(renderer);
    g_telemetry.world = reinterpret_cast<std::uintptr_t>(world);
    g_telemetry.camera = reinterpret_cast<std::uintptr_t>(camera);
    g_telemetry.frame_time = frame_time;
    g_telemetry.has_current_gl_context = has_current_gl_context;
    g_telemetry.framebuffer_api = g_framebuffer_api;
    g_telemetry.viewport = {
        viewport[0], viewport[1], viewport[2], viewport[3],
    };
    g_telemetry.max_viewport_dimensions = {
        g_max_viewport_dimensions[0], g_max_viewport_dimensions[1],
    };
    g_telemetry.framebuffer_binding = framebuffer_binding;
    g_telemetry.max_texture_size = g_max_texture_size;
    g_telemetry.max_renderbuffer_size = g_max_renderbuffer_size;
    g_telemetry.open_gl_version = g_open_gl_version;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    const auto original = reinterpret_cast<RenderWorld>(
        g_original_target.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(renderer, world, camera, frame_time);
    }
}

[[nodiscard]] void* DecodeExpectedTarget(std::uint8_t* instruction) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, kExpectedCall.data() + 1, sizeof(displacement));
    const std::intptr_t next_instruction =
        reinterpret_cast<std::intptr_t>(instruction + kExpectedCall.size());
    return reinterpret_cast<void*>(next_instruction + displacement);
}

} // namespace

bool InstallRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed()) {
        error = "The Black Plague RenderWorld probe is already installed";
        return false;
    }

    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "GetModuleHandleW(NULL) failed";
        return false;
    }
    std::uint8_t* call_site = image + kRenderWorldCallSiteRva;
    void* expected_target = image + kRenderWorldRva;
    if (DecodeExpectedTarget(call_site) != expected_target) {
        error = "The manifest call displacement does not target RenderWorld";
        return false;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_active_calls.store(0, std::memory_order_release);
    g_capabilities_initialized.store(false, std::memory_order_release);
    g_framebuffer_api = FramebufferApi::unavailable;
    g_open_gl_version = {};
    g_max_viewport_dimensions = {};
    g_max_texture_size = 0;
    g_max_renderbuffer_size = 0;
    ResetEyeTargetProbe();
    g_original_target.store(expected_target, std::memory_order_release);

    if (!hooks::InstallRel32CallHook(
            call_site,
            kExpectedCall,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        g_original_target.store(nullptr, std::memory_order_release);
        return false;
    }
    return true;
}

bool RemoveRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (!hooks::RemoveRel32CallHook(g_hook, error)) {
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original_target.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for an active RenderWorld probe call to finish";
    return false;
}

RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    RenderWorldFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    result.eye_targets = ConsumeEyeTargetProbeTelemetry();
    return result;
}

} // namespace penumbra_vr::backends::black_plague
