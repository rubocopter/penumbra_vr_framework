#pragma once
#include "vr_math.hpp"
#include <string>

namespace penumbra_vr::graphics {
// Final desktop presentation of an already rendered eye; preserves GL state.
[[nodiscard]] bool DrawMonitorMirror(unsigned int texture, std::string& error) noexcept;
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
                            const runtime::VrMatrix44& projection, std::string& error) const noexcept;
private:
    unsigned int texture_ = 0;
    void* context_ = nullptr;
    float aspect_ = 1.0F;
};
}
