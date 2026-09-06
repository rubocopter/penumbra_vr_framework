#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace penumbra_vr::graphics {
namespace {
using ActiveTexture = void(APIENTRY*)(GLenum);
using UseProgram = void(APIENTRY*)(GLuint);
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kFramebufferBinding = 0x8CA6;

template<class T> T Proc(const char* name) noexcept {
    const auto p = wglGetProcAddress(name);
    if (!p || p == reinterpret_cast<PROC>(1) || p == reinterpret_cast<PROC>(2) ||
        p == reinterpret_cast<PROC>(3) || p == reinterpret_cast<PROC>(-1)) return nullptr;
    return reinterpret_cast<T>(p);
}

// HPL mixes fixed-function, ARB programs and texture units. Matrix stacks and
// GLSL program binding are not covered by glPushAttrib and need explicit saves.
struct State {
    ActiveTexture active = Proc<ActiveTexture>("glActiveTexture");
    UseProgram use = Proc<UseProgram>("glUseProgram");
    GLint program = 0;
    GLint matrix_mode = GL_MODELVIEW;
    bool valid = false;
    State() {
        if (!active || !use) return;
        glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
        glGetIntegerv(0x8B8D, &program); // GL_CURRENT_PROGRAM
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        use(0);
        glDisable(0x8620); // GL_VERTEX_PROGRAM_ARB (supported by mapped HPL build)
        glDisable(0x8804); // GL_FRAGMENT_PROGRAM_ARB
        GLint units = 0;
        glGetIntegerv(0x84E2, &units); // GL_MAX_TEXTURE_UNITS
        for (GLint unit = 0; unit < units; ++unit) {
            active(kTexture0 + static_cast<GLenum>(unit));
            glDisable(GL_TEXTURE_1D); glDisable(GL_TEXTURE_2D);
            glDisable(0x806F); glDisable(0x8513); // 3D / cube
            glDisable(GL_TEXTURE_GEN_S); glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_GEN_R); glDisable(GL_TEXTURE_GEN_Q);
        }
        active(kTexture0);
        glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity();
        glMatrixMode(GL_PROJECTION); glPushMatrix();
        glMatrixMode(GL_MODELVIEW); glPushMatrix();
        glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST);
        glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_LIGHTING);
        glDisable(GL_FOG); glDisable(GL_CULL_FACE); glDisable(GL_COLOR_LOGIC_OP);
        for (int plane = 0; plane < 6; ++plane) glDisable(GL_CLIP_PLANE0 + plane);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glPixelTransferf(GL_RED_SCALE, 1); glPixelTransferf(GL_GREEN_SCALE, 1);
        glPixelTransferf(GL_BLUE_SCALE, 1); glPixelTransferf(GL_ALPHA_SCALE, 1);
        glPixelTransferf(GL_RED_BIAS, 0); glPixelTransferf(GL_GREEN_BIAS, 0);
        glPixelTransferf(GL_BLUE_BIAS, 0); glPixelTransferf(GL_ALPHA_BIAS, 0);
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
        valid = true;
    }
    ~State() {
        if (!valid) return;
        active(kTexture0);
        glMatrixMode(GL_TEXTURE); glPopMatrix();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopMatrix();
        use(static_cast<GLuint>(program));
        glPopAttrib();
        glMatrixMode(static_cast<GLenum>(matrix_mode));
    }
};
std::array<float, 16> ColumnMajor(const runtime::VrMatrix44& matrix) {
    std::array<float, 16> result{};
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t col = 0; col < 4; ++col) result[col * 4 + row] = matrix.values[row * 4 + col];
    return result;
}
void Box(float x, float y, float z) {
    const std::array<std::array<float,3>,8> p{{{-x,-y,-z},{x,-y,-z},{x,y,-z},{-x,y,-z},
        {-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}}};
    constexpr std::array<unsigned,24> faces{0,1,2,3, 5,4,7,6, 4,0,3,7, 1,5,6,2, 3,2,6,7, 4,5,1,0};
    glBegin(GL_QUADS);
    for (const auto i:faces) glVertex3fv(p[i].data());
    glEnd();
}
bool Rigid(const runtime::VrMatrix44& pose) {
    runtime::VrMatrix34 compact;
    std::copy_n(pose.values.begin(),compact.values.size(),compact.values.begin());
    runtime::VrMatrix44 inverse; std::string error;
    return pose.values[12]==0 && pose.values[13]==0 && pose.values[14]==0 && pose.values[15]==1 &&
        runtime::InvertRigidTransform(compact,inverse,error);
}
}

