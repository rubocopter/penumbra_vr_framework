// Actual Requiem allocation/draw consumers with real hidden-window OpenGL.
// OpenVR and native gameplay boundaries are supplied by this host fixture.
#include "../../src/backends/requiem/render_world.cpp"
#include "../../src/backends/requiem/vr_settings_capabilities.hpp"
#include <iostream>

namespace {
using namespace penumbra_vr;
runtime::VrRenderTargetSize recommended;
int submissions = 0;
}
namespace penumbra_vr::runtime {
OpenVrSession::~OpenVrSession() noexcept = default;
bool OpenVrSession::initialized() const noexcept { return true; }
bool OpenVrSession::ReadEyeConfiguration(std::array<VrEyeConfiguration,2>&,std::string&) const noexcept { return true; }
VrRenderTargetSize OpenVrSession::recommended_render_target_size() const noexcept { return recommended; }
bool OpenVrSession::WaitForHmdPose(VrHmdPose& pose, std::string&) const noexcept {
    pose.device_connected = pose.pose_valid = true;
    std::copy_n(IdentityMatrix().values.begin(), 12, pose.device_to_absolute.values.begin());
    return true;
}
bool OpenVrSession::SubmitOpenGlEyeTextures(const std::array<std::uint32_t,2>&, std::string&) const noexcept {
    ++submissions; return true;
}
}
namespace penumbra_vr::probe { void WriteLog(const char*, ...) noexcept {} }
namespace penumbra_vr::backends::requiem {
RequiemBodyTrackingSample ReadGameplayTrackingSample() noexcept { return {}; }
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept { return {}; }
bool NativeUiActive() noexcept { return true; }
NativeUiSurface CurrentNativeUiSurface() noexcept { return NativeUiSurface::fullscreen; }
void PublishGameplayHeadTracking(const runtime::VrHmdPose&,float) noexcept {}
}

