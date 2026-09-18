#include "audio_environment_probe.hpp"

#include "audio_environment_policy.hpp"
#include "rel32_call_hook.hpp"
#include "spatial_audio.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

// Exact-build data for FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.
constexpr std::uintptr_t kLowLevelSoundVtableRva = 0x287D98;
constexpr std::uintptr_t kSetEnvVolumeRva = 0x15B510;
constexpr std::uintptr_t kInitialAttachCallRva = 0x15C26A;
constexpr std::uintptr_t kInitialAttachTargetRva = 0x18CA80;
constexpr std::uintptr_t kSetEnvGainCallRva = 0x15B557;
constexpr std::uintptr_t kSetEnvGainTargetRva = 0x18CAC0;
constexpr std::uintptr_t kInitializedOffset = 0x3E1;
constexpr std::uintptr_t kEnvAudioEnabledOffset = 0x0E;
constexpr std::uintptr_t kEnvVolumeOffset = 0x08;
constexpr std::uintptr_t kEffectOffset = 0x3EC;

constexpr std::uintptr_t kDensityRva = 0x18C480;
constexpr std::uintptr_t kDiffusionRva = 0x18C4B0;
constexpr std::uintptr_t kGainRva = 0x18C4E0;
constexpr std::uintptr_t kGainHfRva = 0x18C510;
constexpr std::uintptr_t kGainLfRva = 0x18C540;
constexpr std::uintptr_t kDecayTimeRva = 0x18C570;
constexpr std::uintptr_t kDecayHfRatioRva = 0x18C5A0;
constexpr std::uintptr_t kDecayLfRatioRva = 0x18C5D0;
constexpr std::uintptr_t kReflectionsGainRva = 0x18C600;
constexpr std::uintptr_t kReflectionsDelayRva = 0x18C630;
constexpr std::uintptr_t kLateReverbGainRva = 0x18C660;
constexpr std::uintptr_t kLateReverbDelayRva = 0x18C690;
constexpr std::uintptr_t kEchoTimeRva = 0x18C6C0;
constexpr std::uintptr_t kEchoDepthRva = 0x18C6F0;
constexpr std::uintptr_t kModulationTimeRva = 0x18C720;
constexpr std::uintptr_t kModulationDepthRva = 0x18C750;
constexpr std::uintptr_t kAirAbsorptionGainHfRva = 0x18C780;
constexpr std::uintptr_t kHfReferenceRva = 0x18C7B0;
constexpr std::uintptr_t kLfReferenceRva = 0x18C7E0;
constexpr std::uintptr_t kRoomRolloffFactorRva = 0x18C810;

// cOAL_Effect_Reverb fields proven from the matching OALWrapper source and
// exact-build setter disassembly. The base object occupies 0x14 bytes.
constexpr std::uintptr_t kEffectDensityOffset = 0x14;
constexpr std::uintptr_t kEffectDiffusionOffset = 0x18;
constexpr std::uintptr_t kEffectGainOffset = 0x1C;
constexpr std::uintptr_t kEffectGainHfOffset = 0x20;
constexpr std::uintptr_t kEffectGainLfOffset = 0x24;
constexpr std::uintptr_t kEffectDecayTimeOffset = 0x28;
constexpr std::uintptr_t kEffectDecayHfRatioOffset = 0x2C;
constexpr std::uintptr_t kEffectDecayLfRatioOffset = 0x30;
constexpr std::uintptr_t kEffectReflectionsGainOffset = 0x34;
constexpr std::uintptr_t kEffectReflectionsDelayOffset = 0x38;
constexpr std::uintptr_t kEffectLateReverbGainOffset = 0x48;
constexpr std::uintptr_t kEffectLateReverbDelayOffset = 0x4C;
constexpr std::uintptr_t kEffectEchoTimeOffset = 0x5C;
constexpr std::uintptr_t kEffectEchoDepthOffset = 0x60;
constexpr std::uintptr_t kEffectModulationTimeOffset = 0x64;
constexpr std::uintptr_t kEffectModulationDepthOffset = 0x68;
constexpr std::uintptr_t kEffectAirAbsorptionGainHfOffset = 0x6C;
constexpr std::uintptr_t kEffectHfReferenceOffset = 0x70;
constexpr std::uintptr_t kEffectLfReferenceOffset = 0x74;
constexpr std::uintptr_t kEffectRoomRolloffFactorOffset = 0x78;

using AttachEffect = bool(__cdecl*)(int, void*);
using SetSlotGain = void(__cdecl*)(int, float);
using ReverbSetter = void(__cdecl*)(void*, float);
using SetEnvVolume = void(__thiscall*)(void*, float);

