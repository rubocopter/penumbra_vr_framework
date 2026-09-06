#include "opengl_eye_targets.hpp"
#include "opengl_eye_scissor.hpp"
#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"

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

[[nodiscard]] bool ScissorEquals(const std::array<GLint, 4>& expected, bool enabled) noexcept {
    std::array<GLint, 4> actual{};
    glGetIntegerv(GL_SCISSOR_BOX, actual.data());
    return actual == expected && (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE) == enabled;
}

[[nodiscard]] bool TestScissorHook(std::string& error) {
    using penumbra_vr::hooks::ScopedEyeScissor;
    using penumbra_vr::hooks::ScissorRect;
    const GLuint eye_framebuffer = CurrentFramebuffer();
    const auto bind_framebuffer = reinterpret_cast<void(APIENTRY*)(GLenum, GLuint)>(
        wglGetProcAddress("glBindFramebuffer"));
    if (bind_framebuffer == nullptr) {
        error = "Test requires core framebuffer binding";
        return false;
    }
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    {
        ScopedEyeScissor scope({1280, 720});
        glScissor(640, 360, 320, 180);
        if (!ScissorEquals({159, 119, 82, 62}, false)) {
            error = "IAT hook did not scale the light rectangle";
            return false;
        }
        glEnable(GL_SCISSOR_TEST);
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        std::array<GLubyte, 4> inside{}, outside{};
        glReadPixels(200, 150, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside.data());
        glReadPixels(100, 150, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside.data());
        if (inside != std::array<GLubyte, 4>{255, 0, 0, 255} ||
            outside != std::array<GLubyte, 4>{0, 0, 0, 255}) {
            error = "Scaled light scissor failed actual framebuffer pixel coverage";
            return false;
        }
        {
            ScopedEyeScissor nested({320, 240});
            glScissor(20, 20, 20, 20);
            if (!ScissorEquals({19, 19, 22, 22}, true) || nested.remapped() != 1) {
                error = "Nested scissor scope failed";
                return false;
            }
        }
        glScissor(640, 360, 320, 180);
        if (!ScissorEquals({159, 119, 82, 62}, true)) {
            error = "Outer scissor scope was not restored";
            return false;
        }
        glViewport(0, 0, 128, 128);
        glScissor(1, 2, 3, 4);
        if (!ScissorEquals({1, 2, 3, 4}, true)) {
            error = "Intermediate viewport was incorrectly remapped";
            return false;
        }
        glViewport(0, 0, 320, 240);
        bind_framebuffer(0x8D40, 0);
        glScissor(5, 6, 7, 8);
        if (!ScissorEquals({5, 6, 7, 8}, true) || scope.remapped() != 2 || scope.bypassed() != 2) {
            error = "Foreign framebuffer bypass/counters failed";
            return false;
        }
        bind_framebuffer(0x8D40, eye_framebuffer);
    }
    glScissor(9, 10, 11, 12);
    if (!ScissorEquals({9, 10, 11, 12}, true)) {
        error = "Scissor scope leaked into non-eye calls";
        return false;
    }
    glDisable(GL_SCISSOR_TEST);
    glScissor(0, 0, 320, 240);
    return glGetError() == GL_NO_ERROR;
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
    if (!penumbra_vr::hooks::InstallOpenGlEyeScissor(error)) {
        std::cerr << "Scissor IAT installation failed: " << error << '\n';
        return 20;
    }
    if (penumbra_vr::hooks::InstallOpenGlEyeScissor(error) || error.empty()) {
        std::cerr << "Duplicate scissor installation was accepted\n";
        return 26;
    }
    glScissor(1, 2, 3, 4);
    glEnable(GL_SCISSOR_TEST);
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
        !ViewportEquals(eye_viewport) || !ScissorEquals(eye_viewport, false)) {
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
    if (!TestScissorHook(error)) {
        std::cerr << "Scissor integration failed: " << error << '\n';
        return 21;
    }
    if (!targets.BeginEye(
            penumbra_vr::graphics::Eye::right, inner_binding, error) ||
        !targets.EndEye(inner_binding, error)) {
        std::cerr << "Nested right-eye bind/restore failed: " << error << '\n';
        return 7;
    }
    if (CurrentFramebuffer() !=
            targets.target(penumbra_vr::graphics::Eye::left).framebuffer ||
        !ViewportEquals(eye_viewport) || !ScissorEquals(eye_viewport, false)) {
        std::cerr << "Nested eye restoration did not return to the left eye\n";
        return 8;
    }
    if (!targets.EndEye(outer_binding, error) || CurrentFramebuffer() != 0 ||
        !ViewportEquals(original_viewport) || !ScissorEquals({1, 2, 3, 4}, true)) {
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
    if (!penumbra_vr::hooks::RemoveOpenGlEyeScissor(error) ||
        !penumbra_vr::hooks::InstallOpenGlEyeScissor(error) ||
        !penumbra_vr::hooks::RemoveOpenGlEyeScissor(error)) {
        std::cerr << "Scissor hook removal/reinstallation failed: " << error << '\n';
        return 22;
    }
    {
        penumbra_vr::graphics::OpenGlEyeTargets unhooked_targets;
        penumbra_vr::graphics::OpenGlEyeBinding unhooked_binding;
        if (!unhooked_targets.CreateOrResize(320, 240, error) ||
            !unhooked_targets.BeginEye(penumbra_vr::graphics::Eye::left, unhooked_binding, error)) {
            return 23;
        }
        {
            penumbra_vr::hooks::ScopedEyeScissor scope({1280, 720});
            glScissor(20, 30, 40, 50);
            if (!ScissorEquals({20, 30, 40, 50}, false) || scope.remapped() != 0) {
                return 24;
            }
        }
        if (!unhooked_targets.EndEye(unhooked_binding, error)) {
            return 25;
        }
    }
    glDeleteTextures(1, &sentinel_texture);
    {
        // Capture two distinct halves, then verify both pixels and GL state in
        // the actual WGL driver. A compile-only test cannot catch a flipped menu.
        glViewport(0, 0, 8, 8);
        glDisable(GL_SCISSOR_TEST);
        glDrawBuffer(GL_BACK);
        glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST); glScissor(0, 4, 8, 4);
        glClearColor(0, 1, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_TEXTURE); glLoadIdentity(); glTranslatef(3, 4, 0);
        std::array<float, 16> matrix_before{}, matrix_after{};
        glGetFloatv(GL_TEXTURE_MATRIX, matrix_before.data());
        penumbra_vr::graphics::OpenGlMenuFrame menu;
        if (!menu.Capture(error) || !ScissorEquals({0, 4, 8, 4}, true) ||
            !ViewportEquals({0, 0, 8, 8})) {
            std::cerr << "Menu capture/state restoration failed: " << error << '\n'; return 26;
        }
        glGetFloatv(GL_TEXTURE_MATRIX, matrix_after.data());
        if (matrix_before != matrix_after || menu.Capture(error)) return 27;
        penumbra_vr::graphics::OpenGlEyeTargets menu_targets;
        penumbra_vr::graphics::OpenGlEyeBinding binding;
        penumbra_vr::runtime::VrEyeConfiguration eye;
        eye.left_tangent = -1; eye.right_tangent = 1;
        eye.top_tangent = -1; eye.bottom_tangent = 1;
        penumbra_vr::runtime::VrMatrix44 projection;
        if (!penumbra_vr::runtime::BuildHplInfiniteProjection(eye, 0.05F, projection, error) ||
            !menu_targets.CreateOrResize(320, 240, error)) return 28;
        for (auto which : {penumbra_vr::graphics::Eye::left, penumbra_vr::graphics::Eye::right}) {
            if (!menu_targets.BeginEye(which, binding, error)) return 29;
            glScissor(1, 2, 3, 4); glEnable(GL_SCISSOR_TEST);
            if (!menu.Draw(penumbra_vr::runtime::IdentityMatrix(), projection, error) ||
                !ScissorEquals({1, 2, 3, 4}, true)) return 30;
            std::array<GLubyte, 4> bottom{}, top{};
            glReadPixels(160, 100, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bottom.data());
            glReadPixels(160, 140, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, top.data());
            if (bottom != std::array<GLubyte, 4>{255, 0, 0, 255} ||
                top != std::array<GLubyte, 4>{0, 255, 0, 255}) {
                std::cerr << "Menu pixels missing/flipped: " << static_cast<int>(bottom[0]) << ","
                    << static_cast<int>(bottom[1]) << " / " << static_cast<int>(top[0]) << ","
                    << static_cast<int>(top[1]) << '\n'; return 31;
            }
            // A palm centered at z=-0.5 must appear over the distant menu and
            // disappear behind a nearer depth value, with all state restored.
            std::array<penumbra_vr::graphics::TrackedHandVisual,2> hands{};
            hands[1].visible=true;
            hands[1].palm=penumbra_vr::runtime::IdentityMatrix();
            hands[1].palm.values[11]=-0.5F;
            glDepthFunc(GL_GREATER); glDepthMask(GL_FALSE);
            if (!penumbra_vr::graphics::DrawTrackedHands(hands,penumbra_vr::runtime::IdentityMatrix(),projection,error) ||
                !ScissorEquals({1,2,3,4},true)) return 34;
            GLint depth_func=0; GLboolean depth_write=GL_TRUE;
            glGetIntegerv(GL_DEPTH_FUNC,&depth_func); glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);
            if (depth_func!=GL_GREATER || depth_write!=GL_FALSE) return 35;
            std::array<GLubyte,4> palm{};
            glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,palm.data());
            if (palm[0]<32 || palm[0]>120 || palm[1]<30 || palm[1]>115) {
                std::cerr<<"Hand pixel "<<static_cast<int>(palm[0])<<','<<static_cast<int>(palm[1])<<','
                    <<static_cast<int>(palm[2])<<" GL error "<<glGetError()<<'\n'; return 36;
            }
            glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE); glClearDepth(0);
            glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            if (!penumbra_vr::graphics::DrawTrackedHands(hands,penumbra_vr::runtime::IdentityMatrix(),projection,error)) return 37;
            glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,palm.data());
            if (palm!=std::array<GLubyte,4>{0,0,0,255}) return 38;
            glClearDepth(1); glDepthFunc(GL_LESS);
            if (!menu_targets.EndEye(binding, error)) return 32;
        }
        glGetFloatv(GL_TEXTURE_MATRIX, matrix_after.data());
        if (matrix_before != matrix_after || !ScissorEquals({0, 4, 8, 4}, true)) return 33;
    }
    std::cout << "OpenGL eye target allocation, resize and state restoration passed\n";
    return 0;
}
