#include "openvr_session.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <openvr.h>

#include <algorithm>
#include <array>

namespace penumbra_vr::runtime {
namespace {

using VrInitInternal2 = std::uint32_t(VR_CALLTYPE*)(
    vr::EVRInitError*, vr::EVRApplicationType, const char*);
using VrGetGenericInterface = std::intptr_t(VR_CALLTYPE*)(
    const char*, vr::EVRInitError*);
using VrShutdownInternal = void(VR_CALLTYPE*)();
using VrErrorDescription = const char*(VR_CALLTYPE*)(vr::EVRInitError);

template <typename Procedure>
[[nodiscard]] bool LoadProcedure(
    HMODULE library,
    const char* name,
    Procedure& destination,
    std::string& error) noexcept {
    FARPROC procedure = GetProcAddress(library, name);
    if (procedure == nullptr) {
        error = std::string("openvr_api.dll does not export ") + name;
        return false;
    }
    destination = reinterpret_cast<Procedure>(procedure);
    return true;
}

[[nodiscard]] std::string InitializationError(
    vr::EVRInitError error_code,
    VrErrorDescription describe) {
    const char* description = describe == nullptr ? nullptr : describe(error_code);
    if (description == nullptr || description[0] == '\0') {
        return "OpenVR initialization failed with code " +
            std::to_string(static_cast<int>(error_code));
    }
    return std::string("OpenVR initialization failed: ") + description;
}

void CopyMatrix(
    const vr::HmdMatrix34_t& source,
    VrMatrix34& destination) noexcept {
    std::copy_n(&source.m[0][0], destination.values.size(), destination.values.begin());
}

[[nodiscard]] std::string CompositorError(vr::EVRCompositorError error_code) {
    return "OpenVR compositor call failed with code " +
        std::to_string(static_cast<int>(error_code));
}

} // namespace

OpenVrSession::~OpenVrSession() noexcept {
    std::string ignored_error;
    static_cast<void>(Shutdown(ignored_error));
}

bool OpenVrSession::Initialize(
    const std::wstring& loader_path,
    std::string& error) noexcept {
    error.clear();
    if (initialized()) {
        error = "OpenVR is already initialized";
        return false;
    }
    if (library_ != nullptr) {
        error = "An incomplete OpenVR loader session is still active";
        return false;
    }

    HMODULE library = LoadLibraryW(loader_path.c_str());
    if (library == nullptr) {
        error = "Could not load openvr_api.dll from the probe directory; Win32 error " +
            std::to_string(GetLastError());
        return false;
    }

    VrInitInternal2 initialize = nullptr;
    VrGetGenericInterface get_interface = nullptr;
    VrShutdownInternal shutdown = nullptr;
    VrErrorDescription describe_error = nullptr;
    if (!LoadProcedure(library, "VR_InitInternal2", initialize, error) ||
        !LoadProcedure(library, "VR_GetGenericInterface", get_interface, error) ||
        !LoadProcedure(library, "VR_ShutdownInternal", shutdown, error) ||
        !LoadProcedure(
            library,
            "VR_GetVRInitErrorAsEnglishDescription",
            describe_error,
            error)) {
        FreeLibrary(library);
        return false;
    }

    vr::EVRInitError init_error = vr::VRInitError_None;
    static_cast<void>(initialize(
        &init_error, vr::VRApplication_Scene, "Penumbra VR research probe"));
    if (init_error != vr::VRInitError_None) {
        error = InitializationError(init_error, describe_error);
        shutdown();
        FreeLibrary(library);
        return false;
    }

    vr::EVRInitError interface_error = vr::VRInitError_None;
    const std::intptr_t system_address = get_interface(
        vr::IVRSystem_Version, &interface_error);
    if (interface_error != vr::VRInitError_None || system_address == 0) {
        error = InitializationError(interface_error, describe_error);
        shutdown();
        FreeLibrary(library);
        return false;
    }

    auto* system = reinterpret_cast<vr::IVRSystem*>(system_address);
    const vr::EVRInitError sdk_error = system->SetSDKVersion(
        vr::k_nSteamVRVersionMajor,
        vr::k_nSteamVRVersionMinor,
        vr::k_nSteamVRVersionBuild);
    if (sdk_error != vr::VRInitError_None) {
        error = InitializationError(sdk_error, describe_error);
        shutdown();
        FreeLibrary(library);
        return false;
    }

    interface_error = vr::VRInitError_None;
    const std::intptr_t compositor_address = get_interface(
        vr::IVRCompositor_Version, &interface_error);
    if (interface_error != vr::VRInitError_None || compositor_address == 0) {
        error = InitializationError(interface_error, describe_error);
        shutdown();
        FreeLibrary(library);
        return false;
    }
    auto* compositor = reinterpret_cast<vr::IVRCompositor*>(compositor_address);
    compositor->SetTrackingSpace(vr::TrackingUniverseStanding);

    VrRenderTargetSize recommended_size;
    system->GetRecommendedRenderTargetSize(
        &recommended_size.width, &recommended_size.height);
    if (recommended_size.width == 0 || recommended_size.height == 0) {
        error = "OpenVR returned an empty recommended render-target size";
        shutdown();
        FreeLibrary(library);
        return false;
    }

    library_ = library;
    system_ = system;
    compositor_ = compositor;
    shutdown_ = reinterpret_cast<void*>(shutdown);
    recommended_size_ = recommended_size;
    return true;
}

bool OpenVrSession::Shutdown(std::string& error) noexcept {
    error.clear();
    if (library_ == nullptr) {
        system_ = nullptr;
        compositor_ = nullptr;
        shutdown_ = nullptr;
        recommended_size_ = {};
        return true;
    }

    if (system_ != nullptr && shutdown_ != nullptr) {
        reinterpret_cast<VrShutdownInternal>(shutdown_)();
    }
    system_ = nullptr;
    compositor_ = nullptr;
    shutdown_ = nullptr;
    recommended_size_ = {};

    HMODULE library = static_cast<HMODULE>(library_);
    library_ = nullptr;
    if (!FreeLibrary(library)) {
        error = "FreeLibrary(openvr_api.dll) failed with Win32 error " +
            std::to_string(GetLastError());
        return false;
    }
    return true;
}

bool OpenVrSession::ReadEyeConfiguration(
    std::array<VrEyeConfiguration, 2>& eyes,
    std::string& error) const noexcept {
    error.clear();
    eyes = {};
    if (!initialized()) {
        error = "OpenVR is not initialized";
        return false;
    }

    auto* system = static_cast<vr::IVRSystem*>(system_);
    constexpr std::array<vr::EVREye, 2> kEyes{
        vr::Eye_Left,
        vr::Eye_Right,
    };
    for (std::size_t index = 0; index < kEyes.size(); ++index) {
        system->GetProjectionRaw(
            kEyes[index],
            &eyes[index].left_tangent,
            &eyes[index].right_tangent,
            &eyes[index].top_tangent,
            &eyes[index].bottom_tangent);
        CopyMatrix(
            system->GetEyeToHeadTransform(kEyes[index]),
            eyes[index].eye_to_head);
    }
    return true;
}

bool OpenVrSession::WaitForHmdPose(
    VrHmdPose& pose,
    std::string& error) const noexcept {
    error.clear();
    pose = {};
    if (!initialized() || compositor_ == nullptr) {
        error = "OpenVR compositor is not initialized";
        return false;
    }

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
    const vr::EVRCompositorError wait_error =
        static_cast<vr::IVRCompositor*>(compositor_)->WaitGetPoses(
            poses.data(), static_cast<std::uint32_t>(poses.size()), nullptr, 0);
    if (wait_error != vr::VRCompositorError_None) {
        error = CompositorError(wait_error);
        return false;
    }

    const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    CopyMatrix(hmd.mDeviceToAbsoluteTracking, pose.device_to_absolute);
    std::copy_n(hmd.vVelocity.v, pose.velocity.size(), pose.velocity.begin());
    std::copy_n(
        hmd.vAngularVelocity.v,
        pose.angular_velocity.size(),
        pose.angular_velocity.begin());
    pose.tracking_result = static_cast<std::uint32_t>(hmd.eTrackingResult);
    pose.pose_valid = hmd.bPoseIsValid;
    pose.device_connected = hmd.bDeviceIsConnected;
    return true;
}

bool OpenVrSession::initialized() const noexcept {
    return library_ != nullptr && system_ != nullptr && compositor_ != nullptr &&
        shutdown_ != nullptr;
}

VrRenderTargetSize OpenVrSession::recommended_render_target_size() const noexcept {
    return recommended_size_;
}

} // namespace penumbra_vr::runtime
