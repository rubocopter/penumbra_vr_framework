#pragma once

#include "openvr_session.hpp"
#include "vr_action_input.hpp"
#include "vr_locomotion.hpp"
#include "vr_settings.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {

struct RequiemBodyTrackingSample final {
    void* character_body = nullptr;
    float body_center_y = 0.0F;
    float active_size_y = 0.0F;
    std::uint64_t sampled_at_ms = 0;
    std::uint64_t body_generation = 0;
    bool body_height_valid = false;
    bool native_crouched = false;
    bool physical_crouch = false;
    bool room_scale_valid = false;
    std::array<float, 3> head_anchor{};
    std::array<float, 3> body_position{};
    runtime::VrMatrix34 observed_tracking_pose{};
    runtime::VrTrackingSampleIdentity tracking_identity{};
    runtime::VrPhysicalReconciliationResult physical_reconciliation{};
};

[[nodiscard]] RequiemBodyTrackingSample ReadGameplayTrackingSample() noexcept;

[[nodiscard]] bool InstallGameplayBridge(std::string& error) noexcept;
[[nodiscard]] bool GameplayBridgeInstalled() noexcept;
[[nodiscard]] bool RemoveGameplayBridge(std::string& error) noexcept;
void ConfigureGameplaySettings(runtime::VrSettings settings) noexcept;
[[nodiscard]] bool OpenNativeControllerBindings(std::string& error) noexcept;
void ConnectGameplayInput(runtime::OpenVrSession* session) noexcept;
void PublishGameplayHeadTracking(const runtime::VrHmdPose& pose,
    float world_yaw) noexcept;
[[nodiscard]] bool NativeUiActive() noexcept;
enum class NativeUiSurface : unsigned char {
    none, fullscreen, inventory, notebook,
};
[[nodiscard]] NativeUiSurface CurrentNativeUiSurface() noexcept;
[[nodiscard]] runtime::VrControllerFrame ReadNativeControllerFrame() noexcept;

} // namespace penumbra_vr::backends::requiem
