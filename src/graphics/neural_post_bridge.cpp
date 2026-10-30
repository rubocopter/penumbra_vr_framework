#include "neural_post_bridge.hpp"
#include "neural_post_protocol.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include "neural_post_resolve.hpp"

namespace penumbra_vr::neural_post {
namespace {
enum class State { off, starting, warming, active, recovering, unavailable, bypassed, stopping };
std::atomic<State> status{State::off};
SRWLOCK preferences_lock = SRWLOCK_INIT;
runtime::NeuralPostSettings preferences{};
std::uint64_t preferences_revision = 0;
bool preferences_configured = false;
std::atomic<std::uint64_t> history_epoch{0};

void Trace(const wchar_t* message) noexcept {
    wchar_t root[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", root, 32768);
    if (!length || length >= 32768) return;
    const std::wstring base = std::wstring(root) + L"\\PenumbraVR";
    CreateDirectoryW(base.c_str(), nullptr);
    const std::wstring logs = base + L"\\logs";
    CreateDirectoryW(logs.c_str(), nullptr);
    const std::wstring file = logs + L"\\neural-game-" + std::to_wstring(GetCurrentProcessId()) + L".log";
    const HANDLE handle = CreateFileW(file.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return;
    const std::wstring line = std::to_wstring(GetTickCount64()) + L" " + message + L"\r\n";
    char bytes[16384]{};
    const int count = WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
        static_cast<int>(line.size()), bytes, sizeof(bytes), nullptr, nullptr);
    DWORD written = 0;
    if (count > 0) WriteFile(handle, bytes, static_cast<DWORD>(count), &written, nullptr);
    CloseHandle(handle);
}

bool Equal(const runtime::NeuralPostSettings& a, const runtime::NeuralPostSettings& b) noexcept {
    return a.enabled == b.enabled && a.internal_height == b.internal_height &&
        a.scale_percent == b.scale_percent && a.intensity_percent == b.intensity_percent &&
        a.passes == b.passes && a.style == b.style && a.fovea_percent == b.fovea_percent &&
        a.outer_enabled == b.outer_enabled && a.outer_scale_percent == b.outer_scale_percent;
}
constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kDrawFramebuffer = 0x8CA9;
constexpr GLenum kReadFramebufferBinding = 0x8CAA;
constexpr GLenum kDrawFramebufferBinding = 0x8CA6;
constexpr GLenum kColorAttachment0 = 0x8CE0;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kPixelPackBufferBinding = 0x88ED;
constexpr GLenum kPixelUnpackBufferBinding = 0x88EF;
constexpr GLenum kPackRowLength = 0x0D02;
constexpr GLenum kPackSkipRows = 0x0D03;
constexpr GLenum kPackSkipPixels = 0x0D04;
constexpr GLenum kUnpackRowLength = 0x0CF2;
constexpr GLenum kUnpackSkipRows = 0x0CF3;
constexpr GLenum kUnpackSkipPixels = 0x0CF4;
constexpr GLenum kRgba8 = 0x8058;

using GenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using BindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using FramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using CheckFramebuffer = GLenum(APIENTRY*)(GLenum);
using BlitFramebuffer = void(APIENTRY*)(GLint, GLint, GLint, GLint,
    GLint, GLint, GLint, GLint, GLbitfield, GLenum);

struct GlApi {
    GenFramebuffers gen = nullptr;
    DeleteFramebuffers remove = nullptr;
    BindFramebuffer bind = nullptr;
    FramebufferTexture2D attach = nullptr;
    CheckFramebuffer status = nullptr;
    BlitFramebuffer blit = nullptr;
    [[nodiscard]] bool Load() noexcept {
        const auto lookup = [](const char* name) -> PROC {
            const PROC p = wglGetProcAddress(name);
            if (p != nullptr && p != reinterpret_cast<PROC>(1) &&
                p != reinterpret_cast<PROC>(2) && p != reinterpret_cast<PROC>(3) &&
                p != reinterpret_cast<PROC>(-1)) return p;
            return GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name);
        };
        gen = reinterpret_cast<GenFramebuffers>(lookup("glGenFramebuffers"));
        remove = reinterpret_cast<DeleteFramebuffers>(lookup("glDeleteFramebuffers"));
        bind = reinterpret_cast<BindFramebuffer>(lookup("glBindFramebuffer"));
        attach = reinterpret_cast<FramebufferTexture2D>(lookup("glFramebufferTexture2D"));
        status = reinterpret_cast<CheckFramebuffer>(lookup("glCheckFramebufferStatus"));
        blit = reinterpret_cast<BlitFramebuffer>(lookup("glBlitFramebuffer"));
        return gen && remove && bind && attach && status && blit;
    }
};

struct Handle {
    HANDLE value = nullptr;
    void Close() noexcept { if (value) CloseHandle(value); value = nullptr; }
};

struct GlState {
    GLint read = 0, draw = 0, texture = 0, pack = 4, unpack = 4;
    GLint read_buffer = 0, draw_buffer = 0;
    GLint pack_row_length = 0, pack_skip_rows = 0, pack_skip_pixels = 0;
    GLint unpack_row_length = 0, unpack_skip_rows = 0, unpack_skip_pixels = 0;
    GLboolean scissor = GL_FALSE, framebuffer_srgb = GL_FALSE;
    bool valid = false;
    [[nodiscard]] bool Save() noexcept {
        GLint pack_buffer = 0, unpack_buffer = 0;
        glGetIntegerv(kPixelPackBufferBinding, &pack_buffer);
        glGetIntegerv(kPixelUnpackBufferBinding, &unpack_buffer);
        if (pack_buffer || unpack_buffer) return false;
        glGetIntegerv(kReadFramebufferBinding, &read);
        glGetIntegerv(kDrawFramebufferBinding, &draw);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
        glGetIntegerv(kPackRowLength, &pack_row_length);
        glGetIntegerv(kPackSkipRows, &pack_skip_rows);
        glGetIntegerv(kPackSkipPixels, &pack_skip_pixels);
        glGetIntegerv(kUnpackRowLength, &unpack_row_length);
        glGetIntegerv(kUnpackSkipRows, &unpack_skip_rows);
        glGetIntegerv(kUnpackSkipPixels, &unpack_skip_pixels);
        glGetIntegerv(GL_READ_BUFFER, &read_buffer);
        glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        framebuffer_srgb = glIsEnabled(0x8DB9 /* FRAMEBUFFER_SRGB */);
        if (scissor) glDisable(GL_SCISSOR_TEST);
        // IPC carries encoded sRGB bytes, even when the caller's render target
        // has sRGB storage. Do not inherit a decode into our plain RGBA8 proxy.
        glDisable(0x8DB9 /* FRAMEBUFFER_SRGB */);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(kPackRowLength, 0);
        glPixelStorei(kPackSkipRows, 0);
        glPixelStorei(kPackSkipPixels, 0);
        glPixelStorei(kUnpackRowLength, 0);
        glPixelStorei(kUnpackSkipRows, 0);
        glPixelStorei(kUnpackSkipPixels, 0);
        valid = true;
        return true;
    }
    void Restore(const GlApi& gl) const noexcept {
        if (!valid) return;
        gl.bind(kReadFramebuffer, static_cast<GLuint>(read));
        gl.bind(kDrawFramebuffer, static_cast<GLuint>(draw));
        glReadBuffer(static_cast<GLenum>(read_buffer));
        glDrawBuffer(static_cast<GLenum>(draw_buffer));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glPixelStorei(GL_PACK_ALIGNMENT, pack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
        glPixelStorei(kPackRowLength, pack_row_length);
        glPixelStorei(kPackSkipRows, pack_skip_rows);
        glPixelStorei(kPackSkipPixels, pack_skip_pixels);
        glPixelStorei(kUnpackRowLength, unpack_row_length);
        glPixelStorei(kUnpackSkipRows, unpack_skip_rows);
        glPixelStorei(kUnpackSkipPixels, unpack_skip_pixels);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        if (framebuffer_srgb) glEnable(0x8DB9); else glDisable(0x8DB9);
    }
};

struct Session {
    bool attempted = false, enabled = false, failed = false;
    bool ready = false;
    bool warmup_pending = false, warmed = false;
    bool late_pending = false;
    ULONGLONG pending_started = 0;
    GlApi gl{};
    ResidualResolve resolve{};
    HGLRC context = nullptr;
    Handle mapping, request, complete, ready_event, child, stop;
    std::uint64_t applied_revision = 0;
    bool retiring = false;
    FrameHeader* header = nullptr;
    std::uint32_t stage_width = 0, stage_height = 0;
    std::uint32_t output_width = 0, output_height = 0;
    std::uint32_t width_setting = 0, height_setting = 896;
    DWORD timeout_ms = 20;
    GLuint read_fbo = 0, draw_fbo = 0;
    GLuint stage = 0;
    std::array<GLuint, 2> result{};
    std::uint64_t sequence = 0;
    std::wstring ini;

