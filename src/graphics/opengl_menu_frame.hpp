#pragma once
#include "vr_math.hpp"
#include <string>

namespace penumbra_vr::graphics {
// Deterministically blanks the desktop backbuffer when continuous stereo owns
// presentation and the optional monitor mirror is disabled.
[[nodiscard]] bool ClearMonitorBackbuffer(std::string& error) noexcept;
// Final desktop presentation of an already rendered eye; preserves GL state.
[[nodiscard]] bool DrawMonitorMirror(unsigned int texture, std::string& error) noexcept;
// Composites an RGBA HPL 2D surface over an already-rendered eye without
// clearing color/depth. model_view is the eye-from-head transform; geometry is
// expressed in head-local metres so callers can reproduce Rework's authored UI
// plane while keeping exact-build HPL drawing outside shared graphics code.
[[nodiscard]] bool DrawTransparentOverlay(
    unsigned int texture,
    const runtime::VrMatrix44& model_view,
    const runtime::VrMatrix44& projection,
    float left,
    float right,
    float bottom,
    float top,
    float distance,
    std::string& error) noexcept;
// A transient desktop capture, owned by the render thread and shared by both
// eye passes. Destruction must happen before leaving the current GL context.
class OpenGlMenuFrame final {
public:
    OpenGlMenuFrame() = default;
    ~OpenGlMenuFrame();
    OpenGlMenuFrame(const OpenGlMenuFrame&) = delete;
    OpenGlMenuFrame& operator=(const OpenGlMenuFrame&) = delete;
    [[nodiscard]] bool Capture(std::string& error) noexcept;
    [[nodiscard]] bool Draw(const runtime::VrMatrix44& view,
                            const runtime::VrMatrix44& projection,
                            float distance,
                          float width,
                          float center_y,
                            std::string& error) const noexcept;
private:
    unsigned int texture_ = 0;
    void* context_ = nullptr;
    float aspect_ = 1.0F;
};
}
