#include "movement_ownership_probe.hpp"

#include "body_collision_probe.hpp"
#include "native_input_bridge.hpp"
#include "rel32_call_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

using NoArg = void(__thiscall*)(void*);
using BoolArg = void(__thiscall*)(void*, bool);

enum class SiteKind {
    jump,
    jump_hold,
    sprint_start,
    sprint_stop,
    crouch_pressed,
    crouch_release_or_not_held,
    gravity_disabled_camera_sync,
    gravity_disabled_entity_sync,
};

struct Site {
    const char* name;
    std::uintptr_t call;
    std::uintptr_t target;
    SiteKind kind;
    std::array<std::uint8_t, 16> target_signature;
};

// Every call/target pair below is decoded from the initialized exact-build
// capture FD316F... . 0x52CD is the preceding "push 1"; the held-jump CALL
// itself starts at 0x52CF.
constexpr std::array<Site, 8> kSites{{
    {"jump", 0x52A5, 0x9CEA0, SiteKind::jump,
        {0x56, 0x8B, 0xF1, 0x8B, 0x8E, 0xC4, 0x02, 0x00,
         0x00, 0x8B, 0x86, 0xBC, 0x02, 0x00, 0x00, 0x8D}},
    {"jump_hold", 0x52CF, 0x9A890, SiteKind::jump_hold,
        {0x8A, 0x44, 0x24, 0x04, 0x84, 0xC0, 0x56, 0x8B,
         0xF1, 0x88, 0x86, 0xFC, 0x01, 0x00, 0x00, 0x74}},
    {"sprint_start", 0x52F7, 0x9CF40, SiteKind::sprint_start,
        {0x8A, 0x81, 0x6C, 0x02, 0x00, 0x00, 0x84, 0xC0,
         0x75, 0x16, 0x8B, 0x81, 0xBC, 0x02, 0x00, 0x00}},
    {"sprint_stop", 0x531F, 0x9CF70, SiteKind::sprint_stop,
        {0x8A, 0x81, 0x6C, 0x02, 0x00, 0x00, 0x84, 0xC0,
         0x75, 0x16, 0x8B, 0x81, 0xBC, 0x02, 0x00, 0x00}},
    {"crouch_pressed", 0x5347, 0x9CFA0, SiteKind::crouch_pressed,
        {0x8A, 0x81, 0x6C, 0x02, 0x00, 0x00, 0x84, 0xC0,
         0x75, 0x16, 0x8B, 0x81, 0xBC, 0x02, 0x00, 0x00}},
    {"crouch_release_or_not_held", 0x538A, 0x9CFD0, SiteKind::crouch_release_or_not_held,
        {0x8A, 0x81, 0x6C, 0x02, 0x00, 0x00, 0x84, 0xC0,
         0x75, 0x16, 0x8B, 0x81, 0xBC, 0x02, 0x00, 0x00}},
    {"gravity_disabled_camera_sync", 0xD790C, 0xD5F00, SiteKind::gravity_disabled_camera_sync,
        {0x83, 0xEC, 0x28, 0x56, 0x8B, 0xF1, 0x8B, 0x86,
         0x10, 0x01, 0x00, 0x00, 0x85, 0xC0, 0x0F, 0x84}},
    {"gravity_disabled_entity_sync", 0xD7913, 0xD6120, SiteKind::gravity_disabled_entity_sync,
        {0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00, 0x53, 0x8B,
         0xD9, 0x8B, 0x83, 0x30, 0x01, 0x00, 0x00, 0x85}},
}};

std::uint8_t* g_image = nullptr;
std::array<hooks::Rel32CallHook, kSites.size()> g_hooks;
SRWLOCK g_lock = SRWLOCK_INIT;
MovementOwnershipTelemetry g_data;
std::atomic<std::uint64_t> g_sequence{0};

constexpr std::uintptr_t kPlayerCharacterBodyOffset = 0x274;
constexpr std::uintptr_t kCharacterCameraOffset = 0x110;

bool ReadBytes(const void* source, void* destination, std::size_t size) noexcept {
    if (source == nullptr || destination == nullptr) {
        return false;
    }
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<class T>
T Read(void* object, std::uintptr_t offset) noexcept {
    T value{};
    if (object != nullptr) {
        static_cast<void>(ReadBytes(static_cast<std::uint8_t*>(object) + offset,
            &value, sizeof(value)));
    }
    return value;
}

std::array<std::uint8_t, 5> CallBytes(
    std::uintptr_t call,
    std::uintptr_t target) noexcept {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(target - call - 5);
    std::memcpy(bytes.data() + 1, &displacement, sizeof(displacement));
    return bytes;
}

std::uintptr_t DecodeTarget(const std::array<std::uint8_t, 5>& bytes,
    std::uintptr_t call) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
    return call + 5 + displacement;
}

template<std::size_t Size>
void AppendHex(char (&destination)[Size], const std::uint8_t* bytes,
    std::size_t count) noexcept {
    std::size_t used = 0;
    for (std::size_t index = 0; index < count && used + 3 < Size; ++index) {
        const int written = std::snprintf(destination + used, Size - used,
            index == 0 ? "%02X" : " %02X", bytes[index]);
        if (written <= 0) {
            break;
        }
        used += static_cast<std::size_t>(written);
    }
}

