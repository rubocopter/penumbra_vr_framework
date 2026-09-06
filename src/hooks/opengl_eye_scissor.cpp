#include "opengl_eye_scissor.hpp"
#include "iat_hook.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace penumbra_vr::hooks {
namespace {
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
using GlScissor = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
IatHook g_hook;
std::atomic<void*> g_original{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
thread_local ScopedEyeScissor* g_scope = nullptr;

void PublishOriginal(void* original) noexcept {
    g_original.store(original, std::memory_order_release);
}

void APIENTRY HookedScissor(GLint x, GLint y, GLsizei width, GLsizei height) noexcept {
    g_active_calls.fetch_add(1, std::memory_order_acq_rel);
    ScissorRect rect{x, y, width, height};
    if (g_scope != nullptr) {
        rect = g_scope->Remap(rect);
    }
    const auto original = reinterpret_cast<GlScissor>(
        g_original.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(rect[0], rect[1], rect[2], rect[3]);
    }
    g_active_calls.fetch_sub(1, std::memory_order_release);
}
} // namespace

bool MapEyeScissor(
    const ScissorRect& source, const std::array<std::int32_t, 2>& source_size,
    const ScissorRect& viewport, ScissorRect& result) noexcept {
    if (source_size[0] <= 0 || source_size[1] <= 0 ||
        viewport[2] <= 0 || viewport[3] <= 0 || source[2] < 0 || source[3] < 0) {
        return false;
    }
    ScissorRect mapped{};
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto target_end = static_cast<std::int64_t>(viewport[axis]) +
            viewport[axis + 2];
        if (target_end > std::numeric_limits<std::int32_t>::max()) {
            return false;
        }
        const double scale = static_cast<double>(viewport[axis + 2]) /
            source_size[axis];
        const double start = std::clamp(
            static_cast<double>(source[axis]) - 1.0,
            0.0, static_cast<double>(source_size[axis]));
        const double end = std::clamp(
            static_cast<double>(source[axis]) + source[axis + 2] + 1.0,
            0.0, static_cast<double>(source_size[axis]));
        const auto low = static_cast<std::int32_t>(std::clamp(
            std::floor(start * scale), 0.0, static_cast<double>(viewport[axis + 2])));
        const auto high = static_cast<std::int32_t>(std::clamp(
            std::ceil(end * scale), 0.0, static_cast<double>(viewport[axis + 2])));
        mapped[axis] = viewport[axis] + low;
        mapped[axis + 2] = source[axis + 2] == 0 ? 0 : high - low;
    }
    result = mapped;
    return true;
}

bool InstallOpenGlEyeScissor(std::string& error) noexcept {
    return InstallIatHook("OPENGL32.dll", "glScissor",
        reinterpret_cast<void*>(&HookedScissor), g_hook, error, &PublishOriginal);
}

bool RemoveOpenGlEyeScissor(std::string& error) noexcept {
    if (!RemoveIatHook(g_hook, error)) {
        return false;
    }
    for (unsigned elapsed = 0; elapsed < 2000; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            // Keep the original published for a caller that fetched the IAT
            // entry just before removal. The probe DLL stays loaded.
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for active eye-scissor calls";
    return false;
}

ScopedEyeScissor::ScopedEyeScissor(
    std::array<std::int32_t, 2> source_size) noexcept
    : previous_(g_scope), context_(wglGetCurrentContext()), source_size_(source_size) {
    if (context_ != nullptr) {
        glGetIntegerv(kGlFramebufferBinding, &framebuffer_);
        glGetIntegerv(GL_VIEWPORT, viewport_.data());
    }
    g_scope = this;
}

ScopedEyeScissor::~ScopedEyeScissor() noexcept {
    g_scope = previous_;
}

ScissorRect ScopedEyeScissor::Remap(const ScissorRect& source) noexcept {
    ScissorRect mapped{};
    if (context_ != nullptr && context_ == wglGetCurrentContext() && framebuffer_ != 0) {
        GLint framebuffer = 0;
        ScissorRect viewport{};
        glGetIntegerv(kGlFramebufferBinding, &framebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        if (framebuffer == framebuffer_ && viewport == viewport_ &&
            MapEyeScissor(source, source_size_, viewport_, mapped)) {
            ++remapped_;
            return mapped;
        }
    }
    ++bypassed_;
    return source;
}
} // namespace penumbra_vr::hooks
