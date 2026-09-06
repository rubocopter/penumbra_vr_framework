#include "opengl_eye_targets.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <cstdio>
#include <limits>

namespace penumbra_vr::graphics {
namespace {

constexpr GLenum kGlFramebuffer = 0x8D40;
constexpr GLenum kGlRenderbuffer = 0x8D41;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlRenderbufferBinding = 0x8CA7;
constexpr GLenum kGlColorAttachment0 = 0x8CE0;
constexpr GLenum kGlDepthStencilAttachment = 0x821A;
constexpr GLenum kGlFramebufferComplete = 0x8CD5;
constexpr GLenum kGlDepth24Stencil8 = 0x88F0;
constexpr GLenum kGlRgba8 = 0x8058;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr GLenum kGlClampToEdge = 0x812F;

using GlGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using GlDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using GlBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using GlCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using GlFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using GlFramebufferRenderbuffer = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
using GlGenRenderbuffers = void(APIENTRY*)(GLsizei, GLuint*);
using GlDeleteRenderbuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using GlBindRenderbuffer = void(APIENTRY*)(GLenum, GLuint);
using GlRenderbufferStorage = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);

struct FramebufferFunctions {
    GlGenFramebuffers gen_framebuffers = nullptr;
    GlDeleteFramebuffers delete_framebuffers = nullptr;
    GlBindFramebuffer bind_framebuffer = nullptr;
    GlCheckFramebufferStatus check_framebuffer_status = nullptr;
    GlFramebufferTexture2D framebuffer_texture_2d = nullptr;
    GlFramebufferRenderbuffer framebuffer_renderbuffer = nullptr;
    GlGenRenderbuffers gen_renderbuffers = nullptr;
    GlDeleteRenderbuffers delete_renderbuffers = nullptr;
    GlBindRenderbuffer bind_renderbuffer = nullptr;
    GlRenderbufferStorage renderbuffer_storage = nullptr;
};

[[nodiscard]] bool IsValidProcedure(PROC procedure) noexcept {
    return procedure != nullptr &&
        procedure != reinterpret_cast<PROC>(1) &&
        procedure != reinterpret_cast<PROC>(2) &&
        procedure != reinterpret_cast<PROC>(3) &&
        procedure != reinterpret_cast<PROC>(-1);
}

template <typename Procedure>
[[nodiscard]] bool LoadProcedure(
    const char* base_name,
    const char* suffix,
    Procedure& destination) noexcept {
    std::array<char, 64> name{};
    if (sprintf_s(name.data(), name.size(), "%s%s", base_name, suffix) < 0) {
        return false;
    }
    const PROC procedure = wglGetProcAddress(name.data());
    if (!IsValidProcedure(procedure)) {
        return false;
    }
    destination = reinterpret_cast<Procedure>(procedure);
    return true;
}

[[nodiscard]] bool LoadWithSuffix(
    const char* suffix,
    FramebufferFunctions& functions) noexcept {
    FramebufferFunctions candidate;
    if (!LoadProcedure("glGenFramebuffers", suffix, candidate.gen_framebuffers) ||
        !LoadProcedure("glDeleteFramebuffers", suffix, candidate.delete_framebuffers) ||
        !LoadProcedure("glBindFramebuffer", suffix, candidate.bind_framebuffer) ||
        !LoadProcedure("glCheckFramebufferStatus", suffix, candidate.check_framebuffer_status) ||
        !LoadProcedure("glFramebufferTexture2D", suffix, candidate.framebuffer_texture_2d) ||
        !LoadProcedure("glFramebufferRenderbuffer", suffix, candidate.framebuffer_renderbuffer) ||
        !LoadProcedure("glGenRenderbuffers", suffix, candidate.gen_renderbuffers) ||
        !LoadProcedure("glDeleteRenderbuffers", suffix, candidate.delete_renderbuffers) ||
        !LoadProcedure("glBindRenderbuffer", suffix, candidate.bind_renderbuffer) ||
        !LoadProcedure("glRenderbufferStorage", suffix, candidate.renderbuffer_storage)) {
        return false;
    }
    functions = candidate;
    return true;
}

[[nodiscard]] bool LoadFramebufferFunctions(
    FramebufferFunctions& functions,
    std::string& error) noexcept {
    if (wglGetCurrentContext() == nullptr) {
        error = "No OpenGL context is current on this thread";
        return false;
    }
    if (LoadWithSuffix("", functions) || LoadWithSuffix("EXT", functions)) {
        return true;
    }
    error = "Neither the core nor EXT framebuffer-object API is complete";
    return false;
}

void DeleteTarget(
    const FramebufferFunctions& functions,
    OpenGlEyeTarget& target) noexcept {
    if (target.framebuffer != 0) {
        const GLuint object = target.framebuffer;
        functions.delete_framebuffers(1, &object);
    }
    if (target.depth_stencil_renderbuffer != 0) {
        const GLuint object = target.depth_stencil_renderbuffer;
        functions.delete_renderbuffers(1, &object);
    }
    if (target.color_texture != 0) {
        const GLuint object = target.color_texture;
        glDeleteTextures(1, &object);
    }
    target = {};
}

[[nodiscard]] bool CreateTarget(
    const FramebufferFunctions& functions,
    GLsizei width,
    GLsizei height,
    OpenGlEyeTarget& target,
    std::string& error) noexcept {
    GLuint color_texture = 0;
    glGenTextures(1, &color_texture);
    target.color_texture = color_texture;
    if (color_texture == 0) {
        error = "OpenGL did not allocate a color texture";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        static_cast<GLint>(kGlRgba8),
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    GLuint framebuffer = 0;
    functions.gen_framebuffers(1, &framebuffer);
    target.framebuffer = framebuffer;
    if (framebuffer == 0) {
        error = "OpenGL did not allocate a framebuffer";
        return false;
    }
    functions.bind_framebuffer(kGlFramebuffer, framebuffer);
    functions.framebuffer_texture_2d(
        kGlFramebuffer,
        kGlColorAttachment0,
        GL_TEXTURE_2D,
        color_texture,
        0);

    GLuint renderbuffer = 0;
    functions.gen_renderbuffers(1, &renderbuffer);
    target.depth_stencil_renderbuffer = renderbuffer;
    if (renderbuffer == 0) {
        error = "OpenGL did not allocate a depth-stencil renderbuffer";
        return false;
    }
    functions.bind_renderbuffer(kGlRenderbuffer, renderbuffer);
    functions.renderbuffer_storage(
        kGlRenderbuffer,
        kGlDepth24Stencil8,
        width,
        height);
    functions.framebuffer_renderbuffer(
        kGlFramebuffer,
        kGlDepthStencilAttachment,
        kGlRenderbuffer,
        renderbuffer);

    const GLenum status = functions.check_framebuffer_status(kGlFramebuffer);
    if (status != kGlFramebufferComplete) {
        std::array<char, 96> message{};
        sprintf_s(
            message.data(),
            message.size(),
            "Eye framebuffer is incomplete (status 0x%04X)",
            static_cast<unsigned int>(status));
        error = message.data();
        return false;
    }
    return true;
}

[[nodiscard]] std::size_t EyeIndex(Eye eye) noexcept {
    return eye == Eye::left ? 0U : 1U;
}

} // namespace

OpenGlEyeTargets::~OpenGlEyeTargets() noexcept {
    if (ready() && context_ == wglGetCurrentContext()) {
        std::string ignored_error;
        static_cast<void>(Destroy(ignored_error));
    }
}

bool OpenGlEyeTargets::CreateOrResize(
    std::uint32_t width,
    std::uint32_t height,
    std::string& error) noexcept {
    error.clear();
    FramebufferFunctions functions;
    if (!LoadFramebufferFunctions(functions, error)) {
        return false;
    }

    HGLRC current_context = wglGetCurrentContext();
    if (context_ != nullptr && context_ != current_context) {
        error = "Eye targets belong to a different OpenGL context";
        return false;
    }
    if (width == 0 || height == 0 ||
        width > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max())) {
        error = "Eye target dimensions must be positive GLsizei values";
        return false;
    }
    if (ready() && width == width_ && height == height_) {
        return true;
    }