    void Release() noexcept {
        if (wglGetCurrentContext() == context && context != nullptr) {
            resolve.Release();
            if (stage) glDeleteTextures(1, &stage);
            if (result[0]) glDeleteTextures(2, result.data());
            if (read_fbo) gl.remove(1, &read_fbo);
            if (draw_fbo) gl.remove(1, &draw_fbo);
        }
        if (header) UnmapViewOfFile(header);
        header = nullptr;
        mapping.Close(); request.Close(); complete.Close(); ready_event.Close(); child.Close(); stop.Close();
        stage = 0; result = {}; read_fbo = 0; draw_fbo = 0; context = nullptr;
        // A destroyed/replaced context already owns disposal of its objects.
        // Never carry their numeric names into a new context on live re-enable.
        resolve.program = 0; resolve.stage[0] = resolve.stage[1] = 0;
        resolve.width = 0; resolve.height = 0;
        stage_width = 0; stage_height = 0; output_width = 0; output_height = 0;
        ready = false; warmup_pending = false; warmed = false; sequence = 0;
        late_pending = false; pending_started = 0;
        retiring = false;
    }
    ~Session() {
        if (stop.value) SetEvent(stop.value);
        // Worker also observes parent exit. Closing this process's view never
        // invalidates the worker's independently mapped view.
        Release();
    }
    bool Retire() noexcept {
        retiring = true;
        if (stop.value) SetEvent(stop.value);
        if (child.value && WaitForSingleObject(child.value, 0) == WAIT_TIMEOUT)
            return false;
        Release();
        return true;
    }
    void Fail(const char* cause) noexcept {
        if (!failed) {
            std::string log = std::string("PenumbraVR Neural Post bypass: ") + cause + "\n";
            OutputDebugStringA(log.c_str());
            Trace(std::wstring(log.begin(), log.end()).c_str());
        }
        failed = true;
        status.store(State::bypassed, std::memory_order_release);
        if (stop.value) SetEvent(stop.value);
    }
    [[nodiscard]] bool Init(const runtime::NeuralPostSettings& settings, bool configured) noexcept {
        attempted = true;
        wchar_t path[32768]{};
        const DWORD env = GetEnvironmentVariableW(L"PVR_NEURAL_POST_INI", path, 32768);
        if (env == 0 || env >= 32768) {
            const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
            if (!length || length >= 32768) return false;
            std::wstring directory(path, length);
            const auto separator = directory.find_last_of(L"\\/");
            if (separator == std::wstring::npos) return false;
            ini = directory.substr(0, separator + 1) + L"NeuralPost.ini";
        } else {
            ini = path;
        }
        Trace((L"configuration INI: " + ini).c_str());
        enabled = configured ? settings.enabled :
            GetPrivateProfileIntW(L"Neural", L"Enabled", 0, ini.c_str()) == 1;
        if (!enabled) return false;
        status.store(State::starting, std::memory_order_release);
        width_setting = static_cast<std::uint32_t>(std::clamp(
            GetPrivateProfileIntW(L"Neural", L"InternalWidth", 0, ini.c_str()), 0u, kMaxWidth));
        height_setting = static_cast<std::uint32_t>(std::clamp(
            GetPrivateProfileIntW(L"Neural", L"InternalHeight", 896, ini.c_str()), 64u, kMaxHeight));
        timeout_ms = static_cast<DWORD>(std::clamp(
            GetPrivateProfileIntW(L"Neural", L"TimeoutMs", 20, ini.c_str()), 1u, 250u));
        if (configured) {
            width_setting = 0;
            height_setting = static_cast<std::uint32_t>(settings.internal_height);
        }
        wchar_t exe[32768]{};
        GetPrivateProfileStringW(L"Neural", L"HostPath", L"", exe, 32768, ini.c_str());
        Trace((L"host: " + std::wstring(exe)).c_str());
        SetLastError(ERROR_SUCCESS);
        const DWORD host_attributes = GetFileAttributesW(exe);
        const DWORD host_error = GetLastError();
        if (!*exe || host_attributes == INVALID_FILE_ATTRIBUTES) {
            Trace((L"host attribute check failed, Win32=" + std::to_wstring(host_error)).c_str());
            Fail("HostPath is missing or unavailable");
            status.store(State::unavailable, std::memory_order_release);
            return false;
        }
        const auto bytes = MappingBytes();
        wchar_t name[100]{};
        const DWORD pid = GetCurrentProcessId();
        ObjectName(name, L"memory", pid);
        const auto bytes64 = static_cast<std::uint64_t>(bytes);
        mapping.value = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            static_cast<DWORD>(bytes64 >> 32), static_cast<DWORD>(bytes64), name);
        if (!mapping.value) { Fail("CreateFileMapping failed"); return false; }
        header = static_cast<FrameHeader*>(MapViewOfFile(mapping.value,
            FILE_MAP_ALL_ACCESS, 0, 0, bytes));
        if (!header) { Fail("MapViewOfFile failed"); return false; }
        *header = {};
        header->settings_revision = applied_revision;
        const auto number = [&](const wchar_t* key, int fallback, int selected) {
            return static_cast<std::uint32_t>(configured ? selected :
                static_cast<int>(GetPrivateProfileIntW(L"Neural", key, fallback, ini.c_str())));
        };
        header->scale_percent = number(L"ScalePercent", 60, settings.scale_percent);
        header->intensity_percent = number(L"IntensityPercent", 90, settings.intensity_percent);
        header->passes = number(L"Passes", 1, settings.passes);
        header->style = number(L"Style", 2, settings.style);
        header->fovea_percent = number(L"FoveaPercent", 100, settings.fovea_percent);
        header->outer_enabled = number(L"OuterEnabled", 0, settings.outer_enabled ? 1 : 0);
        header->outer_scale_percent = number(L"OuterScalePercent", 25, settings.outer_scale_percent);
        ObjectName(name, L"request", pid);
        request.value = CreateEventW(nullptr, FALSE, FALSE, name);
        ObjectName(name, L"complete", pid);
        complete.value = CreateEventW(nullptr, FALSE, FALSE, name);
        ObjectName(name, L"ready", pid);
        ready_event.value = CreateEventW(nullptr, TRUE, FALSE, name);
        ObjectName(name, L"stop", pid);
        stop.value = CreateEventW(nullptr, TRUE, FALSE, name);
        if (!request.value || !complete.value || !ready_event.value || !stop.value) {
            Fail("CreateEvent failed"); return false;
        }
        const std::wstring cmd = L"\"" + std::wstring(exe) + L"\" " +
            std::to_wstring(pid) + L" \"" + ini + L"\"";
        std::wstring mutable_cmd = cmd;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe, mutable_cmd.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
            Fail("CreateProcess failed"); return false;
        }
        CloseHandle(process.hThread);
        child.value = process.hProcess;
        Trace((L"companion PID: " + std::to_wstring(process.dwProcessId)).c_str());
        return true;
    }
    [[nodiscard]] bool LoadGl() noexcept {
        if (context) return wglGetCurrentContext() == context;
        if (!wglGetCurrentContext() || !gl.Load()) return false;
        context = wglGetCurrentContext();
        gl.gen(1, &read_fbo);
        gl.gen(1, &draw_fbo);
        return read_fbo && draw_fbo;
    }
    [[nodiscard]] bool AllocateTexture(GLuint& object, std::uint32_t w,
        std::uint32_t h) noexcept {
        if (!object) glGenTextures(1, &object);
        if (!object) return false;
        glBindTexture(GL_TEXTURE_2D, object);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(kRgba8),
            static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        return glGetError() == GL_NO_ERROR;
    }
    [[nodiscard]] bool Capture(const std::array<std::uint32_t, 2>& source,
        std::uint32_t& w, std::uint32_t& h) noexcept {
        GLint original_binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &original_binding);
        for (const auto texture : source) {
            if (!glIsTexture(texture)) return false;
            glBindTexture(GL_TEXTURE_2D, texture);
            GLint x = 0, y = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &x);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &y);
            if (x <= 0 || y <= 0 || (w && (w != static_cast<std::uint32_t>(x) || h != static_cast<std::uint32_t>(y)))) {
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(original_binding));
                return false;
            }
            w = static_cast<std::uint32_t>(x);
            h = static_cast<std::uint32_t>(y);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(original_binding));
        std::uint32_t sw = width_setting;
        std::uint32_t sh = height_setting;
        if (!sw) {
            // Keep the source eye aspect ratio when the automatic width hits
            // the protocol cap. Clamping width alone stretches neural detail.
            const auto desired_width =
                (static_cast<std::uint64_t>(w) * sh + h / 2) / h;
            if (desired_width > kMaxWidth) {
                sw = kMaxWidth;
                sh = static_cast<std::uint32_t>(std::clamp(
                    (static_cast<std::uint64_t>(h) * sw + w / 2) / w,
                    static_cast<std::uint64_t>(64), static_cast<std::uint64_t>(kMaxHeight)));
            } else {
                sw = static_cast<std::uint32_t>(std::clamp(
                    desired_width, static_cast<std::uint64_t>(64),
                    static_cast<std::uint64_t>(kMaxWidth)));
            }
        }
        if (stage_width != sw || stage_height != sh) {
            if (!AllocateTexture(stage, sw, sh)) return false;
            stage_width = sw;
            stage_height = sh;
        }
        for (unsigned eye = 0; eye != 2; ++eye) {
            gl.bind(kReadFramebuffer, read_fbo);
            gl.attach(kReadFramebuffer, kColorAttachment0, GL_TEXTURE_2D, source[eye], 0);
            gl.bind(kDrawFramebuffer, draw_fbo);
            gl.attach(kDrawFramebuffer, kColorAttachment0, GL_TEXTURE_2D, stage, 0);
            if (gl.status(kReadFramebuffer) != kFramebufferComplete ||
                gl.status(kDrawFramebuffer) != kFramebufferComplete) return false;
            glReadBuffer(kColorAttachment0);
            glDrawBuffer(kColorAttachment0);
            // Flip in the blit: glReadPixels uses bottom-first rows, while
            // DirectNR expects conventional top-first image memory.
            gl.blit(0, 0, static_cast<GLint>(w), static_cast<GLint>(h),
                0, static_cast<GLint>(sh), static_cast<GLint>(sw), 0,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);
            gl.bind(kReadFramebuffer, draw_fbo);
            glReadPixels(0, 0, static_cast<GLsizei>(sw), static_cast<GLsizei>(sh),
                GL_RGBA, GL_UNSIGNED_BYTE,
                InputPixels(header) + eye * EyeBytes(sw, sh));
            if (glGetError() != GL_NO_ERROR) return false;
        }
        return true;
    }
    [[nodiscard]] bool Upload(const std::array<std::uint32_t, 2>& source,
        std::uint32_t w, std::uint32_t h) noexcept {
        if (output_width != w || output_height != h) {
            for (auto& texture : result) {
                if (!AllocateTexture(texture, w, h)) return false;
            }
            output_width = w;
            output_height = h;
        }
        for (unsigned eye = 0; eye != 2; ++eye) {
            gl.bind(kDrawFramebuffer, draw_fbo);
            gl.attach(kDrawFramebuffer, kColorAttachment0, GL_TEXTURE_2D, result[eye], 0);
            if (gl.status(kDrawFramebuffer) != kFramebufferComplete) return false;
            glDrawBuffer(kColorAttachment0);
            const auto offset = eye * EyeBytes(stage_width, stage_height);
            if (!resolve.Draw(source[eye], InputPixels(header) + offset,
                    OutputPixels(header) + offset, stage_width, stage_height, w, h)) return false;
        }
        return true;
    }
};
// The companion's mapping and event names are process-scoped. Only the
// compositor thread that first submits a valid GL stereo pair may own them.
// Keep that ownership for the process lifetime: a second thread must never
// reset a live mapping or start another companion on the same event names.
std::atomic<DWORD> owner_thread{0};
thread_local Session session;
} // namespace

