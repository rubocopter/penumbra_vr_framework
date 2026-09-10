#pragma once

#include "overture_backend.hpp"

#include <string>

class cInit;
class cPlayer;

namespace hpl {
struct cVRInputState;
}

namespace penumbra_vr::adapters::overture_source {

// Source-level Overture boundary. This class is compiled into the Overture
// executable, where it maps the existing HPL types to the shared backend.
class OvertureSourceIntegration final {
public:
    void Reset() noexcept;

    [[nodiscard]] bool HandleInput(
        cPlayer& player,
        cInit& init,
        const hpl::cVRInputState& input,
        float delta_seconds,
        bool gameplay_active,
        bool player_update_expected,
        std::string& error) noexcept;

    [[nodiscard]] bool UpdatePlayer(
        cPlayer& player,
        cInit& init,
        float delta_seconds,
        bool body_motion_enabled,
        bool constrained_movement,
        std::string& error) noexcept;

private:
    [[nodiscard]] bool EnsureInitialized(
        cPlayer& player,
        cInit& init,
        std::string& error) noexcept;

    backends::overture::OvertureBackend backend_;
    bool initialized_ = false;
    bool input_pending_for_player_ = false;
};

} // namespace penumbra_vr::adapters::overture_source
