#include "openvr_session.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <openvr.h>

#include <algorithm>
#include <filesystem>

namespace penumbra_vr::runtime {
namespace {
bool Check(vr::EVRInputError code, const char* operation, std::string& error) {
    if (code == vr::VRInputError_None) return true;
    error = std::string(operation) + " failed with OpenVR input error " + std::to_string(static_cast<int>(code));
    return false;
}
class OpenVrActionBackend final : public VrActionBackend {
public:
    explicit OpenVrActionBackend(void* input) : input_(*static_cast<vr::IVRInput*>(input)) {}
    bool Manifest(const char* path, std::string& error) override {
        return Check(input_.SetActionManifestPath(path), "SetActionManifestPath", error);
    }
    bool Resolve(const char* path, VrHandleKind kind, VrActionHandle& handle, std::string& error) override {
        const auto code = kind == VrHandleKind::action_set ? input_.GetActionSetHandle(path, &handle) :
            kind == VrHandleKind::source ? input_.GetInputSourceHandle(path, &handle) : input_.GetActionHandle(path, &handle);
        return Check(code, path, error);
    }
    bool Activate(std::span<const VrActiveSet> sets, std::string& error) override {
        std::array<vr::VRActiveActionSet_t, 3> active{};
        if (sets.size() > active.size()) { error = "Too many active input sets"; return false; }
        for (std::size_t i = 0; i < sets.size(); ++i) {
            active[i].ulActionSet = sets[i].set;
            active[i].ulRestrictedToDevice = sets[i].restricted_source;
        }
        return Check(input_.UpdateActionState(active.data(), sizeof(active[0]),
            static_cast<std::uint32_t>(sets.size())), "UpdateActionState", error);
    }
    bool Digital(VrActionHandle handle, VrButtonState& result, std::string& error) override {
        vr::InputDigitalActionData_t data{};
        if (!Check(input_.GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle),
            "GetDigitalActionData", error)) return false;
        result = MakeVrButtonState(data.bActive, data.bState, data.bChanged);
        return true;
    }
    bool Analog(VrActionHandle handle, VrAnalogState& result, std::string& error) override {
        vr::InputAnalogActionData_t data{};
        if (!Check(input_.GetAnalogActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle),
            "GetAnalogActionData", error)) return false;
        result = data.bActive ? VrAnalogState{true, data.x, data.y} : VrAnalogState{};
        return true;
    }
    bool Pose(VrActionHandle handle, VrHmdPose& result) override {
        vr::InputPoseActionData_t data{};
        if (input_.GetPoseActionDataForNextFrame(handle, vr::TrackingUniverseStanding,
                &data, sizeof(data), vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None || !data.bActive) return false;
        const auto& pose = data.pose;
        std::copy_n(&pose.mDeviceToAbsoluteTracking.m[0][0], 12, result.device_to_absolute.values.begin());
        std::copy_n(pose.vVelocity.v, 3, result.velocity.begin());
        std::copy_n(pose.vAngularVelocity.v, 3, result.angular_velocity.begin());
        result.pose_valid = pose.bPoseIsValid;
        result.device_connected = pose.bDeviceIsConnected;
        result.tracking_result = static_cast<std::uint32_t>(pose.eTrackingResult);
        return true;
    }
    bool Skeleton(VrActionHandle handle, std::array<float, 5>& curl) override {
        vr::InputSkeletalActionData_t action{};
        if (input_.GetSkeletalActionData(handle, &action, sizeof(action)) != vr::VRInputError_None || !action.bActive) return false;
        vr::VRSkeletalSummaryData_t data{};
        if (input_.GetSkeletalSummaryData(handle, vr::VRSummaryType_FromDevice, &data) != vr::VRInputError_None &&
            input_.GetSkeletalSummaryData(handle, vr::VRSummaryType_FromAnimation, &data) != vr::VRInputError_None) return false;
        std::copy_n(data.flFingerCurl, curl.size(), curl.begin());
        return true;
    }
    bool Haptic(VrActionHandle handle, float duration, float frequency, float amplitude, std::string& error) override {
        return Check(input_.TriggerHapticVibrationAction(handle, 0, duration, frequency,
            amplitude, vr::k_ulInvalidInputValueHandle), "TriggerHapticVibrationAction", error);
    }
private:
    vr::IVRInput& input_;
};
} // namespace

bool OpenVrSession::InitializeControllerInput(const std::wstring& manifest_path, std::string& error) noexcept {
    error.clear();
    if (!initialized()) { error = "OpenVR is not initialized"; return false; }
    if (controller_input_initialized()) { error = "Controller input is already initialized"; return false; }
    const std::filesystem::path path(manifest_path);
    std::error_code filesystem_error;
    if (!path.is_absolute() || !std::filesystem::is_regular_file(path, filesystem_error) || filesystem_error) {
        error = "Controller manifest must be an existing absolute file"; return false;
    }
    using GetInterface = std::intptr_t(VR_CALLTYPE*)(const char*, vr::EVRInitError*);
    const auto get_interface = reinterpret_cast<GetInterface>(
        GetProcAddress(static_cast<HMODULE>(library_), "VR_GetGenericInterface"));
    if (get_interface == nullptr) { error = "OpenVR interface export is unavailable"; return false; }
    vr::EVRInitError code = vr::VRInitError_None;
    void* input = reinterpret_cast<void*>(get_interface(vr::IVRInput_Version, &code));
    if (code != vr::VRInitError_None || input == nullptr) {
        error = "OpenVR input interface is unavailable: " + std::to_string(static_cast<int>(code)); return false;
    }
    const auto utf8 = path.u8string();
    OpenVrActionBackend backend(input);
    if (!actions_.Initialize(backend, reinterpret_cast<const char*>(utf8.c_str()), error)) return false;
    input_ = input;
    return true;
}

bool OpenVrSession::ReadControllerInput(VrInputContext context, VrHand handedness,
    std::uint64_t now_ms, VrControllerFrame& frame, std::string& error) noexcept {
    frame = {};
    error.clear();
    if (!controller_input_initialized()) { error = "Controller input is not initialized"; return false; }
    OpenVrActionBackend backend(input_);
    return actions_.Update(backend, context, handedness,
        static_cast<vr::IVRSystem*>(system_)->IsInputAvailable(), now_ms, frame, error);
}

bool OpenVrSession::TriggerHaptic(VrHand hand, float duration, float frequency,
    float amplitude, std::string& error) noexcept {
    if (!controller_input_initialized()) { error = "Controller input is not initialized"; return false; }
    if (!static_cast<vr::IVRSystem*>(system_)->IsInputAvailable()) { error = "VR input focus is unavailable"; return false; }
    OpenVrActionBackend backend(input_);
    return actions_.TriggerHaptic(backend, hand, duration, frequency, amplitude, error);
}

void OpenVrSession::SetControllerMoveDeadZone(float dead_zone) noexcept {
    actions_.SetMoveDeadZone(dead_zone);
}

bool OpenVrSession::controller_input_initialized() const noexcept {
    return initialized() && input_ != nullptr && actions_.initialized();
}
} // namespace penumbra_vr::runtime
