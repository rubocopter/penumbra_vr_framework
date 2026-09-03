#pragma once

#include <string>

namespace penumbra_vr::probe {

[[nodiscard]] bool OpenLog(std::wstring& path, std::wstring& error) noexcept;
void WriteLog(const char* format, ...) noexcept;
void CloseLog() noexcept;

} // namespace penumbra_vr::probe
