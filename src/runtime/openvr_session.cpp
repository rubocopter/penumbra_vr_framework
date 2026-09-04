#include "openvr_session.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <openvr.h>

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
    shutdown_ = reinterpret_cast<void*>(shutdown);
    recommended_size_ = recommended_size;
    return true;
}

bool OpenVrSession::Shutdown(std::string& error) noexcept {
    error.clear();
    if (library_ == nullptr) {
        system_ = nullptr;
        shutdown_ = nullptr;
        recommended_size_ = {};
        return true;
    }

    if (system_ != nullptr && shutdown_ != nullptr) {
        reinterpret_cast<VrShutdownInternal>(shutdown_)();
    }
    system_ = nullptr;
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

bool OpenVrSession::initialized() const noexcept {
    return library_ != nullptr && system_ != nullptr && shutdown_ != nullptr;
}

VrRenderTargetSize OpenVrSession::recommended_render_target_size() const noexcept {
    return recommended_size_;
}

} // namespace penumbra_vr::runtime