void Configure(const runtime::NeuralPostSettings& source) noexcept {
    auto settings = source;
    runtime::NormalizeNeuralPostSettings(settings);
    AcquireSRWLockExclusive(&preferences_lock);
    if (!preferences_configured || !Equal(preferences, settings)) {
        preferences = settings;
        ++preferences_revision;
        preferences_configured = true;
        status.store(settings.enabled ? State::starting : State::off, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&preferences_lock);
}

const char* StatusText() noexcept {
    switch (status.load(std::memory_order_acquire)) {
    case State::off: return "Off";
    case State::starting: return "Starting";
    case State::warming: return "Warming up";
    case State::active: return "Active";
    case State::recovering: return "Recovering";
    case State::unavailable: return "Unavailable";
    case State::bypassed: return "Bypassed";
    case State::stopping: return "Stopping";
    }
    return "Unavailable";
}

void ResetHistory() noexcept { history_epoch.fetch_add(1, std::memory_order_acq_rel); }

bool TryProcess(const std::array<std::uint32_t, 2>& source,
    const runtime::VrHmdPose& pose,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    std::array<std::uint32_t, 2>& output) noexcept {
    output = source;
    if (!wglGetCurrentContext())
        return false;
    const DWORD thread_id = GetCurrentThreadId();
    DWORD expected = 0;
    if (!owner_thread.compare_exchange_strong(expected, thread_id,
            std::memory_order_acq_rel, std::memory_order_acquire) &&
        expected != thread_id)
        return false;
    auto& s = session;
    runtime::NeuralPostSettings selected{};
    std::uint64_t revision = 0;
    bool configured = false;
    AcquireSRWLockShared(&preferences_lock);
    selected = preferences; revision = preferences_revision; configured = preferences_configured;
    ReleaseSRWLockShared(&preferences_lock);
    if (revision != s.applied_revision) {
        if (!s.Retire()) {
            status.store(selected.enabled ? State::stopping : State::off, std::memory_order_release);
            return false;
        }
        s.applied_revision = revision;
        s.attempted = false; s.failed = false; s.enabled = false;
    }
    if (configured && !selected.enabled) {
        static_cast<void>(s.Retire());
        status.store(State::off, std::memory_order_release);
        return false;
    }
    if (s.failed) {
        static_cast<void>(s.Retire());
        return false;
    }
    if (!pose.pose_valid || !pose.device_connected) {
        ResetHistory();
        return false;
    }
    if (!s.attempted && !s.Init(selected, configured)) return false;
    if (!s.enabled) return false;
    if (s.late_pending) {
        const auto completion = WaitForSingleObject(s.complete.value, 0);
        if (completion == WAIT_TIMEOUT) {
            if (WaitForSingleObject(s.child.value, 0) == WAIT_OBJECT_0)
                s.Fail("companion exited while draining a late frame");
            else if (GetTickCount64() - s.pending_started > 2000)
                s.Fail("companion stalled for two seconds after a frame deadline");
            return false;
        }
        if (completion != WAIT_OBJECT_0 ||
            s.header->completed_id != s.header->request_id ||
            s.header->completed_revision != s.applied_revision || s.header->result != 1) {
            s.Fail("companion rejected a late stereo pair");
            return false;
        }
        // Never display this output with the current eyes or overwrite its
        // shared input before completion. Discard it, then start fresh on the
        // next compositor frame. Discontinuous history cannot span the gap.
        s.late_pending = false;
        ResetHistory();
        Trace(L"late stereo output discarded; Neural can resume on the next frame");
        return false;
    }
    if (!s.ready) {
        if (WaitForSingleObject(s.ready_event.value, 0) != WAIT_OBJECT_0) {
            if (WaitForSingleObject(s.child.value, 0) == WAIT_OBJECT_0) s.Fail("companion exited during startup");
            return false;
        }
        if (s.header->result != 0) {
            s.Fail("companion could not initialize DirectNR; original stereo retained");
            status.store(State::unavailable, std::memory_order_release);
            return false;
        }
        s.ready = true;
        status.store(State::warming, std::memory_order_release);
    }
    // Feature creation on the first DirectNR frame can take hundreds of
    // milliseconds. Let it finish asynchronously while the compositor keeps
    // receiving the original stereo pair. Never overwrite shared input until
    // this exact request has completed; discard its output when it does.
    if (s.warmup_pending) {
        const DWORD completion_status = WaitForSingleObject(s.complete.value, 0);
        if (completion_status == WAIT_TIMEOUT) {
            if (WaitForSingleObject(s.child.value, 0) == WAIT_OBJECT_0)
                s.Fail("companion exited during neural warmup");
            return false;
        }
        if (completion_status != WAIT_OBJECT_0 ||
            s.header->completed_id != s.header->request_id ||
            s.header->completed_revision != s.applied_revision ||
            s.header->result != 1) {
            s.Fail("companion failed neural warmup");
            return false;
        }
        s.warmup_pending = false;
        s.warmed = true;
    }
    if (!s.LoadGl()) { s.Fail("OpenGL context or FBO blit unavailable"); return false; }
    GlState gl_state;
    // HPL leaves errors pending during normal rendering. They describe earlier
    // calls, not these transfers. Start a clean error boundary so only errors
    // from our capture/upload can disable Neural; do not poison the session.
    bool inherited_error = false;
    for (unsigned index = 0; index != 32; ++index) {
        if (glGetError() == GL_NO_ERROR) break;
        inherited_error = true;
    }
    static thread_local bool reported_inherited_error = false;
    if (inherited_error && !reported_inherited_error) {
        Trace(L"cleared inherited renderer GL errors before Neural transfers");
        reported_inherited_error = true;
    }
    if (!gl_state.Save()) { s.Fail("unsupported OpenGL pixel buffer binding"); return false; }
    std::uint32_t w = 0, h = 0;
    const bool captured = s.Capture(source, w, h);
    gl_state.Restore(s.gl);
    if (!captured) { s.Fail("stereo capture failed"); return false; }

    FrameHeader& frame = *s.header;
    frame.width = s.stage_width;
    frame.height = s.stage_height;
    frame.request_id = ++s.sequence;
    frame.history_epoch = history_epoch.load(std::memory_order_acquire);
    frame.tracking_valid = 1;
    std::memcpy(frame.head, pose.device_to_absolute.values.data(), sizeof(frame.head));
    for (unsigned eye = 0; eye != 2; ++eye) {
        std::memcpy(frame.eye_to_head[eye], eyes[eye].eye_to_head.values.data(), sizeof(frame.eye_to_head[eye]));
        const auto& e = eyes[eye];
        frame.frustum[eye][0] = e.left_tangent;
        frame.frustum[eye][1] = e.right_tangent;
        frame.frustum[eye][2] = e.top_tangent;
        frame.frustum[eye][3] = e.bottom_tangent;
    }
    s.pending_started = GetTickCount64();
    if (!SetEvent(s.request.value)) {
        s.Fail("companion request event failed");
        return false;
    }
    if (!s.warmed) {
        s.warmup_pending = true;
        return false;
    }
    const auto completion = WaitForSingleObject(s.complete.value, s.timeout_ms);
    if (completion == WAIT_TIMEOUT) {
        s.late_pending = true;
        status.store(State::recovering, std::memory_order_release);
        Trace((L"frame " + std::to_wstring(frame.request_id) + L" missed " +
            std::to_wstring(s.timeout_ms) + L" ms deadline; original stereo retained until late output is drained").c_str());
        return false;
    }
    if (completion != WAIT_OBJECT_0) {
        s.Fail("companion completion wait failed");
        return false;
    }
    if (frame.completed_id != frame.request_id || frame.completed_revision != s.applied_revision || frame.result != 1) {
        s.Fail("companion rejected stereo pair");
        return false;
    }
    if (!gl_state.Save()) { s.Fail("OpenGL upload state unavailable"); return false; }
    const bool uploaded = s.Upload(source, w, h);
    gl_state.Restore(s.gl);
    if (!uploaded) { s.Fail("stereo output upload failed"); return false; }
    AcquireSRWLockShared(&preferences_lock);
    const bool still_current = preferences_revision == revision;
    ReleaseSRWLockShared(&preferences_lock);
    if (!still_current) return false;
    output = {s.result[0], s.result[1]};
    if (status.exchange(State::active, std::memory_order_acq_rel) != State::active)
        Trace(L"Neural active: completed current stereo pair");
    return true;
}
} // namespace penumbra_vr::neural_post