std::uint8_t* g_image = nullptr;
hooks::Rel32CallHook g_attach_hook;
hooks::Rel32CallHook g_gain_hook;
std::atomic<bool> g_initial_bus_trim_pending{false};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
AudioEnvironmentTelemetry g_telemetry;

template<class T>
bool ReadValue(const void* address, T& value) noexcept {
    if (address == nullptr) return false;
    __try {
        std::memcpy(&value, address, sizeof(value));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::array<std::uint8_t, 5> CallBytes(
    std::uintptr_t call_rva,
    std::uintptr_t target_rva) noexcept {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(
        target_rva - call_rva - bytes.size());
    std::memcpy(bytes.data() + 1, &displacement, sizeof(displacement));
    return bytes;
}

bool ValidateStaticBoundaries(std::string& error) noexcept {
    const auto expected_attach = CallBytes(
        kInitialAttachCallRva, kInitialAttachTargetRva);
    const auto expected_gain = CallBytes(kSetEnvGainCallRva, kSetEnvGainTargetRva);
    std::array<std::uint8_t, 5> actual_attach{};
    std::array<std::uint8_t, 5> actual_gain{};
    std::uintptr_t set_env_volume = 0;
    std::uintptr_t set_environment = 0;
    if (!ReadValue(g_image + kInitialAttachCallRva, actual_attach) ||
        !ReadValue(g_image + kSetEnvGainCallRva, actual_gain) ||
        !ReadValue(g_image + kLowLevelSoundVtableRva + 0x28, set_env_volume) ||
        !ReadValue(g_image + kLowLevelSoundVtableRva + 0x30, set_environment)) {
        error = "Audio environment exact-build boundaries are unreadable";
        return false;
    }
    if (actual_attach != expected_attach || actual_gain != expected_gain ||
        set_env_volume != reinterpret_cast<std::uintptr_t>(g_image + kSetEnvVolumeRva) ||
        set_environment != reinterpret_cast<std::uintptr_t>(g_image + 0x15B570)) {
        error = "Audio environment exact-build boundary mismatch";
        return false;
    }
    return true;
}

void Record(const auto& operation) noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry.valid = true;
    operation(g_telemetry);
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

void SetReverb(void* effect, std::uintptr_t rva, float value) noexcept {
    reinterpret_cast<ReverbSetter>(g_image + rva)(effect, value);
}

void ApplyMineGalleryPreset(void* effect) noexcept {
    const auto& p = audio::kMineGalleryReverb;
    SetReverb(effect, kDensityRva, p.density);
    SetReverb(effect, kDiffusionRva, p.diffusion);
    SetReverb(effect, kGainRva, p.gain);
    SetReverb(effect, kGainHfRva, p.gain_hf);
    SetReverb(effect, kGainLfRva, p.gain_lf);
    SetReverb(effect, kDecayTimeRva, p.decay_time);
    SetReverb(effect, kDecayHfRatioRva, p.decay_hf_ratio);
    SetReverb(effect, kDecayLfRatioRva, p.decay_lf_ratio);
    SetReverb(effect, kReflectionsGainRva, p.reflections_gain);
    SetReverb(effect, kReflectionsDelayRva, p.reflections_delay);
    SetReverb(effect, kLateReverbGainRva, p.late_reverb_gain);
    SetReverb(effect, kLateReverbDelayRva, p.late_reverb_delay);
    SetReverb(effect, kEchoTimeRva, p.echo_time);
    SetReverb(effect, kEchoDepthRva, p.echo_depth);
    SetReverb(effect, kModulationTimeRva, p.modulation_time);
    SetReverb(effect, kModulationDepthRva, p.modulation_depth);
    SetReverb(effect, kAirAbsorptionGainHfRva, p.air_absorption_gain_hf);
    SetReverb(effect, kHfReferenceRva, p.hf_reference);
    SetReverb(effect, kLfReferenceRva, p.lf_reference);
    SetReverb(effect, kRoomRolloffFactorRva, p.room_rolloff_factor);
}

bool ReadNativeReverbState(void* effect, NativeReverbState& state) noexcept {
    if (effect == nullptr) return false;
    auto* bytes = static_cast<std::uint8_t*>(effect);
    return ReadValue(bytes + kEffectDensityOffset, state.density) &&
        ReadValue(bytes + kEffectDiffusionOffset, state.diffusion) &&
        ReadValue(bytes + kEffectGainOffset, state.gain) &&
        ReadValue(bytes + kEffectGainHfOffset, state.gain_hf) &&
        ReadValue(bytes + kEffectGainLfOffset, state.gain_lf) &&
        ReadValue(bytes + kEffectDecayTimeOffset, state.decay_time) &&
        ReadValue(bytes + kEffectDecayHfRatioOffset, state.decay_hf_ratio) &&
        ReadValue(bytes + kEffectDecayLfRatioOffset, state.decay_lf_ratio) &&
        ReadValue(bytes + kEffectReflectionsGainOffset, state.reflections_gain) &&
        ReadValue(bytes + kEffectReflectionsDelayOffset, state.reflections_delay) &&
        ReadValue(bytes + kEffectLateReverbGainOffset, state.late_reverb_gain) &&
        ReadValue(bytes + kEffectLateReverbDelayOffset, state.late_reverb_delay) &&
        ReadValue(bytes + kEffectEchoTimeOffset, state.echo_time) &&
        ReadValue(bytes + kEffectEchoDepthOffset, state.echo_depth) &&
        ReadValue(bytes + kEffectModulationTimeOffset, state.modulation_time) &&
        ReadValue(bytes + kEffectModulationDepthOffset, state.modulation_depth) &&
        ReadValue(bytes + kEffectAirAbsorptionGainHfOffset,
            state.air_absorption_gain_hf) &&
        ReadValue(bytes + kEffectHfReferenceOffset, state.hf_reference) &&
        ReadValue(bytes + kEffectLfReferenceOffset, state.lf_reference) &&
        ReadValue(bytes + kEffectRoomRolloffFactorOffset,
            state.room_rolloff_factor);
}

bool IsReadablePrivateRegion(const MEMORY_BASIC_INFORMATION& info) noexcept {
    if (info.State != MEM_COMMIT || info.Type != MEM_PRIVATE ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD base = info.Protect & 0xFFU;
    return base == PAGE_READONLY || base == PAGE_READWRITE ||
        base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ ||
        base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsLowLevelSoundCandidate(std::uint8_t* candidate) noexcept {
    std::uintptr_t vtable = 0;
    bool initialized = false;
    bool env_enabled = false;
    void* effect = nullptr;
    if (!ReadValue(candidate, vtable) ||
        !ReadValue(candidate + kInitializedOffset, initialized) ||
        !ReadValue(candidate + kEnvAudioEnabledOffset, env_enabled) ||
        !ReadValue(candidate + kEffectOffset, effect)) {
        return false;
    }
    if (vtable != reinterpret_cast<std::uintptr_t>(
            g_image + kLowLevelSoundVtableRva) ||
        !initialized || !env_enabled || effect == nullptr) {
        return false;
    }
    NativeReverbState state{};
    return ReadNativeReverbState(effect, state) &&
        std::isfinite(state.density) && state.density >= 0.0F &&
        state.density <= 1.0F && std::isfinite(state.hf_reference) &&
        state.hf_reference >= 1000.0F && state.hf_reference <= 20000.0F;
}

void* FindLiveLowLevelSoundObject() noexcept {
    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const auto maximum = reinterpret_cast<std::uintptr_t>(
        system_info.lpMaximumApplicationAddress);
    const auto needle = reinterpret_cast<std::uintptr_t>(
        g_image + kLowLevelSoundVtableRva);
    std::uint8_t* match = nullptr;
    std::size_t matches = 0;

    std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(
        system_info.lpMinimumApplicationAddress);
    while (cursor < maximum) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),
                &info, sizeof(info)) == 0 || info.RegionSize == 0) {
            break;
        }
        const auto region_begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const auto region_end = region_begin + info.RegionSize;
        if (IsReadablePrivateRegion(info) && region_end > region_begin) {
            auto scan = (region_begin + alignof(std::uintptr_t) - 1U) &
                ~(static_cast<std::uintptr_t>(alignof(std::uintptr_t)) - 1U);
            for (; scan + sizeof(std::uintptr_t) <= region_end;
                 scan += alignof(std::uintptr_t)) {
                std::uintptr_t value = 0;
                std::memcpy(&value, reinterpret_cast<const void*>(scan), sizeof(value));
                if (value != needle) continue;
                auto* candidate = reinterpret_cast<std::uint8_t*>(scan);
                if (!IsLowLevelSoundCandidate(candidate)) continue;
                match = candidate;
                if (++matches > 1) return nullptr;
            }
        }
        if (region_end <= cursor) break;
        cursor = region_end;
    }
    return matches == 1 ? match : nullptr;
}

