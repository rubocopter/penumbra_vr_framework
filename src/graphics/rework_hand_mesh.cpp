#include "rework_hand_mesh.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include "vr_math.hpp"
#include "vr_rework_hand_profile.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace penumbra_vr::graphics {
namespace {

struct MeshVec2 { float x; float y; };
struct MeshVec3 { float x; float y; float z; };
struct MeshCorner { std::uint16_t position; std::uint16_t uv; };
struct DrawVertex { float u; float v; float x; float y; float z; };

namespace generated {
#include "generated/rework_hand_mesh_data.inl"
} // namespace generated

using Matrix = runtime::VrMatrix44;

struct MeshView {
    const MeshVec3* positions = nullptr;
    std::size_t position_count = 0;
    const std::uint8_t* bones = nullptr;
    const MeshCorner* render_vertices = nullptr;
    std::size_t render_vertex_count = 0;
    const std::uint16_t* triangle_indices = nullptr;
    std::size_t index_count = 0;
    const MeshVec3* translations = nullptr;
    const std::int8_t* parents = nullptr;
    const std::array<float, 16>* inverse_bind = nullptr;
};

constexpr float kRadiansToDegrees = 57.29577951308232F;
constexpr float kDegreesToRadians = 0.017453292519943295F;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kClientActiveTexture = 0x84E1;
constexpr GLenum kArrayBuffer = 0x8892;
constexpr GLenum kElementArrayBuffer = 0x8893;
constexpr GLenum kArrayBufferBinding = 0x8894;
constexpr GLenum kElementArrayBufferBinding = 0x8895;
using ClientActiveTexture = void(APIENTRY*)(GLenum);
using BindBuffer = void(APIENTRY*)(GLenum, GLuint);

template<class T>
T ExtensionProc(const char* name) noexcept {
    const auto proc = wglGetProcAddress(name);
    if (!proc || proc == reinterpret_cast<PROC>(1) || proc == reinterpret_cast<PROC>(2) ||
        proc == reinterpret_cast<PROC>(3) || proc == reinterpret_cast<PROC>(-1)) return nullptr;
    return reinterpret_cast<T>(proc);
}

struct ClientArrayApi {
    HGLRC context = nullptr;
    ClientActiveTexture active_texture = nullptr;
    BindBuffer bind_buffer = nullptr;
};

ClientArrayApi& CurrentClientArrayApi() noexcept {
    thread_local ClientArrayApi api;
    const HGLRC current = wglGetCurrentContext();
    if (api.context == current) return api;
    api = {};
    api.context = current;
    api.active_texture = ExtensionProc<ClientActiveTexture>("glClientActiveTexture");
    if (!api.active_texture)
        api.active_texture = ExtensionProc<ClientActiveTexture>("glClientActiveTextureARB");
    api.bind_buffer = ExtensionProc<BindBuffer>("glBindBuffer");
    if (!api.bind_buffer) api.bind_buffer = ExtensionProc<BindBuffer>("glBindBufferARB");
    return api;
}

Matrix Translation(const MeshVec3& value) noexcept {
    Matrix result = runtime::IdentityMatrix();
    result.values[3] = value.x;
    result.values[7] = value.y;
    result.values[11] = value.z;
    return result;
}

Matrix AxisRotation(float degrees, float x, float y, float z) noexcept {
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 1.0e-6F || std::fabs(degrees) <= 1.0e-6F) {
        return runtime::IdentityMatrix();
    }
    x /= length; y /= length; z /= length;
    const float angle = degrees * kDegreesToRadians;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0F - c;
    Matrix result = runtime::IdentityMatrix();
    result.values[0] = t*x*x + c;
    result.values[1] = t*x*y - s*z;
    result.values[2] = t*x*z + s*y;
    result.values[4] = t*x*y + s*z;
    result.values[5] = t*y*y + c;
    result.values[6] = t*y*z - s*x;
    result.values[8] = t*x*z - s*y;
    result.values[9] = t*y*z + s*x;
    result.values[10] = t*z*z + c;
    return result;
}

Matrix StoredMatrix(const std::array<float, 16>& values) noexcept {
    Matrix result;
    result.values = values;
    return result;
}

MeshVec3 TransformPoint(const Matrix& matrix, const MeshVec3& point) noexcept {
    return {
        matrix.values[0] * point.x + matrix.values[1] * point.y + matrix.values[2] * point.z + matrix.values[3],
        matrix.values[4] * point.x + matrix.values[5] * point.y + matrix.values[6] * point.z + matrix.values[7],
        matrix.values[8] * point.x + matrix.values[9] * point.y + matrix.values[10] * point.z + matrix.values[11],
    };
}

