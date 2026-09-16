#include "rework_hand_mesh.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include "vr_math.hpp"

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
    const MeshCorner* corners = nullptr;
    std::size_t corner_count = 0;
    const MeshVec3* translations = nullptr;
    const std::int8_t* parents = nullptr;
    const std::array<float, 16>* inverse_bind = nullptr;
};

constexpr float kRadiansToDegrees = 57.29577951308232F;
constexpr float kDegreesToRadians = 0.017453292519943295F;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kClientActiveTexture = 0x84E1;
using ClientActiveTexture = void(APIENTRY*)(GLenum);

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
            generated::kLeftPositionBones.data(), generated::kLeftCorners.data(), generated::kLeftCorners.size(),
            generated::kLeftLocalTranslations.data(), generated::kLeftParents.data(), generated::kLeftInverseBind.data(),
        };
    }
    return {
        generated::kRightPositions.data(), generated::kRightPositions.size(),
        generated::kRightPositionBones.data(), generated::kRightCorners.data(), generated::kRightCorners.size(),
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
        generated::kHandTextureWidth, generated::kHandTextureHeight,
        0, GL_RGB, GL_UNSIGNED_BYTE, generated::kHandTextureRgb.data());
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

const std::array<DrawVertex, generated::kRightCorners.size()>& DrawVertices(
    const MeshView& mesh,
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept {
    struct Cache {
        std::array<float, 21> key{};
        std::array<DrawVertex, generated::kRightCorners.size()> vertices{};
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
    for (std::size_t index = 0; index < mesh.corner_count; ++index) {
        const auto& corner = mesh.corners[index];
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
    return {mesh.position_count, mesh.corner_count / 3U, 17U};
}

bool DrawReworkHandMesh(
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept {
    if (wglGetCurrentContext() == nullptr) return false;
    const MeshView mesh = View(left);
    if (mesh.position_count == 0U || mesh.corner_count == 0U || mesh.corner_count % 3U != 0U) return false;
    if (mesh.position_count != generated::kRightPositions.size() ||
        mesh.corner_count != generated::kRightCorners.size()) return false;
    const auto& vertices = DrawVertices(mesh, articulation, left);

    glPushMatrix();
    // Exact Rework hand HUD profile. HPL's XYZ helper produces Rz*Ry*Rx;
    // fixed-function calls below post-multiply in that same final order.
    glTranslatef(0.0F, 0.0F, 0.05F);
    glRotatef(-1.1F * kRadiansToDegrees, 0.0F, 0.0F, 1.0F);
    glRotatef(0.525F * kRadiansToDegrees, 0.0F, 1.0F, 0.0F);
    glScalef(0.006F, 0.006F, 0.006F);

    const GLuint texture = HandTexture();
    if (texture != 0U) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
    const auto client_active = reinterpret_cast<ClientActiveTexture>(
        wglGetProcAddress("glClientActiveTexture"));
    const auto client_active_arb = client_active ? client_active :
        reinterpret_cast<ClientActiveTexture>(wglGetProcAddress("glClientActiveTextureARB"));
    GLint previous_client_texture = static_cast<GLint>(kTexture0);
    if (client_active_arb) glGetIntegerv(kClientActiveTexture, &previous_client_texture);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (client_active_arb) client_active_arb(kTexture0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(DrawVertex), &vertices[0].u);
    glVertexPointer(3, GL_FLOAT, sizeof(DrawVertex), &vertices[0].x);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.corner_count));
    glPopClientAttrib();
    if (client_active_arb) client_active_arb(static_cast<GLenum>(previous_client_texture));
    glPopMatrix();
    return true;
}

} // namespace penumbra_vr::graphics