bool ApplyLateBootstrap(void* object) noexcept {
    if (object == nullptr) return false;
    auto* bytes = static_cast<std::uint8_t*>(object);
    bool env_enabled = false;
    void* effect = nullptr;
    if (!ReadValue(bytes + kEnvAudioEnabledOffset, env_enabled) ||
        !ReadValue(bytes + kEffectOffset, effect) || !env_enabled || effect == nullptr) {
        Record([](auto& data) { data.late_object_found = true; });
        return true;
    }

    NativeReverbState state{};
    if (!ReadNativeReverbState(effect, state)) return false;
    const bool native_default = IsNativeDefaultReverb(state);
    if (native_default) {
        ApplyMineGalleryPreset(effect);
    }
    reinterpret_cast<SetEnvVolume>(g_image + kSetEnvVolumeRva)(
        object, audio::kMineGalleryReverb.bus_gain);
    Record([&](auto& data) {
        data.late_object_found = true;
        data.late_native_default = native_default;
        data.bus_trim_applied = true;
        if (native_default) {
            ++data.preset_applications;
            ++data.late_bootstrap_applications;
        } else {
            ++data.late_existing_environment_preserved;
        }
    });
    return true;
}

bool __cdecl HookInitialAttach(int slot, void* effect) {
    const bool attached = reinterpret_cast<AttachEffect>(
        g_image + kInitialAttachTargetRva)(slot, effect);
    if (attached && slot == 0 && effect != nullptr) {
        ApplyMineGalleryPreset(effect);
        g_initial_bus_trim_pending.store(true, std::memory_order_release);
        Record([](auto& data) {
            ++data.init_attach_calls;
            ++data.preset_applications;
        });
    }
    return attached;
}