MeshView View(bool left) noexcept {
    if (left) {
        return {
            generated::kLeftPositions.data(), generated::kLeftPositions.size(),
            generated::kLeftPositionBones.data(),
            generated::kLeftRenderVertices.data(), generated::kLeftRenderVertices.size(),
            generated::kLeftTriangleIndices.data(), generated::kLeftTriangleIndices.size(),
            generated::kLeftLocalTranslations.data(), generated::kLeftParents.data(), generated::kLeftInverseBind.data(),
        };
    }
    return {
        generated::kRightPositions.data(), generated::kRightPositions.size(),
        generated::kRightPositionBones.data(),
        generated::kRightRenderVertices.data(), generated::kRightRenderVertices.size(),
        generated::kRightTriangleIndices.data(), generated::kRightTriangleIndices.size(),
        generated::kRightLocalTranslations.data(), generated::kRightParents.data(), generated::kRightInverseBind.data(),
    };
}

bool FingerForJoint(std::size_t joint, std::size_t& finger, std::size_t& segment) noexcept {
    if (joint >= 2U && joint <= 4U) { finger = 4U; segment = joint - 2U; return true; }
    if (joint >= 5U && joint <= 7U) { finger = 3U; segment = joint - 5U; return true; }
    if (joint >= 8U && joint <= 10U) { finger = 2U; segment = joint - 8U; return true; }
    if (joint >= 11U && joint <= 13U) { finger = 1U; segment = joint - 11U; return true; }
    if (joint >= 14U && joint <= 16U) { finger = 0U; segment = joint - 14U; return true; }
    return false;
}

std::array<Matrix, 17> SkinMatrices(
    const MeshView& mesh,
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept {
    std::array<Matrix, 17> world{};
    std::array<Matrix, 17> skin{};
    for (std::size_t joint = 0; joint < world.size(); ++joint) {
        Matrix local = Translation(mesh.translations[joint]);
        std::size_t finger = 0;
        std::size_t segment = 0;
        if (FingerForJoint(joint, finger, segment)) {
            if (segment == 0U) {
                const float yaw = finger == 0U
                    ? articulation.thumb_yaw_degrees
                    : articulation.fingers[finger].spread_degrees;
                local = runtime::Multiply(local, AxisRotation(yaw, 0.0F, 1.0F, 0.0F));
            }
            const float flexion = articulation.fingers[finger].flexion_degrees[segment];
            if (finger == 0U) {
                // Exact Rework 23c890f rig axis: thumb opposition shares Y and
                // mirrors only its Z component between the two authored meshes.
                local = runtime::Multiply(local,
                    AxisRotation(flexion, 0.0F, 0.94F, left ? 0.342F : -0.342F));
            } else {
                // The old hand skin is rigidly weighted. Rework proved a single
                // local Z flex axis for all four long-finger chains.
                local = runtime::Multiply(local,
                    AxisRotation(flexion, 0.0F, 0.0F, left ? 1.0F : -1.0F));
            }
        }
        const int parent = mesh.parents[joint];
        world[joint] = parent < 0
            ? local
            : runtime::Multiply(world[static_cast<std::size_t>(parent)], local);
        skin[joint] = runtime::Multiply(world[joint], StoredMatrix(mesh.inverse_bind[joint]));
    }
    return skin;
}

GLuint HandTexture() noexcept {
    struct Cache { HGLRC context = nullptr; GLuint texture = 0; };
    thread_local Cache cache;
    const HGLRC current = wglGetCurrentContext();
    if (current == nullptr) return 0;
    if (cache.context == current && cache.texture != 0U && glIsTexture(cache.texture)) {
        return cache.texture;
    }
    cache.context = current;
    cache.texture = 0;
    glGenTextures(1, &cache.texture);
    if (cache.texture == 0U) return 0;
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    // Pixel-store state is client state and is not covered by the caller's
    // glPushAttrib(GL_ALL_ATTRIB_BITS). HPL uploads dynamic textures and may
    // leave row/skip state behind, so make this one-time renderer upload
    // deterministic and restore the host state afterwards.
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
        generated::kHandTextureWidth, generated::kHandTextureHeight,
        0, GL_RGB, GL_UNSIGNED_BYTE, generated::kHandTextureRgb.data());
    glPopClientAttrib();
    return cache.texture;
}

std::array<float, 21> PoseKey(const runtime::VrHandArticulation& articulation) noexcept {
    std::array<float, 21> key{};
    std::size_t index = 0;
    for (const auto& finger : articulation.fingers) {
        for (const float flexion : finger.flexion_degrees) key[index++] = flexion;
        key[index++] = finger.spread_degrees;
    }
    key[index] = articulation.thumb_yaw_degrees;
    return key;
}

