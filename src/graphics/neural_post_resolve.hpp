#pragma once

// DirectNR's residual resolve at the OpenGL transport boundary: preserve the
// real full-resolution eye, adding only the low-resolution neural difference.
// Windows/gl.h and standard containers must be included by the bridge first.
namespace penumbra_vr::neural_post {
struct ResidualResolve {
    GLuint program = 0;
    GLuint stage[2]{};
    using Create = GLuint(APIENTRY*)(GLenum);
    using Source = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
    using Object = void(APIENTRY*)(GLuint);
    using Query = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using NewProgram = GLuint(APIENTRY*)();
    using Attach = void(APIENTRY*)(GLuint, GLuint);
    using Location = GLint(APIENTRY*)(GLuint, const char*);
    using Uniform = void(APIENTRY*)(GLint, GLint);
    using Active = void(APIENTRY*)(GLenum);
    Object delete_program = nullptr, use = nullptr;
    Location location = nullptr;
    Uniform uniform = nullptr;
    Active active = nullptr;
    std::uint32_t width = 0, height = 0;

    template<class T> static T Proc(const char* name) noexcept {
        const PROC p = wglGetProcAddress(name);
        if (!p || p == reinterpret_cast<PROC>(1) || p == reinterpret_cast<PROC>(2) ||
            p == reinterpret_cast<PROC>(3) || p == reinterpret_cast<PROC>(-1)) return nullptr;
        return reinterpret_cast<T>(p);
    }
    bool Initialize() noexcept {
        if (program) return true;
        const auto create = Proc<Create>("glCreateShader");
        const auto source = Proc<Source>("glShaderSource");
        const auto compile = Proc<Object>("glCompileShader");
        const auto shader_query = Proc<Query>("glGetShaderiv");
        const auto remove = Proc<Object>("glDeleteShader");
        const auto new_program = Proc<NewProgram>("glCreateProgram");
        const auto attach = Proc<Attach>("glAttachShader");
        const auto link = Proc<Object>("glLinkProgram");
        const auto program_query = Proc<Query>("glGetProgramiv");
        delete_program = Proc<Object>("glDeleteProgram");
        use = Proc<Object>("glUseProgram");
        location = Proc<Location>("glGetUniformLocation");
        uniform = Proc<Uniform>("glUniform1i");
        active = Proc<Active>("glActiveTexture");
        if (!create || !source || !compile || !shader_query || !remove || !new_program ||
            !attach || !link || !program_query || !delete_program || !use || !location || !uniform || !active)
            return false;
        const char* sources[]{
            "#version 120\nvarying vec2 uv;void main(){uv=gl_MultiTexCoord0.xy;gl_Position=gl_Vertex;}",
            "#version 120\nuniform sampler2D originalEye;uniform sampler2D neuralInput;uniform sampler2D neuralOutput;uniform int sourceSrgb;varying vec2 uv;"
            "void main(){vec4 base=texture2D(originalEye,uv);"
            "if(sourceSrgb!=0){vec3 low=12.92*base.rgb;"
            "vec3 high=1.055*pow(max(base.rgb,vec3(0.0)),vec3(1.0/2.4))-0.055;"
            "base.rgb=mix(high,low,step(base.rgb,vec3(0.0031308)));}"
            "vec2 workUv=vec2(uv.x,1.0-uv.y);"
            "vec3 change=texture2D(neuralOutput,workUv).rgb-texture2D(neuralInput,workUv).rgb;"
            "gl_FragColor=vec4(clamp(base.rgb+change,0.0,1.0),base.a);}"};
        GLuint shaders[2]{};
        bool valid = true;
        for (unsigned i = 0; i != 2; ++i) {
            shaders[i] = create(i == 0 ? 0x8B31 : 0x8B30);
            if (!shaders[i]) { valid = false; break; }
            source(shaders[i], 1, &sources[i], nullptr);
            compile(shaders[i]);
            GLint compiled = 0; shader_query(shaders[i], 0x8B81, &compiled);
            if (!compiled) { valid = false; break; }
        }
        if (valid) {
            program = new_program();
            if (program) {
                attach(program, shaders[0]); attach(program, shaders[1]); link(program);
                GLint linked = 0; program_query(program, 0x8B82, &linked);
                valid = linked != 0;
            } else valid = false;
        }
        for (auto shader : shaders) if (shader) remove(shader);
        if (!valid && program) { delete_program(program); program = 0; }
        return valid;
    }
    void Release() noexcept {
        if (program) delete_program(program);
        glDeleteTextures(2, stage);
        program = 0; stage[0] = stage[1] = 0; width = 0; height = 0;
    }
    bool Draw(GLuint original, const std::uint8_t* input, const std::uint8_t* output,
        std::uint32_t sw, std::uint32_t sh, std::uint32_t w, std::uint32_t h) noexcept {
        if (!Initialize()) return false;
        GLint old_program = 0, old_active = 0;
        glGetIntegerv(0x8B8D, &old_program);
        glGetIntegerv(0x84E0, &old_active);
        // Match the existing enhanced-eye stage's compatibility renderer state.
        // FBO and pixel transfer state are restored by the bridge's outer guard.
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        if (!stage[0]) glGenTextures(2, stage);
        const std::uint8_t* pixels[]{input, output};
        for (unsigned i = 0; i != 2; ++i) {
            active(0x84C1 + i);
            glBindTexture(GL_TEXTURE_2D, stage[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
            // DirectNR explicitly uses UNORM SRV/UAVs, including for incoming
            // SRGB resources. Match its encoded-byte residual, not a second
            // linear-light interpretation of the already-resolved model edit.
            if (width != sw || height != sh)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(sw),
                    static_cast<GLsizei>(sh), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels[i]);
            else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(sw),
                static_cast<GLsizei>(sh), GL_RGBA, GL_UNSIGNED_BYTE, pixels[i]);
        }
        width = sw; height = sh;
        active(0x84C0);
        glBindTexture(GL_TEXTURE_2D, original);
        GLint original_format = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &original_format);
        glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DITHER); glDisable(0x8DB9 /* FRAMEBUFFER_SRGB */);
        glDisable(0x8620 /* VERTEX_PROGRAM_ARB */); glDisable(0x8804 /* FRAGMENT_PROGRAM_ARB */);
        for (unsigned i = 0; i != 6; ++i) glDisable(GL_CLIP_PLANE0 + i);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
        use(program);
        uniform(location(program, "originalEye"), 0);
        uniform(location(program, "neuralInput"), 1);
        uniform(location(program, "neuralOutput"), 2);
        uniform(location(program, "sourceSrgb"), original_format == 0x8C43 || original_format == 0x8C41 ? 1 : 0);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, 1);
        glTexCoord2f(0, 1); glVertex2f(-1, 1);
        glEnd();
        const bool success = glGetError() == GL_NO_ERROR;
        use(static_cast<GLuint>(old_program));
        glPopAttrib();
        active(static_cast<GLenum>(old_active));
        return success && glGetError() == GL_NO_ERROR;
    }
};
}