void __cdecl HookSetEnvGain(int slot, float gain) {
    float effective_gain = gain;
    if (slot == 0 && g_initial_bus_trim_pending.exchange(
            false, std::memory_order_acq_rel)) {
        effective_gain = audio::kMineGalleryReverb.bus_gain;
        Record([](auto& data) {
            data.bus_trim_applied = true;
            ++data.initial_bus_trim_substitutions;
        });
    }
    reinterpret_cast<SetSlotGain>(g_image + kSetEnvGainTargetRva)(slot, effective_gain);
}

} // namespace

bool InstallAudioEnvironmentProbe(std::string& error) noexcept {
    error.clear();
    if (g_attach_hook.installed() && g_gain_hook.installed()) return true;
    if (g_attach_hook.installed() || g_gain_hook.installed()) {
        error = "Audio environment probe is only partially installed";
        return false;
    }

    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    g_image = image;
    if (!ValidateStaticBoundaries(error)) return false;

    g_initial_bus_trim_pending.store(false, std::memory_order_release);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    if (!hooks::InstallRel32CallHook(
            g_image + kInitialAttachCallRva,
            CallBytes(kInitialAttachCallRva, kInitialAttachTargetRva),
            reinterpret_cast<void*>(&HookInitialAttach), g_attach_hook, error)) {
        return false;
    }
    if (!hooks::InstallRel32CallHook(
            g_image + kSetEnvGainCallRva,
            CallBytes(kSetEnvGainCallRva, kSetEnvGainTargetRva),
            reinterpret_cast<void*>(&HookSetEnvGain), g_gain_hook, error)) {
        std::string rollback;
        static_cast<void>(hooks::RemoveRel32CallHook(g_attach_hook, rollback));
        if (!rollback.empty()) error += "; rollback failed: " + rollback;
        return false;
    }

    Record([](auto& data) { data.valid = true; });
    if (void* object = FindLiveLowLevelSoundObject(); object != nullptr &&
        !ApplyLateBootstrap(object)) {
        error = "The live Black Plague OpenAL object was found but its reverb state was unreadable";
        std::string rollback;
        static_cast<void>(RemoveAudioEnvironmentProbe(rollback));
        if (!rollback.empty()) error += "; rollback failed: " + rollback;
        return false;
    }
    return true;
}

bool RemoveAudioEnvironmentProbe(std::string& error) noexcept {
    error.clear();
    g_initial_bus_trim_pending.store(false, std::memory_order_release);
    bool success = true;
    std::string next;
    if (!hooks::RemoveRel32CallHook(g_gain_hook, next)) {
        success = false;
        error = next;
    }
    next.clear();
    if (!hooks::RemoveRel32CallHook(g_attach_hook, next)) {
        success = false;
        if (!error.empty()) error += "; ";
        error += next;
    }
    return success;
}

AudioEnvironmentTelemetry ReadAudioEnvironmentTelemetry() noexcept {
    AcquireSRWLockShared(&g_telemetry_lock);
    const auto result = g_telemetry;
    ReleaseSRWLockShared(&g_telemetry_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
