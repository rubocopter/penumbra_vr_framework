#include "opengl_eye_targets.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

constexpr GLenum kGlFramebufferBinding = 0x8CA6;

class TestOpenGlContext final {
public:
    TestOpenGlContext() noexcept = default;
    TestOpenGlContext(const TestOpenGlContext&) = delete;
    TestOpenGlContext& operator=(const TestOpenGlContext&) = delete;

    ~TestOpenGlContext() {
        if (context_ != nullptr) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context_);
        }
        if (device_context_ != nullptr && window_ != nullptr) {
            ReleaseDC(window_, device_context_);
        }
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
        if (class_registered_) {
            UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
        }
    }

    [[nodiscard]] bool Initialize(std::string& error) noexcept {
        WNDCLASSW window_class{};
        window_class.style = CS_OWNDC;
        window_class.lpfnWndProc = DefWindowProcW;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = kClassName;
        if (RegisterClassW(&window_class) == 0) {
            error = "RegisterClassW failed";
            return false;
        }
        class_registered_ = true;

        window_ = CreateWindowExW(
            0,
            kClassName,
            L"Penumbra VR OpenGL eye target test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            64,
            64,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (window_ == nullptr) {
            error = "CreateWindowExW failed";
            return false;
        }

        device_context_ = GetDC(window_);
        if (device_context_ == nullptr) {
            error = "GetDC failed";
            return false;
        }

        PIXELFORMATDESCRIPTOR descriptor{};
        descriptor.nSize = sizeof(descriptor);
        descriptor.nVersion = 1;
        descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        descriptor.iPixelType = PFD_TYPE_RGBA;
        descriptor.cColorBits = 32;
        descriptor.cDepthBits = 24;
        descriptor.cStencilBits = 8;
        descriptor.iLayerType = PFD_MAIN_PLANE;
        const int pixel_format = ChoosePixelFormat(device_context_, &descriptor);
        if (pixel_format == 0 || !SetPixelFormat(device_context_, pixel_format, &descriptor)) {
            error = "Could not choose and set an OpenGL pixel format";
            return false;
        }

        context_ = wglCreateContext(device_context_);
        if (context_ == nullptr || !wglMakeCurrent(device_context_, context_)) {
            error = "Could not create and activate a WGL context";
            return false;
        }
        return true;
    }

private:
    static constexpr const wchar_t* kClassName = L"PenumbraVREyeTargetsTestWindow";
    bool class_registered_ = false;
    HWND window_ = nullptr;
    HDC device_context_ = nullptr;
    HGLRC context_ = nullptr;
};

[[nodiscard]] bool ViewportEquals(const std::array<GLint, 4>& expected) noexcept {
    std::array<GLint, 4> actual{};
    glGetIntegerv(GL_VIEWPORT, actual.data());
    return actual == expected;
}

[[nodiscard]] std::uint32_t CurrentFramebuffer() noexcept {
    GLint framebuffer = 0;
    glGetIntegerv(kGlFramebufferBinding, &framebuffer);
    return static_cast<std::uint32_t>(framebuffer);
}

} // namespace

int main() {
    std::string error;
    TestOpenGlContext context;
    if (!context.Initialize(error)) {
        std::cerr << "OpenGL test context creation failed: " << error << '\n';
        return 1;
    }

    GLuint sentinel_texture = 0;
    glGenTextures(1, &sentinel_texture);
    glBindTexture(GL_TEXTURE_2D, sentinel_texture);
    const std::array<GLint, 4> original_viewport{7, 9, 101, 103};
    glViewport(
        original_viewport[0],
        original_viewport[1],
        original_viewport[2],
        original_viewport[3]);

    penumbra_vr::graphics::OpenGlEyeTargets targets;
    if (!targets.CreateOrResize(320, 240, error)) {
        std::cerr << "Initial eye target creation failed: " << error << '\n';
        return 2;
    }
    GLint texture_binding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
    if (!targets.ready() || targets.width() != 320 || targets.height() != 240 ||
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer == 0 ||
        targets.target(penumbra_vr::graphics::Eye::right).framebuffer == 0 ||
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer ==
            targets.target(penumbra_vr::graphics::Eye::right).framebuffer ||
        CurrentFramebuffer() != 0 ||
        texture_binding != static_cast<GLint>(sentinel_texture) ||
        !ViewportEquals(original_viewport)) {
        std::cerr << "Eye target creation did not preserve GL state or allocate two targets\n";
        return 3;
    }

    penumbra_vr::graphics::OpenGlEyeBinding outer_binding;
    if (!targets.BeginEye(
            penumbra_vr::graphics::Eye::left, outer_binding, error)) {
        std::cerr << "Left-eye binding failed: " << error << '\n';
        return 4;
    }
    const std::array<GLint, 4> eye_viewport{0, 0, 320, 240};
    if (CurrentFramebuffer() !=
            targets.target(penumbra_vr::graphics::Eye::left).framebuffer ||
        !ViewportEquals(eye_viewport)) {
        std::cerr << "Left-eye binding did not select its framebuffer and viewport\n";
        return 5;
    }
    if (targets.CreateOrResize(321, 241, error) ||
        targets.Destroy(error) || error.empty() ||
        CurrentFramebuffer() !=
            targets.target(penumbra_vr::graphics::Eye::left).framebuffer) {
        std::cerr << "An active eye target was resized or destroyed\n";
        return 6;
    }

    penumbra_vr::graphics::OpenGlEyeBinding inner_binding;
    if (!targets.BeginEye(
            penumbra_vr::graphics::Eye::right, inner_binding, error) ||
        !targets.EndEye(inner_binding, error)) {
        std::cerr << "Nested right-eye bind/restore failed: " << error << '\n';
        return 7;
    }
    if (CurrentFramebuffer() !=
            targets.target(penumbra_vr::graphics::Eye::left).framebuffer ||
        !ViewportEquals(eye_viewport)) {
        std::cerr << "Nested eye restoration did not return to the left eye\n";
        return 8;
    }
    if (!targets.EndEye(outer_binding, error) || CurrentFramebuffer() != 0 ||
        !ViewportEquals(original_viewport)) {
        std::cerr << "Outer eye restoration did not restore the caller state: "
                  << error << '\n';
        return 9;
    }

    const std::uint32_t old_left =
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer;
    const std::uint32_t old_right =
        targets.target(penumbra_vr::graphics::Eye::right).framebuffer;
    if (!targets.CreateOrResize(256, 192, error) ||
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer == old_left ||
        targets.target(penumbra_vr::graphics::Eye::right).framebuffer == old_right ||
        CurrentFramebuffer() != 0 || !ViewportEquals(original_viewport)) {
        std::cerr << "Transactional eye target resize failed: " << error << '\n';
        return 10;
    }

    const std::uint32_t resized_left =
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer;
    if (targets.CreateOrResize(0, 192, error) || error.empty() ||
        targets.target(penumbra_vr::graphics::Eye::left).framebuffer != resized_left) {
        std::cerr << "Invalid resize changed the live eye targets\n";
        return 11;
    }

    if (!targets.Destroy(error) || targets.ready() || CurrentFramebuffer() != 0 ||
        !ViewportEquals(original_viewport)) {
        std::cerr << "Eye target destruction failed: " << error << '\n';
        return 12;
    }
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
    if (texture_binding != static_cast<GLint>(sentinel_texture)) {
        std::cerr << "Eye target destruction changed the caller texture binding\n";
        return 13;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &sentinel_texture);
    std::cout << "OpenGL eye target allocation, resize and state restoration passed\n";
    return 0;
}
