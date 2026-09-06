#include "log.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstring>
#include <cstdio>

namespace penumbra_vr::probe {
namespace {

HANDLE g_log = INVALID_HANDLE_VALUE;
SRWLOCK g_log_lock = SRWLOCK_INIT;

} // namespace

bool OpenLog(std::wstring& path, std::wstring& error) noexcept {
    path.clear();
    error.clear();

    wchar_t local_app_data[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        error = L"LOCALAPPDATA is unavailable";
        return false;
    }

    std::wstring root = std::wstring(local_app_data) + L"\\PenumbraVR";
    std::wstring logs = root + L"\\logs";
    if (!CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        error = L"Could not create " + root + L" (Win32 error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    if (!CreateDirectoryW(logs.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        error = L"Could not create " + logs + L" (Win32 error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }

    path = logs + L"\\black-plague-probe-" + std::to_wstring(GetCurrentProcessId()) + L".log";
    g_log = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (g_log == INVALID_HANDLE_VALUE) {
        error = L"Could not open probe log (Win32 error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    return true;
}

void WriteLog(const char* format, ...) noexcept {
    char message[4096]{}; // Full frame telemetry must not lose its trailing diagnostics.
    va_list arguments;
    va_start(arguments, format);
    _vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    char line[4352]{};
    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [pid=%lu] %s\r\n",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        GetCurrentProcessId(),
        message);

    OutputDebugStringA(line);
    AcquireSRWLockExclusive(&g_log_lock);
    if (g_log != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_log, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
        FlushFileBuffers(g_log);
    }
    ReleaseSRWLockExclusive(&g_log_lock);
}

void CloseLog() noexcept {
    AcquireSRWLockExclusive(&g_log_lock);
    if (g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&g_log_lock);
}

} // namespace penumbra_vr::probe
