#include "opengl_enhanced_eye_stage.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <cstdio>
#include <limits>
#include <string_view>

namespace penumbra_vr::graphics {
namespace {

constexpr GLenum kFramebuffer = 0x8D40;
constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kDrawFramebuffer = 0x8CA9;
constexpr GLenum kFramebufferBinding = 0x8CA6;
constexpr GLenum kColorAttachment0 = 0x8CE0;
constexpr GLenum kDepthStencilAttachment = 0x821A;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kRenderbuffer = 0x8D41;
constexpr GLenum kRenderbufferBinding = 0x8CA7;
constexpr GLenum kDepth24Stencil8 = 0x88F0;
constexpr GLenum kRgba16f = 0x881A;
constexpr GLenum kMaxRenderbufferSize = 0x84E8;
constexpr GLenum kMaxSamples = 0x8D57;
constexpr GLenum kClampToEdge = 0x812F;
constexpr GLenum kVertexShader = 0x8B31;
constexpr GLenum kFragmentShader = 0x8B30;
constexpr GLenum kCompileStatus = 0x8B81;
constexpr GLenum kLinkStatus = 0x8B82;
constexpr GLenum kInfoLogLength = 0x8B84;
constexpr GLenum kCurrentProgram = 0x8B8D;
constexpr GLenum kActiveTexture = 0x84E0;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kVertexProgramArb = 0x8620;
constexpr GLenum kFragmentProgramArb = 0x8804;

using GenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using BindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using CheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using FramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using FramebufferRenderbuffer = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
using GenRenderbuffers = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteRenderbuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using BindRenderbuffer = void(APIENTRY*)(GLenum, GLuint);
using RenderbufferStorageMultisample = void(APIENTRY*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
using BlitFramebuffer = void(APIENTRY*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
using CreateShader = GLuint(APIENTRY*)(GLenum);
using ShaderSource = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using CompileShader = void(APIENTRY*)(GLuint);
using GetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using DeleteShader = void(APIENTRY*)(GLuint);
using CreateProgram = GLuint(APIENTRY*)();
using AttachShader = void(APIENTRY*)(GLuint, GLuint);
using LinkProgram = void(APIENTRY*)(GLuint);
using GetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using DeleteProgram = void(APIENTRY*)(GLuint);
using UseProgram = void(APIENTRY*)(GLuint);
using GetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
using Uniform1i = void(APIENTRY*)(GLint, GLint);
using Uniform2f = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using ActiveTexture = void(APIENTRY*)(GLenum);

struct Api {
    GenFramebuffers gen_framebuffers = nullptr;
    DeleteFramebuffers delete_framebuffers = nullptr;
    BindFramebuffer bind_framebuffer = nullptr;
    CheckFramebufferStatus check_framebuffer_status = nullptr;
    FramebufferTexture2D framebuffer_texture_2d = nullptr;
    FramebufferRenderbuffer framebuffer_renderbuffer = nullptr;
    GenRenderbuffers gen_renderbuffers = nullptr;
    DeleteRenderbuffers delete_renderbuffers = nullptr;
    BindRenderbuffer bind_renderbuffer = nullptr;
    RenderbufferStorageMultisample renderbuffer_storage_multisample = nullptr;
    BlitFramebuffer blit_framebuffer = nullptr;
    CreateShader create_shader = nullptr;
    ShaderSource shader_source = nullptr;
    CompileShader compile_shader = nullptr;
    GetShaderiv get_shader_iv = nullptr;
    GetShaderInfoLog get_shader_info_log = nullptr;
    DeleteShader delete_shader = nullptr;
    CreateProgram create_program = nullptr;
    AttachShader attach_shader = nullptr;
    LinkProgram link_program = nullptr;
    GetProgramiv get_program_iv = nullptr;
    GetProgramInfoLog get_program_info_log = nullptr;
    DeleteProgram delete_program = nullptr;
    UseProgram use_program = nullptr;
    GetUniformLocation get_uniform_location = nullptr;
    Uniform1i uniform_1i = nullptr;
    Uniform2f uniform_2f = nullptr;
    ActiveTexture active_texture = nullptr;
};

[[nodiscard]] bool ValidProc(PROC proc) noexcept {
    return proc != nullptr && proc != reinterpret_cast<PROC>(1) &&
        proc != reinterpret_cast<PROC>(2) && proc != reinterpret_cast<PROC>(3) &&
        proc != reinterpret_cast<PROC>(-1);
}

template <typename T>
[[nodiscard]] T Proc(const char* core, const char* extension = nullptr) noexcept {
    PROC proc = wglGetProcAddress(core);
    if (!ValidProc(proc) && extension != nullptr) proc = wglGetProcAddress(extension);
    return ValidProc(proc) ? reinterpret_cast<T>(proc) : nullptr;
}

[[nodiscard]] bool LoadApi(Api& api, std::string& error) noexcept {
    if (wglGetCurrentContext() == nullptr) {
        error = "Enhanced eye stage requires a current OpenGL context";
        return false;
    }
    api.gen_framebuffers = Proc<GenFramebuffers>("glGenFramebuffers", "glGenFramebuffersEXT");
    api.delete_framebuffers = Proc<DeleteFramebuffers>("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    api.bind_framebuffer = Proc<BindFramebuffer>("glBindFramebuffer", "glBindFramebufferEXT");
    api.check_framebuffer_status = Proc<CheckFramebufferStatus>("glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
    api.framebuffer_texture_2d = Proc<FramebufferTexture2D>("glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    api.framebuffer_renderbuffer = Proc<FramebufferRenderbuffer>("glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
    api.gen_renderbuffers = Proc<GenRenderbuffers>("glGenRenderbuffers", "glGenRenderbuffersEXT");
    api.delete_renderbuffers = Proc<DeleteRenderbuffers>("glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
    api.bind_renderbuffer = Proc<BindRenderbuffer>("glBindRenderbuffer", "glBindRenderbufferEXT");
    api.renderbuffer_storage_multisample = Proc<RenderbufferStorageMultisample>(
        "glRenderbufferStorageMultisample", "glRenderbufferStorageMultisampleEXT");
    api.blit_framebuffer = Proc<BlitFramebuffer>("glBlitFramebuffer", "glBlitFramebufferEXT");
    api.create_shader = Proc<CreateShader>("glCreateShader");
    api.shader_source = Proc<ShaderSource>("glShaderSource");
    api.compile_shader = Proc<CompileShader>("glCompileShader");
    api.get_shader_iv = Proc<GetShaderiv>("glGetShaderiv");
    api.get_shader_info_log = Proc<GetShaderInfoLog>("glGetShaderInfoLog");
    api.delete_shader = Proc<DeleteShader>("glDeleteShader");
    api.create_program = Proc<CreateProgram>("glCreateProgram");
    api.attach_shader = Proc<AttachShader>("glAttachShader");
    api.link_program = Proc<LinkProgram>("glLinkProgram");
    api.get_program_iv = Proc<GetProgramiv>("glGetProgramiv");
    api.get_program_info_log = Proc<GetProgramInfoLog>("glGetProgramInfoLog");
    api.delete_program = Proc<DeleteProgram>("glDeleteProgram");
    api.use_program = Proc<UseProgram>("glUseProgram");
    api.get_uniform_location = Proc<GetUniformLocation>("glGetUniformLocation");
    api.uniform_1i = Proc<Uniform1i>("glUniform1i");
    api.uniform_2f = Proc<Uniform2f>("glUniform2f");
    api.active_texture = Proc<ActiveTexture>("glActiveTexture", "glActiveTextureARB");
    if (!api.gen_framebuffers || !api.delete_framebuffers || !api.bind_framebuffer ||
        !api.check_framebuffer_status || !api.framebuffer_texture_2d ||
        !api.framebuffer_renderbuffer || !api.gen_renderbuffers ||
        !api.delete_renderbuffers || !api.bind_renderbuffer ||
        !api.renderbuffer_storage_multisample || !api.blit_framebuffer ||
        !api.create_shader || !api.shader_source || !api.compile_shader ||
        !api.get_shader_iv || !api.get_shader_info_log || !api.delete_shader ||
        !api.create_program || !api.attach_shader || !api.link_program ||
        !api.get_program_iv || !api.get_program_info_log || !api.delete_program ||
        !api.use_program || !api.get_uniform_location || !api.uniform_1i ||
        !api.uniform_2f || !api.active_texture) {
        error = "Enhanced eye stage requires framebuffer blit, 2x MSAA and GLSL APIs";
        return false;
    }
    return true;
}

constexpr std::string_view kVertexShaderSource = R"GLSL(#version 120
varying vec2 vUv;
void main() {
    gl_Position = gl_Vertex;
    vUv = gl_MultiTexCoord0.xy;
}
)GLSL";

constexpr std::string_view kFragmentShaderSource = R"GLSL(#version 120
uniform sampler2D sceneMap;
uniform vec2 texelSize;
varying vec2 vUv;
void main() {
    vec4 center = texture2D(sceneMap, vUv);
    vec3 east = texture2D(sceneMap, vUv + vec2( texelSize.x, 0.0)).xyz;
    vec3 west = texture2D(sceneMap, vUv + vec2(-texelSize.x, 0.0)).xyz;
    vec3 north = texture2D(sceneMap, vUv + vec2(0.0,  texelSize.y)).xyz;
    vec3 south = texture2D(sceneMap, vUv + vec2(0.0, -texelSize.y)).xyz;
    vec3 neighbours = (east + west + north + south) * 0.25;
    vec3 localMin = min(center.xyz, min(min(east, west), min(north, south)));
    vec3 localMax = max(center.xyz, max(max(east, west), max(north, south)));
    vec3 color = max(clamp(center.xyz + (center.xyz - neighbours) * 0.65,
                           localMin, localMax), vec3(0.0));
    color *= 1.25;
    color = (color * (2.51 * color + 0.03)) /
            (color * (2.43 * color + 0.59) + 0.14);
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, 1.12);
    color = clamp((color - 0.5) * 1.08 + 0.5, 0.0, 1.0);
    color = pow(max(color, vec3(0.0)), vec3(0.94));
    gl_FragColor = vec4(clamp(color, 0.0, 1.0), center.a);
}
)GLSL";

[[nodiscard]] std::string ShaderLog(
    const Api& api, GLuint object, bool program) {
    GLint length = 0;
    if (program) api.get_program_iv(object, kInfoLogLength, &length);
    else api.get_shader_iv(object, kInfoLogLength, &length);
    if (length <= 1) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    if (program) api.get_program_info_log(object, length, &written, result.data());
    else api.get_shader_info_log(object, length, &written, result.data());
    result.resize(written > 0 ? static_cast<std::size_t>(written) : 0U);
    return result;
}

[[nodiscard]] GLuint CompileShaderObject(
    const Api& api, GLenum type, std::string_view source, std::string& error) {
    const GLuint shader = api.create_shader(type);
    if (shader == 0) {
        error = "OpenGL did not allocate an enhanced-eye shader";
        return 0;
    }
    const char* text = source.data();
    const GLint length = static_cast<GLint>(source.size());
    api.shader_source(shader, 1, &text, &length);
    api.compile_shader(shader);
    GLint compiled = GL_FALSE;
    api.get_shader_iv(shader, kCompileStatus, &compiled);
    if (compiled == GL_FALSE) {
        error = "Enhanced-eye shader compilation failed: " + ShaderLog(api, shader, false);
        api.delete_shader(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint CreateProgramObject(const Api& api, std::string& error) {
    const GLuint vertex = CompileShaderObject(api, kVertexShader, kVertexShaderSource, error);
    if (vertex == 0) return 0;
    const GLuint fragment = CompileShaderObject(api, kFragmentShader, kFragmentShaderSource, error);
    if (fragment == 0) {
        api.delete_shader(vertex);
        return 0;
    }
    const GLuint program = api.create_program();
    if (program == 0) {
        api.delete_shader(vertex);
        api.delete_shader(fragment);
        error = "OpenGL did not allocate an enhanced-eye program";
        return 0;
    }
    api.attach_shader(program, vertex);
    api.attach_shader(program, fragment);
    api.link_program(program);
    api.delete_shader(vertex);
    api.delete_shader(fragment);
    GLint linked = GL_FALSE;
    api.get_program_iv(program, kLinkStatus, &linked);
    if (linked == GL_FALSE) {
        error = "Enhanced-eye program link failed: " + ShaderLog(api, program, true);
        api.delete_program(program);
        return 0;
    }
    return program;
}

void DeleteEyeResources(const Api& api, OpenGlEnhancedEyeStage::EyeResources& eye) noexcept {
    if (eye.multisample_framebuffer) api.delete_framebuffers(1, &eye.multisample_framebuffer);
    if (eye.multisample_color) api.delete_renderbuffers(1, &eye.multisample_color);
    if (eye.multisample_depth_stencil) api.delete_renderbuffers(1, &eye.multisample_depth_stencil);
    if (eye.hdr_framebuffer) api.delete_framebuffers(1, &eye.hdr_framebuffer);
    if (eye.hdr_texture) glDeleteTextures(1, &eye.hdr_texture);
    eye = {};
}

[[nodiscard]] bool CreateEyeResources(
    const Api& api,
    GLsizei width,
    GLsizei height,
    OpenGlEnhancedEyeStage::EyeResources& eye,
    std::string& error) noexcept {
    glGenTextures(1, &eye.hdr_texture);
    if (!eye.hdr_texture) {
        error = "OpenGL did not allocate the enhanced-eye HDR texture";
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, eye.hdr_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(kRgba16f), width, height,
                 0, GL_RGBA, GL_FLOAT, nullptr);

    api.gen_framebuffers(1, &eye.hdr_framebuffer);
    api.bind_framebuffer(kFramebuffer, eye.hdr_framebuffer);
    api.framebuffer_texture_2d(
        kFramebuffer, kColorAttachment0, GL_TEXTURE_2D, eye.hdr_texture, 0);
    if (!eye.hdr_framebuffer || api.check_framebuffer_status(kFramebuffer) != kFramebufferComplete) {
        error = "Enhanced-eye RGBA16F resolve framebuffer is incomplete";
        return false;
    }

    api.gen_framebuffers(1, &eye.multisample_framebuffer);
    api.bind_framebuffer(kFramebuffer, eye.multisample_framebuffer);
    api.gen_renderbuffers(1, &eye.multisample_color);
    api.bind_renderbuffer(kRenderbuffer, eye.multisample_color);
    api.renderbuffer_storage_multisample(kRenderbuffer, 2, kRgba16f, width, height);
    api.framebuffer_renderbuffer(
        kFramebuffer, kColorAttachment0, kRenderbuffer, eye.multisample_color);
    api.gen_renderbuffers(1, &eye.multisample_depth_stencil);
    api.bind_renderbuffer(kRenderbuffer, eye.multisample_depth_stencil);
    api.renderbuffer_storage_multisample(
        kRenderbuffer, 2, kDepth24Stencil8, width, height);
    api.framebuffer_renderbuffer(kFramebuffer, kDepthStencilAttachment,
                                 kRenderbuffer, eye.multisample_depth_stencil);
    if (!eye.multisample_framebuffer || !eye.multisample_color ||
        !eye.multisample_depth_stencil ||
        api.check_framebuffer_status(kFramebuffer) != kFramebufferComplete) {
        error = "Enhanced-eye 2x MSAA framebuffer is incomplete";
        return false;
    }
    return true;
}

[[nodiscard]] std::size_t EyeIndex(Eye eye) noexcept {
    return eye == Eye::left ? 0U : 1U;
}

} // namespace

OpenGlEnhancedEyeStage::~OpenGlEnhancedEyeStage() noexcept {
    if (ready() && context_ == wglGetCurrentContext()) {
        std::string ignored;
        static_cast<void>(Destroy(ignored));
    }
}

bool OpenGlEnhancedEyeStage::CreateOrResize(
    std::uint32_t width, std::uint32_t height, std::string& error) noexcept {
    error.clear();
    Api api;
    if (!LoadApi(api, error)) return false;
    const HGLRC current = wglGetCurrentContext();
    if (context_ != nullptr && context_ != current) {
        error = "Enhanced eye resources belong to a different OpenGL context";
        return false;
    }
    if (width == 0 || height == 0 ||
        width > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max())) {
        error = "Enhanced eye dimensions must be positive GLsizei values";
        return false;
    }
    GLint max_texture = 0;
    GLint max_renderbuffer = 0;
    GLint max_samples = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    glGetIntegerv(kMaxRenderbufferSize, &max_renderbuffer);
    glGetIntegerv(kMaxSamples, &max_samples);
    if (max_samples < 2) {
        error = "Enhanced eye stage requires at least 2x framebuffer multisampling";
        return false;
    }
    if (width > static_cast<std::uint32_t>(max_texture) ||
        height > static_cast<std::uint32_t>(max_texture) ||
        width > static_cast<std::uint32_t>(max_renderbuffer) ||
        height > static_cast<std::uint32_t>(max_renderbuffer)) {
        error = "Enhanced eye dimensions exceed the current OpenGL limits";
        return false;
    }
    if (ready() && width_ == width && height_ == height) return true;

    GLint previous_framebuffer = 0;
    GLint previous_renderbuffer = 0;
    GLint previous_texture = 0;
    glGetIntegerv(kFramebufferBinding, &previous_framebuffer);
    glGetIntegerv(kRenderbufferBinding, &previous_renderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

    std::array<EyeResources, 2> replacements{};
    GLuint replacement_program = CreateProgramObject(api, error);
    bool created = replacement_program != 0;
    for (auto& eye : replacements) {
        if (created) created = CreateEyeResources(
            api, static_cast<GLsizei>(width), static_cast<GLsizei>(height), eye, error);
    }
    api.bind_framebuffer(kFramebuffer, static_cast<GLuint>(previous_framebuffer));
    api.bind_renderbuffer(kRenderbuffer, static_cast<GLuint>(previous_renderbuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    if (!created) {
        for (auto& eye : replacements) DeleteEyeResources(api, eye);
        if (replacement_program) api.delete_program(replacement_program);
        return false;
    }

    if (program_) api.delete_program(program_);
    for (auto& eye : eyes_) DeleteEyeResources(api, eye);
    eyes_ = replacements;
    program_ = replacement_program;
    context_ = current;
    width_ = width;
    height_ = height;
    return true;
}

bool OpenGlEnhancedEyeStage::Destroy(std::string& error) noexcept {
    error.clear();
    if (!context_) return true;
    Api api;
    if (!LoadApi(api, error)) return false;
    if (context_ != wglGetCurrentContext()) {
        error = "Enhanced eye resources can only be destroyed from their owning context";
        return false;
    }
    for (auto& eye : eyes_) DeleteEyeResources(api, eye);
    if (program_) api.delete_program(program_);
    program_ = 0;
    context_ = nullptr;
    width_ = 0;
    height_ = 0;
    return true;
}

bool OpenGlEnhancedEyeStage::BeginEye(
    Eye eye, OpenGlEnhancedEyeBinding& binding, std::string& error) const noexcept {
    error.clear();
    if (!ready() || context_ != wglGetCurrentContext()) {
        error = "Enhanced eye stage is not ready in the current OpenGL context";
        return false;
    }
    if (binding.active) {
        error = "Enhanced eye binding is already active";
        return false;
    }
    Api api;
    if (!LoadApi(api, error)) return false;
    GLint framebuffer = 0;
    std::array<GLint, 4> viewport{};
    glGetIntegerv(kFramebufferBinding, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    binding.context = wglGetCurrentContext();
    binding.output_framebuffer = framebuffer;
    binding.output_viewport = viewport;
    binding.eye = eye;
    binding.active = true;
    api.bind_framebuffer(kFramebuffer, eyes_[EyeIndex(eye)].multisample_framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    glDisable(GL_SCISSOR_TEST);
    return true;
}

bool OpenGlEnhancedEyeStage::EndEye(
    OpenGlEnhancedEyeBinding& binding, std::string& error) const noexcept {
    error.clear();
    if (!binding.active || binding.context != wglGetCurrentContext() || !ready()) {
        error = "Enhanced eye binding is not active in its owning context";
        return false;
    }
    Api api;
    if (!LoadApi(api, error)) return false;
    const EyeResources& eye = eyes_[EyeIndex(binding.eye)];

    api.bind_framebuffer(kReadFramebuffer, eye.multisample_framebuffer);
    api.bind_framebuffer(kDrawFramebuffer, eye.hdr_framebuffer);
    api.blit_framebuffer(0, 0, static_cast<GLint>(width_), static_cast<GLint>(height_),
                         0, 0, static_cast<GLint>(width_), static_cast<GLint>(height_),
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
    api.bind_framebuffer(kFramebuffer, static_cast<GLuint>(binding.output_framebuffer));
    glViewport(binding.output_viewport[0], binding.output_viewport[1],
               binding.output_viewport[2], binding.output_viewport[3]);

    GLint previous_program = 0;
    GLint previous_active_texture = static_cast<GLint>(kTexture0);
    glGetIntegerv(kCurrentProgram, &previous_program);
    glGetIntegerv(kActiveTexture, &previous_active_texture);
    api.active_texture(kTexture0);
    GLint previous_texture0 = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture0);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(kVertexProgramArb);
    glDisable(kFragmentProgramArb);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, eye.hdr_texture);
    api.use_program(program_);
    const GLint scene = api.get_uniform_location(program_, "sceneMap");
    const GLint texel = api.get_uniform_location(program_, "texelSize");
    if (scene >= 0) api.uniform_1i(scene, 0);
    if (texel >= 0) api.uniform_2f(
        texel, 1.0F / static_cast<float>(width_), 1.0F / static_cast<float>(height_));
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex4f(-1, -1, 0, 1);
    glTexCoord2f(1, 0); glVertex4f( 1, -1, 0, 1);
    glTexCoord2f(1, 1); glVertex4f( 1,  1, 0, 1);
    glTexCoord2f(0, 1); glVertex4f(-1,  1, 0, 1);
    glEnd();
    api.use_program(0);
    glPopAttrib();
    api.active_texture(kTexture0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture0));
    api.active_texture(static_cast<GLenum>(previous_active_texture));
    api.use_program(static_cast<GLuint>(previous_program));
    binding = {};
    return true;
}

bool OpenGlEnhancedEyeStage::ready() const noexcept {
    return context_ != nullptr && width_ != 0 && height_ != 0 && program_ != 0 &&
        eyes_[0].multisample_framebuffer != 0 && eyes_[0].hdr_framebuffer != 0 &&
        eyes_[0].hdr_texture != 0 && eyes_[1].multisample_framebuffer != 0 &&
        eyes_[1].hdr_framebuffer != 0 && eyes_[1].hdr_texture != 0;
}

std::uint32_t OpenGlEnhancedEyeStage::width() const noexcept { return width_; }
std::uint32_t OpenGlEnhancedEyeStage::height() const noexcept { return height_; }

} // namespace penumbra_vr::graphics