bool ValidateSite(const Site& site, std::string& error) noexcept {
    const auto expected_call = CallBytes(site.call, site.target);
    std::array<std::uint8_t, 5> actual_call{};
    std::array<std::uint8_t, 16> actual_signature{};
    if (!ReadBytes(g_image + site.call, actual_call.data(), actual_call.size()) ||
        !ReadBytes(g_image + site.target, actual_signature.data(),
            actual_signature.size())) {
        error = std::string("Movement ownership ") + site.name +
            " cannot read exact-build bytes at call RVA 0x";
        char rva[16]{};
        std::snprintf(rva, sizeof(rva), "%05lX",
            static_cast<unsigned long>(site.call));
        error += rva;
        return false;
    }

    if (actual_call != expected_call) {
        char expected[16]{};
        char actual[16]{};
        AppendHex(expected, expected_call.data(), expected_call.size());
        AppendHex(actual, actual_call.data(), actual_call.size());
        char detail[256]{};
        std::snprintf(detail, sizeof(detail),
            "Movement ownership %s call mismatch at RVA 0x%05lX: "
            "expected [%s] -> 0x%05lX, actual [%s] -> 0x%05lX",
            site.name, static_cast<unsigned long>(site.call), expected,
            static_cast<unsigned long>(site.target), actual,
            static_cast<unsigned long>(DecodeTarget(actual_call, site.call)));
        error = detail;
        return false;
    }
    if (actual_signature != site.target_signature) {
        char expected[48]{};
        char actual[48]{};
        AppendHex(expected, site.target_signature.data(),
            site.target_signature.size());
        AppendHex(actual, actual_signature.data(), actual_signature.size());
        char detail[256]{};
        std::snprintf(detail, sizeof(detail),
            "Movement ownership %s target signature mismatch: call RVA "
            "0x%05lX -> target RVA 0x%05lX, expected [%s], actual [%s]",
            site.name, static_cast<unsigned long>(site.call),
            static_cast<unsigned long>(site.target), expected, actual);
        error = detail;
        return false;
    }
    return true;
}

void Record(SiteKind kind) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_data.valid = true;
    g_data.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    // Refresh this exact chain on every action callback. The two camera-sync
    // sites below are conditional and are not reached by the active gravity
    // player path, so they cannot be the source of player/body identity.
    void* player = NativePlayerPointer();
    void* body = Read<void*>(player, kPlayerCharacterBodyOffset);
    g_data.player = reinterpret_cast<std::uintptr_t>(player);
    g_data.body = reinterpret_cast<std::uintptr_t>(body);
    g_data.camera = reinterpret_cast<std::uintptr_t>(
        Read<void*>(body, kCharacterCameraOffset));
    switch (kind) {
    case SiteKind::jump: ++g_data.jump; break;
    case SiteKind::jump_hold: ++g_data.jump_hold; break;
    case SiteKind::sprint_start: ++g_data.sprint_start; break;
    case SiteKind::sprint_stop: ++g_data.sprint_stop; break;
    case SiteKind::crouch_pressed: ++g_data.crouch_pressed; break;
    case SiteKind::crouch_release_or_not_held: ++g_data.crouch_release_or_not_held; break;
    case SiteKind::gravity_disabled_camera_sync: ++g_data.gravity_disabled_camera_sync; break;
    case SiteKind::gravity_disabled_entity_sync: ++g_data.gravity_disabled_entity_sync; break;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void __fastcall HookNoArg(void* self, void*) {
    const auto return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
        reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& site : kSites) {
        if (site.call + 5 != return_rva) {
            continue;
        }
        reinterpret_cast<NoArg>(g_image + site.target)(self);
        Record(site.kind);
        if (site.kind == SiteKind::jump) {
            // The original dispatch has committed the state transition before
            // the next native body tick is captured.
            RequestBodyJumpBurst();
        }
        return;
    }
}

void __fastcall HookBool(void* self, void*, bool value) {
    reinterpret_cast<BoolArg>(g_image + 0x9A890)(self, value);
    Record(SiteKind::jump_hold);
}

bool Install(std::string& error) noexcept {
    error.clear();
    if (g_hooks[0].installed()) {
        return true;
    }
    g_image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (g_image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }

    // Validate all sites before changing any instruction. This preserves the
    // exact-build fail-closed contract and prevents partial ownership.
    for (const auto& site : kSites) {
        if (!ValidateSite(site, error)) {
            g_image = nullptr;
            return false;
        }
    }
    for (std::size_t index = 0; index < kSites.size(); ++index) {
        const auto& site = kSites[index];
        void* replacement = site.kind == SiteKind::jump_hold
            ? reinterpret_cast<void*>(&HookBool)
            : reinterpret_cast<void*>(&HookNoArg);
        if (!hooks::InstallRel32CallHook(g_image + site.call,
                CallBytes(site.call, site.target), replacement,
                g_hooks[index], error)) {
            std::string rollback;
            while (index > 0) {
                --index;
                static_cast<void>(hooks::RemoveRel32CallHook(
                    g_hooks[index], rollback));
            }
            g_image = nullptr;
            if (!rollback.empty()) {
                error += "; rollback failed: " + rollback;
            }
            return false;
        }
    }
    return true;
}

} // namespace

bool InstallMovementOwnershipProbe(std::string& error) noexcept {
    return Install(error);
}

bool RemoveMovementOwnershipProbe(std::string& error) noexcept {
    error.clear();
    bool success = true;
    for (auto& hook : g_hooks) {
        std::string next;
        if (!hooks::RemoveRel32CallHook(hook, next)) {
            success = false;
            if (!error.empty()) {
                error += "; ";
            }
            error += next;
        }
    }
    if (success) {
        g_image = nullptr;
    }
    return success;
}

MovementOwnershipTelemetry ConsumeMovementOwnershipTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const MovementOwnershipTelemetry result = g_data;
    g_data = {};
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