bool DrawTrackedHands(const std::array<TrackedHandVisual,2>& hands,
    const runtime::VrMatrix44& view, const runtime::VrMatrix44& projection, std::string& error) noexcept {
    error.clear();
    if (!wglGetCurrentContext() || !Rigid(view) ||
        !std::all_of(projection.values.begin(),projection.values.end(),[](float v){return std::isfinite(v);})) {
        error="Tracked hands require a current GL context and finite eye matrices"; return false;
    }
    State state;
    if (!state.valid) { error="Tracked hands require GL multitexture/program APIs"; return false; }
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glDepthRange(0,1);
    const auto gl_projection=ColumnMajor(projection);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(gl_projection.data());
    glMatrixMode(GL_MODELVIEW);
    for (std::size_t index=0;index<hands.size();++index) {
        const auto& hand=hands[index];
        if (!hand.visible || !Rigid(hand.palm)) continue;
        const auto model=ColumnMajor(runtime::Multiply(view,hand.palm));
        glLoadMatrixf(model.data());
        glColor3f(0.30F,0.27F,0.23F);
        Box(0.041F,0.014F,0.047F);
        glPushMatrix(); glTranslatef(0,0,0.063F);
        glColor3f(0.15F,0.14F,0.12F); Box(0.031F,0.019F,0.022F); glPopMatrix();
        for (std::size_t finger=0;finger<5;++finger) {
            const float curl=std::isfinite(hand.curl[finger]) ? std::clamp(hand.curl[finger],0.0F,1.0F) : 0;
            glPushMatrix();
            if (finger==0) {
                const float side=index==0 ? 1.0F : -1.0F;
                glTranslatef(side*0.036F,-0.003F,0.004F); glRotatef(-side*48,0,1,0);
            } else {
                glTranslatef((static_cast<float>(finger)-2.5F)*0.020F,0,-0.046F);
            }
            const float segment=finger==0 ? 0.022F : finger==4 ? 0.020F : 0.026F;
            for (int joint=0;joint<3;++joint) {
                glRotatef(-curl*70,1,0,0); glTranslatef(0,0,-segment*0.5F);
                const float shade=0.36F-static_cast<float>(joint)*0.025F;
                glColor3f(shade,shade*0.90F,shade*0.76F);
                Box(0.0075F,0.009F,segment*0.46F);
                glTranslatef(0,0,-segment*0.5F);
            }
            glPopMatrix();
        }
        if (hand.ray && Rigid(hand.aim)) {
            const auto ray=ColumnMajor(runtime::Multiply(view,hand.aim));
            glLoadMatrixf(ray.data()); glLineWidth(1); glColor3f(0.6F,0.65F,0.55F);
            glBegin(GL_LINES); glVertex3f(0,0,-0.08F); glVertex3f(0,0,-2); glEnd();
        }
    }
    return true;
}

OpenGlMenuFrame::~OpenGlMenuFrame() {
    if (texture_ && context_ == wglGetCurrentContext()) glDeleteTextures(1, &texture_);
}
bool OpenGlMenuFrame::Capture(std::string& error) noexcept {
    error.clear();
    if (!wglGetCurrentContext() || texture_) {
        error = "Menu capture requires a fresh frame and a current GL context"; return false;
    }
    GLint framebuffer = -1, max_texture = 0;
    std::array<GLint, 4> viewport{};
    glGetIntegerv(kFramebufferBinding, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (framebuffer != 0 || viewport[0] < 0 || viewport[1] < 0 ||
        viewport[2] <= 0 || viewport[3] <= 0 || viewport[2] > max_texture || viewport[3] > max_texture) {
        error = "Menu capture requires a valid desktop framebuffer viewport"; return false;
    }
    State state;
    if (!state.valid) { error = "Menu capture requires GL multitexture/program APIs"; return false; }
    context_ = wglGetCurrentContext();
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glReadBuffer(GL_BACK);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, viewport[0], viewport[1], viewport[2], viewport[3], 0);
    GLint width = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    if (width != viewport[2]) { error = "Menu texture allocation/capture failed"; return false; }
    aspect_ = static_cast<float>(viewport[2]) / static_cast<float>(viewport[3]);
    return true;
}
bool OpenGlMenuFrame::Draw(const runtime::VrMatrix44& view,
                          const runtime::VrMatrix44& projection, std::string& error) const noexcept {
    error.clear();
    if (!texture_ || context_ != wglGetCurrentContext()) {
        error = "Menu draw requires a captured texture in its owning context"; return false;
    }
    State state;
    if (!state.valid) { error = "Menu draw requires GL multitexture/program APIs"; return false; }
    const auto gl_view = ColumnMajor(view), gl_projection = ColumnMajor(projection);
    glClearColor(0.015F, 0.015F, 0.02F, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(gl_projection.data());
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(gl_view.data());
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texture_);
    const float half_width = 1.2F, half_height = half_width / aspect_;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-half_width, -half_height, -2);
    glTexCoord2f(1, 0); glVertex3f( half_width, -half_height, -2);
    glTexCoord2f(1, 1); glVertex3f( half_width,  half_height, -2);
    glTexCoord2f(0, 1); glVertex3f(-half_width,  half_height, -2);
    glEnd();
    return true;
}
}
