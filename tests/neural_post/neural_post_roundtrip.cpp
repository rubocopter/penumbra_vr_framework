#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include "../../src/graphics/neural_post_bridge.hpp"
#include "../../src/graphics/neural_post_protocol.hpp"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace neural = penumbra_vr::neural_post;
namespace runtime = penumbra_vr::runtime;

static void Require(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL %s (Win32=%lu, GL=0x%x)\n", what, GetLastError(), glGetError());
        std::exit(1);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::puts("PvrNeuralRoundtrip <absolute INI> <identity|model|timeout|childfault|watchdog|benchmark|aspect>");
        return 2;
    }
    Require(SetEnvironmentVariableW(L"PVR_NEURAL_POST_INI", argv[1]) != 0, "set INI override");
    const bool srgb_edit = wcscmp(argv[2], L"detail_srgb_edit") == 0;
    const bool detail_edit = wcscmp(argv[2], L"detail_edit") == 0 || srgb_edit;
    const bool detail_srgb = wcscmp(argv[2], L"detail_srgb") == 0 || srgb_edit;
    const bool detail = wcscmp(argv[2], L"detail") == 0 || detail_edit || detail_srgb;
    const bool identity = wcscmp(argv[2], L"identity") == 0 ||
        wcscmp(argv[2], L"zero") == 0 ||
        wcscmp(argv[2], L"aspect") == 0 || (detail && !detail_edit);
    const bool timeout = wcscmp(argv[2], L"timeout") == 0;
    const bool child_fault = wcscmp(argv[2], L"childfault") == 0;
    const bool watchdog = wcscmp(argv[2], L"watchdog") == 0;
    const bool benchmark = wcscmp(argv[2], L"benchmark") == 0;
    const bool aspect = wcscmp(argv[2], L"aspect") == 0;
    const bool live = wcscmp(argv[2], L"live") == 0;
    const bool gl_failure = wcscmp(argv[2], L"glfailure") == 0;
    WNDCLASSW wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"PvrNeuralRoundtripWindow";
    Require(RegisterClassW(&wc) != 0, "register window");
    const HWND window = CreateWindowW(wc.lpszClassName, L"neural-offscreen",
        WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    Require(window != nullptr, "create OpenGL window");
    const HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    const int format = ChoosePixelFormat(dc, &pfd);
    Require(format != 0 && SetPixelFormat(dc, format, &pfd), "set OpenGL pixel format");
    const HGLRC context = wglCreateContext(dc);
    Require(context != nullptr && wglMakeCurrent(dc, context) != 0, "make OpenGL context");

    const int w = aspect ? 448 : 256;
    constexpr int h = 256;
    std::array<GLuint, 2> source{}, output{};
    std::array<std::vector<unsigned char>, 2> original{};
    glGenTextures(2, source.data());
    for (unsigned eye = 0; eye != 2; ++eye) {
        auto& pixels = original[eye];
        pixels.resize(w * h * 4);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const auto i = static_cast<std::size_t>((y * w + x) * 4);
                pixels[i] = detail ? static_cast<unsigned char>((x % 2) ? 255 : 0)
                    : static_cast<unsigned char>((x + eye * 71) % 256);
                pixels[i + 1] = srgb_edit ? 128 : static_cast<unsigned char>((y + eye * 63) % 256);
                pixels[i + 2] = static_cast<unsigned char>((eye ? 180 : 36) + (x % 13));
                pixels[i + 3] = detail ? static_cast<unsigned char>(x % 2 ? 255 : 64) : 255;
            }
        }
        glBindTexture(GL_TEXTURE_2D, source[eye]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, detail_srgb ? 0x8C43 : GL_RGBA8, w, h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    Require(glGetError() == GL_NO_ERROR, "create distinct stereo textures");
    if (srgb_edit) glEnable(0x8DB9 /* FRAMEBUFFER_SRGB */);
    runtime::VrHmdPose pose{};
    pose.pose_valid = true;
    pose.device_connected = true;
    pose.device_to_absolute.values = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    std::array<runtime::VrEyeConfiguration, 2> eyes{};
    for (unsigned eye = 0; eye != 2; ++eye) {
        eyes[eye].left_tangent = -.8F;
        eyes[eye].right_tangent = .8F;
        eyes[eye].top_tangent = -.8F;
        eyes[eye].bottom_tangent = .8F;
        eyes[eye].eye_to_head.values = {1, 0, 0, eye ? .032F : -.032F,
                                       0, 1, 0, 0, 0, 0, 1, 0};
    }
    const auto input = std::array<std::uint32_t, 2>{source[0], source[1]};
    std::array<std::uint32_t, 2> processed{};
    if (wcscmp(argv[2], L"missinghost") == 0 || wcscmp(argv[2], L"missingmodel") == 0) {
        bool unavailable=false;
        for (int attempt=0;attempt!=500;++attempt) {
            Require(!neural::TryProcess(input,pose,eyes,processed) && processed==input,
                "missing Neural dependency must preserve both original eyes without crashing");
            if (std::strcmp(neural::StatusText(),"Unavailable")==0) { unavailable=true;break; }
            Sleep(10);
        }
        Require(unavailable,"missing helper/model must report unavailable");
        for (unsigned i=0;i!=20;++i)
            Require(!neural::TryProcess(input,pose,eyes,processed) && processed==input,
                "unavailable Neural remains a stable original-pair fallback");
        std::puts("PASS missing helper/model: Unavailable, stable stereo fallback, no crash");
        return 0;
    }
    bool received = false;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    bool invalidated_pending = false;
    for (int attempt = 0; attempt != 750; ++attempt) {
        if (live && !invalidated_pending) {
            wchar_t inspect_name[100]{};
            neural::ObjectName(inspect_name, L"memory", GetCurrentProcessId());
            const HANDLE inspect_mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, inspect_name);
            if (inspect_mapping != nullptr) {
                const auto* frame = static_cast<const neural::FrameHeader*>(MapViewOfFile(
                    inspect_mapping, FILE_MAP_READ, 0, 0, sizeof(neural::FrameHeader)));
                const bool pending = frame && frame->request_id != frame->completed_id;
                if (frame) UnmapViewOfFile(frame);
                CloseHandle(inspect_mapping);
                if (pending) {
                    runtime::NeuralPostSettings changed{};
                    changed.enabled = true;
                    changed.internal_height = h;
                    changed.intensity_percent = 50;
                    neural::Configure(changed);
                    invalidated_pending = true;
                }
            }
        }
        if (neural::TryProcess(input, pose, eyes, processed)) {
            received = true;
            break;
        }
        Sleep(20);
    }
    Require(received, "companion startup and synchronized stereo roundtrip");
    if (srgb_edit) {
        Require(glIsEnabled(0x8DB9) == GL_TRUE, "restore game framebuffer sRGB state");
        wchar_t name[100]{}; neural::ObjectName(name, L"memory", GetCurrentProcessId());
        const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
        Require(mapping != nullptr, "inspect sRGB transport");
        auto* frame = static_cast<neural::FrameHeader*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
        Require(frame != nullptr, "map sRGB transport");
        const int green = neural::InputPixels(frame)[1];
        UnmapViewOfFile(frame); CloseHandle(mapping);
        Require(green >= 127 && green <= 129, "sRGB capture delivers encoded bytes without double decoding");
    }
    if (live) Require(invalidated_pending, "settings can invalidate an outstanding warmup request");
    if (aspect) {
        wchar_t mapping_name[100]{};
        neural::ObjectName(mapping_name, L"memory", GetCurrentProcessId());
        const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
        Require(mapping != nullptr, "open neural frame protocol for aspect check");
        const auto* header = static_cast<const neural::FrameHeader*>(
            MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(neural::FrameHeader)));
        Require(header != nullptr, "inspect neural stage dimensions");
        Require(header->width == 1280 && header->height == 731,
            "automatic stage aspect remains 448:256 at 1280 width cap");
        UnmapViewOfFile(header);
        CloseHandle(mapping);
    }
    const auto boot = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    Require(processed[0] != source[0] && processed[1] != source[1] &&
            processed[0] != processed[1], "two independent output texture names");

    if (gl_failure) {
        glEnable(0); // Deterministic caller GL error, before any neural transfer.
        Require(neural::TryProcess(input, pose, eyes, processed),
            "an inherited renderer GL error must not permanently disable neural processing");
        // An actual invalid capture still fails closed. Keep a valid texture
        // object but remove its image, rather than injecting a bridge test flag.
        glBindTexture(GL_TEXTURE_2D, source[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
            "a failed stereo capture preserves the complete original pair");
        Require(std::strcmp(neural::StatusText(), "Bypassed") == 0,
            "GL failure is visible instead of claiming active neural output");
        std::puts("PASS GL failure bypasses output");
        return 0;
    }

    if (live) {
        runtime::NeuralPostSettings settings{};
        settings.enabled = false;
        neural::Configure(settings);
        Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
            "live disable restores original pair immediately");
        settings.enabled = true;
        settings.internal_height = h;
        settings.intensity_percent = 35;
        settings.style = 1;
        settings.passes = 2;
        settings.fovea_percent = 75;
        settings.outer_enabled = true;
        settings.outer_scale_percent = 15;
        neural::Configure(settings);
        Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
            "changed settings cannot reuse the previous worker output");
        received = false;
        for (int attempt = 0; attempt != 500; ++attempt) {
            if (neural::TryProcess(input, pose, eyes, processed)) {
                received = true;
                break;
            }
            Sleep(20);
        }
        Require(received, "live enable and settings recreation without game restart");
        Require(std::strcmp(neural::StatusText(), "Active") == 0, "status reports actual processing");
        wchar_t inspect_name[100]{};
        neural::ObjectName(inspect_name, L"memory", GetCurrentProcessId());
        const HANDLE inspect_mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, inspect_name);
        Require(inspect_mapping != nullptr, "inspect live configuration mapping");
        const auto* frame = static_cast<const neural::FrameHeader*>(MapViewOfFile(
            inspect_mapping, FILE_MAP_READ, 0, 0, sizeof(neural::FrameHeader)));
        Require(frame && frame->settings_revision == frame->completed_revision &&
            frame->intensity_percent == 35 && frame->passes == 2 && frame->style == 1 &&
            frame->fovea_percent == 75 && frame->outer_enabled == 1 && frame->outer_scale_percent == 15,
            "actual helper consumes all newly edited options under the current revision");
        UnmapViewOfFile(frame);
        CloseHandle(inspect_mapping);
        neural::ResetHistory();
        Require(neural::TryProcess(input, pose, eyes, processed), "reset history keeps stereo processing");
        settings.enabled = false;
        neural::Configure(settings);
        for (int attempt = 0; attempt != 100; ++attempt) {
            Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
                "retiring helper never leaks late stereo output");
            Sleep(20);
        }
        wchar_t mapping_name[100]{};
        neural::ObjectName(mapping_name, L"memory", GetCurrentProcessId());
        const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
        if (mapping != nullptr) CloseHandle(mapping);
        Require(mapping == nullptr, "live disable releases worker IPC mapping");
        std::puts("PASS live disable/reconfigure/enable/history reset/worker retirement");
        return 0;
    }

    if (timeout || child_fault || watchdog) {
        const auto before = std::chrono::steady_clock::now();
        const auto elapsed_ms = [&] {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - before).count();
        };
        Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
            "an unfinished request preserves the entire original stereo pair");
        const auto elapsed = elapsed_ms();
        Require(elapsed < 150.0, "deadline stops waiting for delayed companion");
        Require(std::strcmp(neural::StatusText(), "Recovering") == 0,
            "an unfinished frame enters recovery before fault/watchdog handling");
        wchar_t name[100]{}; neural::ObjectName(name, L"memory", GetCurrentProcessId());
        const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
        Require(mapping != nullptr, "inspect pending inference ownership");
        auto* pending = static_cast<neural::FrameHeader*>(
            MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
        Require(pending && pending->request_id == 3 && pending->completed_id == 2 &&
            pending->settings_revision == pending->completed_revision && pending->result == 1,
            "request 3 cannot reuse the successful header from request 2");
        const auto pending_header = *pending;
        const auto* pending_pixels = neural::InputPixels(pending);
        const std::vector<std::uint8_t> captured(pending_pixels,
            pending_pixels + neural::PairBytes(pending->width, pending->height));
        neural::ObjectName(name, L"complete", GetCurrentProcessId());
        const HANDLE complete = OpenEventW(SYNCHRONIZE, FALSE, name);
        Require(complete != nullptr, "observe actual companion completion");

        // Make the current eyes and tracking differ from the worker's pending
        // pair. Recovery must neither overwrite that pair nor display it later.
        for (unsigned eye = 0; eye != 2; ++eye) {
            for (std::size_t i = 0; i != original[eye].size(); ++i)
                if (i % 4 != 3) original[eye][i] ^= 0x5A;
            glBindTexture(GL_TEXTURE_2D, source[eye]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                GL_RGBA, GL_UNSIGNED_BYTE, original[eye].data());
        }
        Require(glGetError() == GL_NO_ERROR, "replace current eyes while inference is pending");
        const auto pending_start = std::chrono::steady_clock::now();
        pose.device_to_absolute.values[3] = 1.0F;
        Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
            "pending inference preserves the current source pair");
        Require(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - pending_start).count() < 25.0,
            "a still-pending frame is polled without another blocking wait");
        Require(std::memcmp(pending, &pending_header, sizeof(pending_header)) == 0 &&
            std::memcmp(neural::InputPixels(pending), captured.data(), captured.size()) == 0,
            "pending shared header, pixels and tracking are not overwritten by a newer frame");
        if (child_fault || watchdog) {
            const double limit_ms = watchdog ? 2400.0 : 1000.0;
            while (std::strcmp(neural::StatusText(), "Bypassed") != 0 && elapsed_ms() < limit_ms) {
                Sleep(10);
                const auto poll_start = std::chrono::steady_clock::now();
                Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
                    "fault/stall polling preserves the current original stereo pair");
                Require(std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - poll_start).count() < 100.0,
                    "fault/stall polling never repeats the blocking frame wait");
            }
            const double bypass_ms = elapsed_ms();
            Require(std::strcmp(neural::StatusText(), "Bypassed") == 0 && bypass_ms < limit_ms,
                "actual child failure or watchdog permanently bypasses before the bounded deadline");
            if (watchdog) Require(bypass_ms >= 1950.0,
                "a live stalled worker is bypassed by the two-second watchdog, not the frame deadline");
            Require(WaitForSingleObject(complete, 0) == WAIT_TIMEOUT &&
                std::memcmp(pending, &pending_header, sizeof(pending_header)) == 0,
                "bypass occurs without consuming stale request-2 success or a request-3 completion");
            if (watchdog) {
                Require(WaitForSingleObject(complete, 1200) == WAIT_OBJECT_0 &&
                    pending->completed_id == 3 && pending->completed_revision == pending_header.settings_revision &&
                    pending->result == 1,
                    "the stalled child actually completes late after watchdog bypass");
            }
            for (unsigned attempt = 0; attempt != 5; ++attempt) {
                Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input &&
                    std::strcmp(neural::StatusText(), "Bypassed") == 0,
                    "a faulted session never resumes or displays stale/late output");
                Sleep(10);
            }
            Require(pending->request_id == 3 && pending->head[3] == 0.0F &&
                std::memcmp(neural::InputPixels(pending), captured.data(), captured.size()) == 0,
                "fault retirement leaves the outstanding request and its input immutable");
            if (child_fault) Require(pending->completed_id == 2 &&
                WaitForSingleObject(complete, 0) == WAIT_TIMEOUT,
                "self-exit publishes no completion and cannot reuse stale success");
            std::printf("PASS %s bypass %.1f ms, original pair retained, stale/late output rejected\n",
                watchdog ? "watchdog" : "childfault", bypass_ms);
        } else {
            Sleep(120); // Allow late output to arrive; it must never be reused.
            Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
                "late companion output never leaks into a later stereo pair");
            Require(std::strcmp(neural::StatusText(), "Bypassed") != 0,
                "an isolated late frame must not permanently disable Neural");
            received = false;
            for (int attempt = 0; attempt != 100; ++attempt) {
                if (neural::TryProcess(input, pose, eyes, processed)) { received = true; break; }
                Sleep(10);
            }
            Require(received && std::strcmp(neural::StatusText(), "Active") == 0,
                "Neural recovers after draining a late pair without a settings toggle");
            Require(pending->request_id > 3 && pending->completed_id == pending->request_id &&
                pending->completed_revision == pending->settings_revision && pending->result == 1 &&
                pending->head[3] == 1.0F && pending->history_epoch > pending_header.history_epoch,
                "recovery processes fresh tracking under matching IDs/revision with reset history");
            for (unsigned eye = 0; eye != 2; ++eye) {
                std::vector<unsigned char> pixels(w * h * 4);
                glBindTexture(GL_TEXTURE_2D, processed[eye]);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                Require(glGetError() == GL_NO_ERROR && pixels == original[eye],
                    "recovered identity output contains the new eyes, not the discarded late pair");
            }
            std::printf("PASS timeout %.1f ms, pending input isolated, late pair discarded, processing recovered\n", elapsed);
        }
        for (unsigned eye = 0; eye != 2; ++eye) {
            std::vector<unsigned char> pixels(w * h * 4);
            glBindTexture(GL_TEXTURE_2D, source[eye]);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            Require(glGetError() == GL_NO_ERROR && pixels == original[eye],
                "timeout/fault handling leaves the current original input untouched");
        }
        UnmapViewOfFile(pending); CloseHandle(mapping); CloseHandle(complete);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return 0;
    }

    // Test GL state restoration during a subsequent successful processing pass.
    constexpr GLenum packRowLength = 0x0D02;
    constexpr GLenum unpackRowLength = 0x0CF2;
    constexpr GLenum packSkipRows = 0x0D03;
    glPixelStorei(packRowLength, 312);
    glPixelStorei(unpackRowLength, 313);
    glPixelStorei(packSkipRows, 2);
    glEnable(GL_SCISSOR_TEST);
    glViewport(17, 23, 99, 101);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    const GLuint currentTexture = source[1];
    glBindTexture(GL_TEXTURE_2D, currentTexture);
    Require(neural::TryProcess(input, pose, eyes, processed), "subsequent stereo frame");
    GLint value = 0;
    glGetIntegerv(packRowLength, &value);
    Require(value == 312, "restore pack row length");
    glGetIntegerv(unpackRowLength, &value);
    Require(value == 313, "restore unpack row length");
    glGetIntegerv(packSkipRows, &value);
    Require(value == 2 && glIsEnabled(GL_SCISSOR_TEST), "restore pixel offsets and scissor");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    Require(value == static_cast<GLint>(currentTexture), "restore bound texture");
    GLint viewport[4]{}; GLboolean mask[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    Require(viewport[0] == 17 && viewport[1] == 23 && viewport[2] == 99 && viewport[3] == 101,
        "residual draw restores the game viewport");
    Require(!mask[0] && mask[1] && !mask[2] && mask[3], "residual draw restores game color mask");
    glPixelStorei(packRowLength, 0);
    glPixelStorei(unpackRowLength, 0);
    glPixelStorei(packSkipRows, 0);

    std::uint64_t delta[2]{};
    for (unsigned eye = 0; eye != 2; ++eye) {
        std::vector<unsigned char> pixels(w * h * 4);
        glBindTexture(GL_TEXTURE_2D, processed[eye]);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        Require(glGetError() == GL_NO_ERROR, "read GL output");
        for (std::size_t i = 0; i != pixels.size(); ++i) {
            const int difference = std::abs(int(pixels[i]) - int(original[eye][i]));
            delta[eye] += static_cast<std::uint64_t>(difference);
        }
        if (identity) Require(pixels == original[eye], "identity roundtrip, eye isolation and orientation");
        else Require(delta[eye] != 0, "neural model changes both eyes");
        if (detail_edit) {
            const auto row = static_cast<std::size_t>(h / 2) * w * 4;
            Require(int(pixels[row + 4]) - int(pixels[row]) > 160,
                "a neural edit must preserve fine source edges instead of replacing them with an upscaled stage");
            Require(pixels[row + 4] >= 190 && pixels[row + 4] <= 192,
                "residual matches DirectNR UNORM resolve: 255 + (64 - 128), without extra gamma conversion");
            for (std::size_t i = 3; i < pixels.size(); i += 4)
                Require(pixels[i] == original[eye][i], "residual reconstruction preserves full-resolution alpha");
        }
        glBindTexture(GL_TEXTURE_2D, source[eye]);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        Require(pixels == original[eye], "input texture remains untouched");
    }
    if (benchmark) {
        double total_ms = 0.0, min_ms = 1e9, max_ms = 0.0;
        std::vector<double> timings;
        int processed_frames = 0;
        for (int i = 0; i < 60; ++i) {
            const auto frame_start = std::chrono::steady_clock::now();
            const bool ok = neural::TryProcess(input, pose, eyes, processed);
            const double ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - frame_start).count();
            if (!ok) {
                Require(processed == input, "benchmark deadline preserves stereo pair");
                std::printf("BENCHMARK fallback after %d frames (last %.2f ms)\n",
                    processed_frames, ms);
                break;
            }
            total_ms += ms;
            timings.push_back(ms);
            min_ms = std::min(min_ms, ms);
            max_ms = std::max(max_ms, ms);
            ++processed_frames;
        }
        if (processed_frames) {
            std::sort(timings.begin(), timings.end());
            std::printf("BENCHMARK processed %d/60, mean %.2f ms, min %.2f ms, max %.2f ms\n",
                processed_frames, total_ms / processed_frames, min_ms, max_ms);
            std::printf("BENCHMARK median %.2f ms, p95 %.2f ms (host roundtrip; no game load)\n",
                timings[timings.size() / 2], timings[(timings.size() - 1) * 95 / 100]);
        }
    }
    pose.pose_valid = false;
    Require(!neural::TryProcess(input, pose, eyes, processed) && processed == input,
        "invalid pose bypass preserves original pair");
    std::printf("PASS OpenGL Win32 -> shared stereo -> DirectNR x64 -> OpenGL, startup %.1f ms, deltas %llu/%llu\n",
        boot, static_cast<unsigned long long>(delta[0]), static_cast<unsigned long long>(delta[1]));
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, dc);
    DestroyWindow(window);
    return 0;
}
