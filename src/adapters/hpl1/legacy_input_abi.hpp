#pragma once
#include <array>
#include <cstdint>
#include <type_traits>
namespace penumbra_vr::adapters::hpl1 {
// Opaque MSVC 2003 string value. The original HPL query must destroy it using
// its own CRT. This type intentionally has no constructor/destructor or access
// to the legacy string's internal allocation policy.
struct LegacyInputString { std::array<std::uint32_t, 7> storage; };
static_assert(sizeof(LegacyInputString) == 28);
static_assert(std::is_trivially_copyable_v<LegacyInputString>);
using LegacyInputQuery = bool(__thiscall*)(void*, LegacyInputString);
}
