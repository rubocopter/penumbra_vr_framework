#pragma once

#include <string>
#include <string_view>

namespace penumbra_vr {

enum class GameId {
    unknown,
    overture,
    black_plague,
    requiem,
};

enum class BuildVariant {
    observed,
    large_address_aware,
};

struct KnownBuild {
    GameId game;
    std::string_view id;
    std::string_view sha256;
    BuildVariant variant;
    std::string_view canonical_sha256;
    bool black_plague_probe_allowed;
};

[[nodiscard]] bool ComputeFileSha256(
    const std::wstring& path,
    std::string& sha256,
    std::wstring& error);

[[nodiscard]] const KnownBuild* FindKnownBuild(std::string_view sha256) noexcept;
[[nodiscard]] std::wstring_view GameDisplayName(GameId game) noexcept;

} // namespace penumbra_vr
