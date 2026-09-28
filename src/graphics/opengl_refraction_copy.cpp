#include "opengl_refraction_copy.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

namespace penumbra_vr::graphics {

RefractionEyeCopyResult CaptureBoundRefractionEye(
    const RefractionEyeCopyContext& eye) noexcept {
    if (eye.context == nullptr || eye.context != wglGetCurrentContext())
        return RefractionEyeCopyResult::skipped;
    // The core/ARB FBO API exposes separate read and draw bindings. Avoid
    // querying unsupported read state on an EXT-only context; skip the adapter.
    const auto bind_framebuffer = wglGetProcAddress("glBindFramebuffer");
    if (bind_framebuffer == nullptr || bind_framebuffer == reinterpret_cast<PROC>(1) ||
        bind_framebuffer == reinterpret_cast<PROC>(2) ||
        bind_framebuffer == reinterpret_cast<PROC>(3) ||
        bind_framebuffer == reinterpret_cast<PROC>(-1))
        return RefractionEyeCopyResult::skipped;
    constexpr GLenum active_texture_enum = 0x84E0;
    constexpr GLint texture0 = 0x84C0;
    constexpr GLenum rectangle = 0x84F5;
    constexpr GLenum binding_rectangle = 0x84F6;
    constexpr GLenum max_rectangle_size_enum = 0x84F8;
    constexpr GLenum framebuffer_binding = 0x8CA6;
    constexpr GLenum read_framebuffer_binding = 0x8CAA;
    constexpr GLint rgba8 = 0x8058;
    GLint active_texture = 0;
    glGetIntegerv(active_texture_enum, &active_texture);
    if (active_texture != texture0) return RefractionEyeCopyResult::skipped;
    GLint framebuffer = 0, texture = 0, max_size = 0;
    std::array<GLint, 4> viewport{};
    glGetIntegerv(framebuffer_binding, &framebuffer);
    GLint read_framebuffer = 0;
    glGetIntegerv(read_framebuffer_binding, &read_framebuffer);
    if (read_framebuffer != eye.framebuffer)
        return RefractionEyeCopyResult::skipped;
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    glGetIntegerv(binding_rectangle, &texture);
    glGetIntegerv(max_rectangle_size_enum, &max_size);
    if (!RefractionEyeGeometryMatches({0, 0, true, eye.framebuffer, framebuffer,
            viewport, eye.eye_width, eye.eye_height,
            eye.native_width, eye.native_height,
            static_cast<std::uint32_t>(texture), max_size}))
        return RefractionEyeCopyResult::skipped;
    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(rectangle, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(rectangle, 0, GL_TEXTURE_HEIGHT, &height);
    if (width != eye.eye_width || height != eye.eye_height) {
        glCopyTexImage2D(rectangle, 0, rgba8, 0, 0,
            eye.eye_width, eye.eye_height, 0);
        return RefractionEyeCopyResult::resized;
    }
    glCopyTexSubImage2D(rectangle, 0, 0, 0, 0, 0,
        eye.eye_width, eye.eye_height);
    return RefractionEyeCopyResult::copied;
}

} // namespace penumbra_vr::graphics