    GLint max_texture_size = 0;
    GLint max_renderbuffer_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(kGlMaxRenderbufferSize, &max_renderbuffer_size);
    if (max_texture_size <= 0 || max_renderbuffer_size <= 0) {
        error = "OpenGL returned invalid framebuffer size limits";
        return false;
    }
    if (width > static_cast<std::uint32_t>(max_texture_size) ||
        height > static_cast<std::uint32_t>(max_texture_size) ||
        width > static_cast<std::uint32_t>(max_renderbuffer_size) ||
        height > static_cast<std::uint32_t>(max_renderbuffer_size)) {
        error = "Eye target dimensions exceed the current OpenGL implementation limits";
        return false;
    }

    GLint previous_framebuffer = 0;
    GLint previous_renderbuffer = 0;
    GLint previous_texture = 0;
    glGetIntegerv(kGlFramebufferBinding, &previous_framebuffer);
    glGetIntegerv(kGlRenderbufferBinding, &previous_renderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    if (OwnsFramebuffer(static_cast<std::uint32_t>(previous_framebuffer)) ||
        OwnsRenderbuffer(static_cast<std::uint32_t>(previous_renderbuffer)) ||
        OwnsTexture(static_cast<std::uint32_t>(previous_texture))) {
        error = "End the active eye binding before resizing eye targets";
        return false;
    }

    std::array<OpenGlEyeTarget, 2> replacements{};
    const GLsizei gl_width = static_cast<GLsizei>(width);
    const GLsizei gl_height = static_cast<GLsizei>(height);
    bool created = CreateTarget(
        functions, gl_width, gl_height, replacements[0], error);
    if (created) {
        created = CreateTarget(
            functions, gl_width, gl_height, replacements[1], error);
    }

    functions.bind_framebuffer(
        kGlFramebuffer, static_cast<GLuint>(previous_framebuffer));
    functions.bind_renderbuffer(
        kGlRenderbuffer, static_cast<GLuint>(previous_renderbuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));

    if (!created) {
        DeleteTarget(functions, replacements[0]);
        DeleteTarget(functions, replacements[1]);
        return false;
    }

    for (OpenGlEyeTarget& old_target : targets_) {
        DeleteTarget(functions, old_target);
    }
    targets_ = replacements;
    context_ = current_context;
    width_ = width;
    height_ = height;
    return true;
}

bool OpenGlEyeTargets::Destroy(std::string& error) noexcept {
    error.clear();
    if (!ready()) {
        context_ = nullptr;
        width_ = 0;
        height_ = 0;
        return true;
    }

    FramebufferFunctions functions;
    if (!LoadFramebufferFunctions(functions, error)) {
        return false;
    }
    if (context_ != wglGetCurrentContext()) {
        error = "Eye targets can only be destroyed from their owning OpenGL context";
        return false;
    }

    GLint framebuffer = 0;
    GLint renderbuffer = 0;
    GLint texture = 0;
    glGetIntegerv(kGlFramebufferBinding, &framebuffer);
    glGetIntegerv(kGlRenderbufferBinding, &renderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    if (OwnsFramebuffer(static_cast<std::uint32_t>(framebuffer)) ||
        OwnsRenderbuffer(static_cast<std::uint32_t>(renderbuffer)) ||
        OwnsTexture(static_cast<std::uint32_t>(texture))) {
        error = "End the active eye binding before destroying eye targets";
        return false;
    }

    for (OpenGlEyeTarget& target_value : targets_) {
        DeleteTarget(functions, target_value);
    }
    context_ = nullptr;
    width_ = 0;
    height_ = 0;
    return true;
}

bool OpenGlEyeTargets::BeginEye(
    Eye eye,
    OpenGlEyeBinding& binding,
    std::string& error) const noexcept {
    error.clear();
    if (!ready()) {
        error = "Eye targets have not been created";
        return false;
    }
    if (binding.active) {
        error = "This eye binding is already active";
        return false;
    }

    FramebufferFunctions functions;
    if (!LoadFramebufferFunctions(functions, error)) {
        return false;
    }
    HGLRC current_context = wglGetCurrentContext();
    if (context_ != current_context) {
        error = "Eye targets can only be bound from their owning OpenGL context";
        return false;
    }

    GLint previous_framebuffer = 0;
    std::array<GLint, 4> previous_viewport{};
    glGetIntegerv(kGlFramebufferBinding, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport.data());

    const OpenGlEyeTarget& eye_target = targets_[EyeIndex(eye)];
    functions.bind_framebuffer(kGlFramebuffer, eye_target.framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));

    GLint current_framebuffer = 0;
    glGetIntegerv(kGlFramebufferBinding, &current_framebuffer);
    if (static_cast<std::uint32_t>(current_framebuffer) != eye_target.framebuffer) {
        functions.bind_framebuffer(
            kGlFramebuffer, static_cast<GLuint>(previous_framebuffer));
        glViewport(
            previous_viewport[0],
            previous_viewport[1],
            previous_viewport[2],
            previous_viewport[3]);
        error = "OpenGL did not retain the requested eye framebuffer binding";
        return false;
    }

    binding.context = current_context;
    binding.previous_framebuffer = previous_framebuffer;
    binding.previous_viewport = {
        previous_viewport[0], previous_viewport[1],
        previous_viewport[2], previous_viewport[3],
    };
    binding.active = true;
    glGetIntegerv(GL_SCISSOR_BOX, binding.previous_scissor.data());
    binding.previous_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    // A leftover desktop/light rectangle must not clip this eye's clear.
    glDisable(GL_SCISSOR_TEST);
    glScissor(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    return true;
}

bool OpenGlEyeTargets::EndEye(
    OpenGlEyeBinding& binding,
    std::string& error) const noexcept {
    error.clear();
    if (!binding.active) {
        error = "The eye binding is not active";
        return false;
    }

    FramebufferFunctions functions;
    if (!LoadFramebufferFunctions(functions, error)) {
        return false;
    }
    if (binding.context != wglGetCurrentContext()) {
        error = "The eye binding must be restored on the context that created it";
        return false;
    }

    functions.bind_framebuffer(
        kGlFramebuffer, static_cast<GLuint>(binding.previous_framebuffer));
    glViewport(
        binding.previous_viewport[0],
        binding.previous_viewport[1],
        binding.previous_viewport[2],
        binding.previous_viewport[3]);
    glScissor(binding.previous_scissor[0], binding.previous_scissor[1],
        binding.previous_scissor[2], binding.previous_scissor[3]);
    if (binding.previous_scissor_enabled) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    binding = {};
    return true;
}

bool OpenGlEyeTargets::ready() const noexcept {
    return context_ != nullptr && width_ != 0 && height_ != 0 &&
        targets_[0].framebuffer != 0 && targets_[0].color_texture != 0 &&
        targets_[0].depth_stencil_renderbuffer != 0 &&
        targets_[1].framebuffer != 0 && targets_[1].color_texture != 0 &&
        targets_[1].depth_stencil_renderbuffer != 0;
}

std::uint32_t OpenGlEyeTargets::width() const noexcept {
    return width_;
}

std::uint32_t OpenGlEyeTargets::height() const noexcept {
    return height_;
}

const OpenGlEyeTarget& OpenGlEyeTargets::target(Eye eye) const noexcept {
    return targets_[EyeIndex(eye)];
}

bool OpenGlEyeTargets::OwnsFramebuffer(std::uint32_t object) const noexcept {
    return object != 0 &&
        (targets_[0].framebuffer == object || targets_[1].framebuffer == object);
}

bool OpenGlEyeTargets::OwnsTexture(std::uint32_t object) const noexcept {
    return object != 0 &&
        (targets_[0].color_texture == object || targets_[1].color_texture == object);
}

bool OpenGlEyeTargets::OwnsRenderbuffer(std::uint32_t object) const noexcept {
    return object != 0 &&
        (targets_[0].depth_stencil_renderbuffer == object ||
         targets_[1].depth_stencil_renderbuffer == object);
}

} // namespace penumbra_vr::graphics
