#include "log.hpp"
#include "iat_hook.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace {
unsigned flush_calls = 0;
BOOL WINAPI SlowFlush(HANDLE) {
    ++flush_calls;
    Sleep(1); // Model storage latency at the external durability boundary.
    return TRUE;
}
}

int main() {
    penumbra_vr::hooks::IatHook hook;
    std::string error;
    if (!penumbra_vr::hooks::InstallIatHook("KERNEL32.dll", "FlushFileBuffers",
            reinterpret_cast<void*>(&SlowFlush), hook, error)) {
        std::fprintf(stderr, "flush boundary: %s\n", error.c_str());
        return 1;
    }
    std::wstring path, log_error;
    if (!penumbra_vr::probe::OpenLog(path, log_error, L"pacing-test")) return 2;
    flush_calls = 0;
    const auto start = GetTickCount64();
    for (unsigned i = 0; i < 200; ++i)
        penumbra_vr::probe::WriteLog("frame pacing regression sample=%u", i);
    const auto hot_flushes = flush_calls;
    std::printf("200 hot log writes: %llu ms, durability calls=%u\n",
        static_cast<unsigned long long>(GetTickCount64() - start), hot_flushes);
    penumbra_vr::probe::CloseLog();
    const bool close_flushed = flush_calls == hot_flushes + 1;
    if (!penumbra_vr::hooks::RemoveIatHook(hook, error)) return 3;
    // Only this executable's explicitly named fixture log is removed.
    DeleteFileW(path.c_str());
    if (hot_flushes != 0 || !close_flushed) {
        std::fprintf(stderr, "durability flush belongs at close, outside hot logging\n");
        return 4;
    }
    return 0;
}