const std::array<DrawVertex, generated::kRightRenderVertices.size()>& DrawVertices(
    const MeshView& mesh,
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept {
    struct Cache {
        std::array<float, 21> key{};
        std::array<DrawVertex, generated::kRightRenderVertices.size()> vertices{};
        bool valid = false;
    };
    thread_local std::array<Cache, 2> caches{};
    auto& cache = caches[left ? 0U : 1U];
    const auto key = PoseKey(articulation);
    if (cache.valid && cache.key == key) return cache.vertices;

    const auto skin = SkinMatrices(mesh, articulation, left);
    std::array<MeshVec3, generated::kRightPositions.size()> skinned{};
    for (std::size_t index = 0; index < mesh.position_count; ++index) {
        const std::size_t bone = mesh.bones[index];
        if (bone >= skin.size()) continue;
        skinned[index] = TransformPoint(skin[bone], mesh.positions[index]);
    }
    for (std::size_t index = 0; index < mesh.render_vertex_count; ++index) {
        const auto& corner = mesh.render_vertices[index];
        if (corner.position >= skinned.size() || corner.uv >= generated::kHandUvs.size()) continue;
        const auto& uv = generated::kHandUvs[corner.uv];
        const auto& position = skinned[corner.position];
        cache.vertices[index] = {uv.x, uv.y, position.x, position.y, position.z};
    }
    cache.key = key;
    cache.valid = true;
    return cache.vertices;
}

} // namespace

ReworkHandMeshStats GetReworkHandMeshStats(bool left) noexcept {
    const MeshView mesh = View(left);
    return {mesh.position_count, mesh.index_count / 3U, 17U};
}

bool DrawReworkHandMesh(
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept {
    if (wglGetCurrentContext() == nullptr) return false;
    const MeshView mesh = View(left);
    if (mesh.position_count == 0U || mesh.render_vertex_count == 0U ||
        mesh.index_count == 0U || mesh.index_count % 3U != 0U) return false;
    if (mesh.position_count != generated::kRightPositions.size() ||
        mesh.render_vertex_count != generated::kRightRenderVertices.size() ||
        mesh.index_count != generated::kRightTriangleIndices.size()) return false;
    const auto& vertices = DrawVertices(mesh, articulation, left);

    glPushMatrix();
    // Exact Rework hand HUD profile. HPL's XYZ helper produces Rz*Ry*Rx;
    // fixed-function calls below post-multiply in that same final order.
    glTranslatef(0.0F, 0.0F,
        runtime::rework_hand_profile::kVisualTranslationZ);
    glRotatef(runtime::rework_hand_profile::kVisualRotationZ * kRadiansToDegrees,
        0.0F, 0.0F, 1.0F);
    glRotatef(runtime::rework_hand_profile::kVisualRotationY * kRadiansToDegrees,
        0.0F, 1.0F, 0.0F);
    glScalef(runtime::rework_hand_profile::kMeshScale,
        runtime::rework_hand_profile::kMeshScale,
        runtime::rework_hand_profile::kMeshScale);

    const GLuint texture = HandTexture();
    if (texture != 0U) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
    auto& client_api = CurrentClientArrayApi();
    GLint previous_client_texture = static_cast<GLint>(kTexture0);
    GLint previous_array_buffer = 0;
    GLint previous_element_buffer = 0;
    if (client_api.active_texture) glGetIntegerv(kClientActiveTexture, &previous_client_texture);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (client_api.bind_buffer) {
        // HPL1 uses VBOs. Client pointers become byte offsets whenever one of
        // these bindings is non-zero, so isolate both before supplying our
        // renderer-owned CPU arrays and restore them after the hand draw.
        glGetIntegerv(kArrayBufferBinding, &previous_array_buffer);
        glGetIntegerv(kElementArrayBufferBinding, &previous_element_buffer);
        client_api.bind_buffer(kArrayBuffer, 0);
        client_api.bind_buffer(kElementArrayBuffer, 0);
    }
    if (client_api.active_texture) client_api.active_texture(kTexture0);
    // HPL commonly leaves VBO-backed client arrays enabled. In particular an
    // inherited color array overrides glColor4f and makes our hand indices
    // fetch unrelated per-vertex colors from the game's VBO, producing the
    // black/rainbow triangular corruption seen in-headset. Our hand renderer
    // owns only vertex + unit-0 UV arrays for this draw.
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(DrawVertex), &vertices[0].u);
    glVertexPointer(3, GL_FLOAT, sizeof(DrawVertex), &vertices[0].x);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.index_count),
        GL_UNSIGNED_SHORT, mesh.triangle_indices);
    glPopClientAttrib();
    if (client_api.bind_buffer) {
        client_api.bind_buffer(kArrayBuffer, static_cast<GLuint>(previous_array_buffer));
        client_api.bind_buffer(kElementArrayBuffer, static_cast<GLuint>(previous_element_buffer));
    }
    if (client_api.active_texture)
        client_api.active_texture(static_cast<GLenum>(previous_client_texture));
    glPopMatrix();
    return true;
}

} // namespace penumbra_vr::graphics
