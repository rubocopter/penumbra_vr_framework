#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"
#include "vr_hand_pose.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

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
        const auto* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        const bool rectangle=extensions && (std::strstr(extensions,"GL_ARB_texture_rectangle") ||
            std::strstr(extensions,"GL_EXT_texture_rectangle") || std::strstr(extensions,"GL_NV_texture_rectangle"));
        glGetIntegerv(0x84E2, &units); // GL_MAX_TEXTURE_UNITS
        for (GLint unit = 0; unit < units; ++unit) {
            active(kTexture0 + static_cast<GLenum>(unit));
            glDisable(GL_TEXTURE_1D); glDisable(GL_TEXTURE_2D);
            glDisable(0x806F); glDisable(0x8513); // 3D / cube
            if (rectangle) glDisable(0x84F5); // Rectangle takes precedence over 2D in fixed-function rendering.
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

bool ClearMonitorBackbuffer(std::string& error) noexcept {
    error.clear();
    if (!wglGetCurrentContext()) {
        error = "Monitor clear requires a current context";
        return false;
    }
    GLint framebuffer = 0;
    glGetIntegerv(kFramebufferBinding, &framebuffer);
    if (framebuffer != 0) {
        error = "Monitor clear requires the desktop framebuffer";
        return false;
    }
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDrawBuffer(GL_BACK);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glPopAttrib();
    return true;
}

bool DrawMonitorMirror(unsigned int texture, std::string& error) noexcept {
    error.clear();
    if (!wglGetCurrentContext() || !texture || !glIsTexture(texture)) {
        error="Mirror requires a current context and an eye texture"; return false;
    }
    GLint framebuffer=0; glGetIntegerv(kFramebufferBinding,&framebuffer);
    if (framebuffer!=0) { error="Mirror requires the desktop framebuffer"; return false; }
    State state;
    if (!state.valid) { error="Mirror requires GL multitexture/program APIs"; return false; }
    glBindTexture(GL_TEXTURE_2D,texture);
    GLint width=0,height=0; std::array<GLint,4> viewport{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
    glGetIntegerv(GL_VIEWPORT,viewport.data());
    if (width<=0 || height<=0 || viewport[2]<=0 || viewport[3]<=0) {
        error="Mirror dimensions are invalid"; return false;
    }
    const float fit=std::min(static_cast<float>(viewport[2])/width,static_cast<float>(viewport[3])/height);
    const float x=width*fit/viewport[2], y=height*fit/viewport[3];
    glDrawBuffer(GL_BACK); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_TEXTURE_2D); glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-x,-y);
    glTexCoord2f(1,0); glVertex2f(x,-y);
    glTexCoord2f(1,1); glVertex2f(x,y);
    glTexCoord2f(0,1); glVertex2f(-x,y);
    glEnd();
    return true;
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
        const auto articulation=runtime::ArticulateVrHand(hand.curl,index==0);
        constexpr std::array<std::array<float,3>,5> lengths{{
            {0.025F,0.027F,0.022F},{0.037F,0.026F,0.020F},{0.041F,0.028F,0.021F},
            {0.038F,0.026F,0.020F},{0.030F,0.020F,0.017F}}};
        const float side=index==0 ? 1.0F : -1.0F;
        for (std::size_t finger=0;finger<5;++finger) {
            glPushMatrix();
            if (finger==0) {
                glTranslatef(side*0.036F,-0.003F,0.004F);
                glRotatef(articulation.thumb_yaw_degrees,0,1,0);
            } else {
                // Mirror digit placement too: the index must neighbour the
                // thumb on BOTH hands, not just on the right hand.
                glTranslatef(-side*(static_cast<float>(finger)-2.5F)*0.020F,0,-0.046F);
                glRotatef(articulation.fingers[finger].spread_degrees,0,1,0);
            }
            for (int joint=0;joint<3;++joint) {
                const auto j=static_cast<std::size_t>(joint);
                const float segment=lengths[finger][j];
                glRotatef(-articulation.fingers[finger].flexion_degrees[j],1,0,0);
                glTranslatef(0,0,-segment*0.5F);
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
                          const runtime::VrMatrix44& projection,
                          float distance,
                          float scale,
                          std::string& error) const noexcept {
    error.clear();
    if (!texture_ || context_ != wglGetCurrentContext()) {
        error = "Menu draw requires a captured texture in its owning context"; return false;
    }
    if (!std::isfinite(distance) || distance <= 0.0F ||
        !std::isfinite(scale) || scale <= 0.0F) {
        error = "Menu draw requires positive finite geometry"; return false;
    }
    State state;
    if (!state.valid) { error = "Menu draw requires GL multitexture/program APIs"; return false; }
    const auto gl_view = ColumnMajor(view), gl_projection = ColumnMajor(projection);
    glClearColor(0.015F, 0.015F, 0.02F, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(gl_projection.data());
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(gl_view.data());
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texture_);
    const float half_width = 1.2F * scale, half_height = half_width / aspect_;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-half_width, -half_height, -distance);
    glTexCoord2f(1, 0); glVertex3f( half_width, -half_height, -distance);
    glTexCoord2f(1, 1); glVertex3f( half_width,  half_height, -distance);
    glTexCoord2f(0, 1); glVertex3f(-half_width,  half_height, -distance);
    glEnd();
    return true;
}
}