namespace penumbra_vr::backends::requiem {
namespace {
std::uint32_t expected_width, expected_height;
std::size_t expected_eye;
bool native_draw_ok;
float native_time;
void __fastcall DrawWorld(void*, void*, void*, void* camera, float time) {
    std::array<GLint,4> viewport{};
    glGetIntegerv(GL_VIEWPORT,viewport.data());
    runtime::VrMatrix44 projection;
    std::memcpy(&projection,static_cast<std::uint8_t*>(camera)+kCameraLayout.projection_matrix_offset,sizeof(projection));
    native_draw_ok = viewport == std::array<GLint,4>{0,0,
        static_cast<GLint>(expected_width),static_cast<GLint>(expected_height)} &&
        projection.values == g_projections[expected_eye].values &&
        ReadNative<float>(camera,kCameraLayout.fov_offset)==1.07F &&
        ReadNative<float>(camera,kCameraLayout.aspect_offset)==1.777F;
    native_time = time;
}
bool TargetsAndDraw(std::uint32_t width,std::uint32_t height) {
    if (g_targets.width()!=width || g_targets.height()!=height) {
        std::cerr<<"Source world target mismatch: "<<g_targets.width()<<'x'<<g_targets.height()
            <<" expected "<<width<<'x'<<height<<'\n'; return false;
    }
    using GetRenderbuffer = void(APIENTRY*)(GLenum,GLenum,GLint*);
    using BindRenderbuffer = void(APIENTRY*)(GLenum,GLuint);
    const auto get_rb=reinterpret_cast<GetRenderbuffer>(wglGetProcAddress("glGetRenderbufferParameteriv"));
    const auto bind_rb=reinterpret_cast<BindRenderbuffer>(wglGetProcAddress("glBindRenderbuffer"));
    if (!get_rb || !bind_rb) return false;
    expected_width=width; expected_height=height;
    std::array<std::uint8_t,0x900> camera{};
    const float native_fov=1.07F, native_aspect=1.777F;
    std::memcpy(camera.data()+kCameraLayout.fov_offset,&native_fov,sizeof(native_fov));
    std::memcpy(camera.data()+kCameraLayout.aspect_offset,&native_aspect,sizeof(native_aspect));
    const auto original=camera;
    const std::array<graphics::TrackedHandVisual,2> hands{};
    std::string error;
    for (expected_eye=0;expected_eye<2;++expected_eye) {
        const auto& target=g_targets.target(expected_eye==0?graphics::Eye::left:graphics::Eye::right);
        glBindTexture(GL_TEXTURE_2D,target.color_texture);
        GLint actual_width=0,actual_height=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&actual_width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&actual_height);
        if (actual_width!=static_cast<GLint>(width) || actual_height!=static_cast<GLint>(height)) return false;
        glBindTexture(GL_TEXTURE_2D,0);
        bind_rb(0x8D41,target.depth_stencil_renderbuffer);
        get_rb(0x8D41,0x8D42,&actual_width); get_rb(0x8D41,0x8D43,&actual_height);
        bind_rb(0x8D41,0);
        if (actual_width!=static_cast<GLint>(width) || actual_height!=static_cast<GLint>(height)) return false;
        bool rendered=false; native_draw_ok=false;
        const float time=expected_eye==0?0.016F:0.0F;
        if (!RenderEye(reinterpret_cast<RenderWorld>(&DrawWorld),nullptr,nullptr,camera.data(),runtime::IdentityMatrix(),
                expected_eye,hands,time,rendered,error) || !rendered || !native_draw_ok ||
            native_time!=time || camera!=original) {
            std::cerr<<"Native eye draw/projection/restore failed: "<<error<<'\n'; return false;
        }
        std::array<GLint,4> viewport{}; glGetIntegerv(GL_VIEWPORT,viewport.data());
        if (viewport!=std::array<GLint,4>{3,5,64,48}) return false;
    }
    return true;
}
bool Cases() {
    runtime::OpenVrSession session;
    g_session.store(&session);
    g_presenting.store(true);
    // Deliberately different, asymmetric frusta; target aspect cannot replace them.
    g_eyes[0]={-1.2F,0.8F,-0.7F,1.1F,{}};
    g_eyes[1]={-0.9F,1.3F,-1.0F,0.8F,{}};
    for (std::size_t i=0;i<2;++i) {
        const auto identity=runtime::IdentityMatrix();
        std::copy_n(identity.values.begin(),12,g_eyes[i].eye_to_head.values.begin());
        std::string error;
        if (!runtime::BuildHplInfiniteProjection(g_eyes[i],0.05F,g_projections[i],error)) return false;
    }
    const auto projections=g_projections;
    const auto close_float=[](float a,float b){return std::fabs(a-b)<0.00001F;};
    if (!close_float(projections[0].values[0],1.0F) || !close_float(projections[0].values[2],-0.2F) ||
        !close_float(projections[0].values[5],1.111111F) || !close_float(projections[0].values[6],0.222222F) ||
        !close_float(projections[1].values[0],0.909091F) || !close_float(projections[1].values[2],0.181818F) ||
        !close_float(projections[1].values[5],1.111111F) || !close_float(projections[1].values[6],-0.111111F))
        return false;
    bool passed=true;
    for (const auto size : {runtime::VrRenderTargetSize{2804,2860},runtime::VrRenderTargetSize{2016,1120}}) {
        recommended=size;
        runtime::VrSettings settings;
        ConfigurePresentationSettings(settings);
        std::string error;
        static_cast<void>(g_targets.Destroy(error));
        bool consumed=false;
        // A null camera deliberately exits after production world-target allocation.
        static_cast<void>(RenderStereo(nullptr,nullptr,nullptr,nullptr,0,consumed,error));
        passed=TargetsAndDraw(size.width,size.height) && passed;
        const auto initial_left=g_targets.target(graphics::Eye::left).color_texture;
        const auto initial_right=g_targets.target(graphics::Eye::right).color_texture;
        settings.render_scale=0.5F;
        ConfigurePresentationSettings(settings);
        static_cast<void>(RenderStereo(nullptr,nullptr,nullptr,nullptr,0,consumed,error));
        passed=TargetsAndDraw(size.width,size.height) && passed;
        if (g_targets.target(graphics::Eye::left).color_texture!=initial_left ||
            g_targets.target(graphics::Eye::right).color_texture!=initial_right) passed=false;
        // End/start the presentation before applying the saved multiplier.
        static_cast<void>(g_targets.Destroy(error));
        static_cast<void>(RenderStereo(nullptr,nullptr,nullptr,nullptr,0,consumed,error));
        passed=TargetsAndDraw(size.width/2,size.height/2) && passed;
        // Menu and world share one fixed pair for the presentation lifetime.
        settings.render_scale=1.25F;
        ConfigurePresentationSettings(settings);
        OnSdlSwap(0);
        passed=TargetsAndDraw(size.width/2,size.height/2) && passed;
        static_cast<void>(g_targets.Destroy(error));
        OnSdlSwap(0);
        passed=TargetsAndDraw(size.width*5/4,size.height*5/4) && passed;
        settings.render_scale=1.0F;
        ConfigurePresentationSettings(settings);
        static_cast<void>(g_targets.Destroy(error));
        OnSdlSwap(1);
        passed=TargetsAndDraw(size.width,size.height) && passed;
        if (g_projections[0].values!=projections[0].values ||
            g_projections[1].values!=projections[1].values || consumed) passed=false;
    }
    const auto old_left=g_targets.target(graphics::Eye::left).color_texture;
    const auto old_right=g_targets.target(graphics::Eye::right).color_texture;
    recommended={0x7fffffffU,0x7fffffffU};
    bool consumed=false;
    std::string error;
    static_cast<void>(RenderStereo(nullptr,nullptr,nullptr,nullptr,0,consumed,error));
    if (g_targets.target(graphics::Eye::left).color_texture!=old_left ||
        g_targets.target(graphics::Eye::right).color_texture!=old_right ||
        consumed) passed=false;
    passed=TargetsAndDraw(2016,1120) && passed;
    const auto caps=RequiemVrSettingCapabilities();
    if (!caps.supported[static_cast<std::size_t>(runtime::VrSettingId::render_scale)]) {
        std::cerr<<"Requiem RenderScale menu capability missing\n"; passed=false;
    }
    g_session.store(nullptr); g_presenting.store(false);
    static_cast<void>(g_targets.Destroy(error)); static_cast<void>(g_overlay_targets.Destroy(error));
    return passed && submissions==6;
}
}
}
int main() {
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.style=CS_OWNDC; wc.lpfnWndProc=DefWindowProcW;
    wc.hInstance=instance; wc.lpszClassName=L"PvrResolutionConsumer";
    if (!RegisterClassW(&wc)) return 2;
    const auto window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,instance,nullptr);
    const auto dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{}; pfd.nSize=sizeof(pfd); pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32; pfd.cDepthBits=24; pfd.cStencilBits=8;
    if (!SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)) return 2;
    const auto context=wglCreateContext(dc);
    if (!context || !wglMakeCurrent(dc,context)) return 2;
    glViewport(3,5,64,48);
    const bool passed=penumbra_vr::backends::requiem::Cases();
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context);
    ReleaseDC(window,dc); DestroyWindow(window); UnregisterClassW(wc.lpszClassName,instance);
    if (passed) std::cout<<"Requiem source resolution, native eyes, live scale and rollback passed\n";
    return passed?0:1;
}
