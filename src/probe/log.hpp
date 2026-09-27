#pragma once

#include <string>
#include <string_view>

namespace penumbra_vr::probe {

[[nodiscard]] bool OpenLog(std::wstring& path, std::wstring& error,
    std::wstring_view game = L"black-plague") noexcept;
void WriteLog(const char* format, ...) noexcept;
void CloseLog() noexcept;

} // namespace penumbra_vr::probe
