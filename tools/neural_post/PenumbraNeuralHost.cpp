#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "NeuralPass.h"
#include "neural_post_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Microsoft::WRL::ComPtr;
namespace bridge = penumbra_vr::neural_post;

struct Handle {
    HANDLE h = nullptr;
    ~Handle() { if (h != nullptr) CloseHandle(h); }
    operator HANDLE() const noexcept { return h; }
};

static std::wstring IniValue(const wchar_t* file, const wchar_t* key) {
    wchar_t value[2048]{};
    GetPrivateProfileStringW(L"Neural", key, L"", value, 2048, file);
    return value;
}

// Row-major OpenVR 3x4 matrices. Orientation is eye->tracking with the
// eye-to-head offset applied; this matches the DirectNR view convention.
// OpenVR GetProjectionRaw supplies tangents; DirectNR expects FOV radians.
static void Multiply(const float* head, const float* eye, float* out) {
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 3; ++col) {
            float v = 0;
            for (int k = 0; k != 3; ++k) v += head[row * 4 + k] * eye[k * 4 + col];
            out[row * 4 + col] = v;
        }
        float p = head[row * 4 + 3];
        for (int k = 0; k != 3; ++k) p += head[row * 4 + k] * eye[k * 4 + 3];
        out[row * 4 + 3] = p;
    }
}

static Transport::Quaternion Quaternion(const float* m) {
    Transport::Quaternion q{};
    const float trace = m[0] + m[5] + m[10];
    if (trace > 0) {
        const float s = 2 * std::sqrt(trace + 1);
        q.w = .25f * s;
        q.x = (m[9] - m[6]) / s;
        q.y = (m[2] - m[8]) / s;
        q.z = (m[4] - m[1]) / s;
    } else if (m[0] > m[5] && m[0] > m[10]) {
        const float s = 2 * std::sqrt(1 + m[0] - m[5] - m[10]);
        q.w = (m[9] - m[6]) / s;
        q.x = .25f * s;
        q.y = (m[1] + m[4]) / s;
        q.z = (m[2] + m[8]) / s;
    } else if (m[5] > m[10]) {
        const float s = 2 * std::sqrt(1 + m[5] - m[0] - m[10]);
        q.w = (m[2] - m[8]) / s;
        q.x = (m[1] + m[4]) / s;
        q.y = .25f * s;
        q.z = (m[6] + m[9]) / s;
    } else {
        const float s = 2 * std::sqrt(1 + m[10] - m[0] - m[5]);
        q.w = (m[4] - m[1]) / s;
        q.x = (m[2] + m[8]) / s;
        q.y = (m[6] + m[9]) / s;
        q.z = .25f * s;
    }
    return q;
}

static Transport::Tracking Tracking(const bridge::FrameHeader& frame) {
    Transport::Tracking result{};
    result.valid = frame.tracking_valid;
    result.id = frame.request_id;
    result.tick = GetTickCount64();
    result.head.orientation = Quaternion(frame.head);
    result.head.position = {frame.head[3], frame.head[7], frame.head[11]};
    for (int eye = 0; eye != 2; ++eye) {
        float matrix[12]{};
        Multiply(frame.head, frame.eye_to_head[eye], matrix);
        auto& target = result.eyes[eye];
        target.pose.orientation = Quaternion(matrix);
        target.pose.position = {matrix[3], matrix[7], matrix[11]};
        const float* f = frame.frustum[eye];
        target.left = std::atan(std::min(f[0], f[1]));
        target.right = std::atan(std::max(f[0], f[1]));
        target.up = std::atan(std::max(f[2], f[3]));
        target.down = std::atan(std::min(f[2], f[3]));
    }
    return result;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        puts("PenumbraNeuralHost.exe <game-pid> <absolute-NeuralPost.ini>");
        return 2;
    }
    const auto pid = static_cast<DWORD>(wcstoul(argv[1], nullptr, 10));
    if (pid == 0) return 2;
    const auto runtime = IniValue(argv[2], L"RuntimeDirectory");
    if (runtime.empty()) return 3;

    wchar_t name[100]{};
    bridge::ObjectName(name, L"memory", pid);
    Handle mapping{OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name)};
    if (!mapping) return 4;
    auto* frame = static_cast<bridge::FrameHeader*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bridge::MappingBytes()));
    if (!frame) return 5;
    bridge::ObjectName(name, L"request", pid);
    Handle request{OpenEventW(SYNCHRONIZE, FALSE, name)};
    bridge::ObjectName(name, L"complete", pid);
    Handle complete{OpenEventW(EVENT_MODIFY_STATE, FALSE, name)};
    bridge::ObjectName(name, L"ready", pid);
    Handle ready{OpenEventW(EVENT_MODIFY_STATE, FALSE, name)};
    bridge::ObjectName(name, L"stop", pid);
    Handle stop{OpenEventW(SYNCHRONIZE, FALSE, name)};
    Handle parent{OpenProcess(SYNCHRONIZE, FALSE, pid)};
    if (!request || !complete || !ready || !parent || !stop ||
        frame->magic != bridge::kMagic || frame->version != bridge::kVersion) {
        UnmapViewOfFile(frame);
        return 6;
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, &level, 1, D3D11_SDK_VERSION,
        &device, nullptr, &context))) {
        frame->result = 9;
        SetEvent(ready);
        UnmapViewOfFile(frame);
        return 7;
    }
    NeuralPass neural;
    auto options = NeuralPass::Options::Read(argv[2]);
    options.enabled = true;
    const auto settings_revision = frame->settings_revision;
    options.scalePercent = std::clamp(static_cast<int>(frame->scale_percent), 25, 100);
    options.intensityPercent = std::clamp(static_cast<int>(frame->intensity_percent), 0, 100);
    options.passes = std::clamp(static_cast<int>(frame->passes), 1, 4);
    options.style = std::clamp(static_cast<int>(frame->style), 0, 2);
    options.foveaPercent = std::clamp(static_cast<int>(frame->fovea_percent), 20, 100);
    options.outerEnabled = frame->outer_enabled != 0;
    options.outerScalePercent = std::clamp(static_cast<int>(frame->outer_scale_percent), 10, 100);
    const int diagnostic = static_cast<int>(GetPrivateProfileIntW(L"Neural", L"DiagnosticMode", 0, argv[2]));
    // Test-only scheduling fault: leave warmup (id 1) and the first real
    // roundtrip (id 2) undelayed so timeout can be verified deterministically.
    const int diagnostic_delay_ms = diagnostic != 0 ? std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Neural", L"DiagnosticDelayMs", 0, argv[2])), 0, 3000) : 0;
    const bool diagnostic_exit = diagnostic != 0 &&
        GetPrivateProfileIntW(L"Neural", L"DiagnosticExitOnRequest3", 0, argv[2]) == 1;
    const unsigned diagnostic_delay_count = GetPrivateProfileIntW(
        L"Neural", L"DiagnosticDelayCount", 0, argv[2]);
    const auto warmup_delay_ms = std::clamp(
        GetPrivateProfileIntW(L"Neural", L"DiagnosticWarmupDelayMs", 0, argv[2]), 0u, 1000u);
    if (!neural.Initialize(device.Get(), options, runtime, diagnostic)) {
        frame->result = 9;
        SetEvent(ready);
        UnmapViewOfFile(frame);
        return 8;
    }
    frame->result = 0;
    SetEvent(ready);
    ComPtr<ID3D11Texture2D> image, readback;
    std::uint32_t old_width = 0, old_height = 0;
    std::uint64_t previous_history_epoch = 0;
    for (;;) {
        // Stop/parent have priority over requests. The game never blocks on
        // model destruction and does not reuse these object names until exit.
        HANDLE waits[]{stop.h, parent.h, request.h};
        const DWORD wait = WaitForMultipleObjects(3, waits, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 2) break;
        if (frame->magic != bridge::kMagic || frame->version != bridge::kVersion ||
            frame->settings_revision != settings_revision) break;
        const auto w = frame->width, h = frame->height;
        const auto id = frame->request_id;
        // Simulate an actual child fault without publishing a completion. Never
        // permit diagnostic INI keys to terminate a real-model worker.
        if (diagnostic_exit && id == 3) ExitProcess(23);
        if (id == 1 && warmup_delay_ms != 0) {
            HANDLE cancellation[]{stop.h, parent.h};
            if (WaitForMultipleObjects(2, cancellation, FALSE, warmup_delay_ms) != WAIT_TIMEOUT)
                break;
        }
        if (frame->history_epoch != previous_history_epoch) {
            neural.Suspend();
            previous_history_epoch = frame->history_epoch;
        }
        if (diagnostic_delay_ms > 0 && id >= 3 &&
            (diagnostic_delay_count == 0 || id - 3 < diagnostic_delay_count))
            Sleep(static_cast<DWORD>(diagnostic_delay_ms));
        bool success = w >= 64 && h >= 64 && w <= bridge::kMaxWidth && h <= bridge::kMaxHeight && frame->tracking_valid != 0;
        if (success && (w != old_width || h != old_height)) {
            image.Reset();
            readback.Reset();
            D3D11_TEXTURE2D_DESC d{};
            d.Width = w;
            d.Height = h;
            d.MipLevels = 1;
            d.ArraySize = 2;
            d.SampleDesc.Count = 1;
            d.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            success = SUCCEEDED(device->CreateTexture2D(&d, nullptr, &image));
            d.BindFlags = 0;
            d.Usage = D3D11_USAGE_STAGING;
            d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (success) success = SUCCEEDED(device->CreateTexture2D(&d, nullptr, &readback));
            old_width = w;
            old_height = h;
        }
        if (success) {
            for (unsigned eye = 0; eye != 2; ++eye) {
                context->UpdateSubresource(image.Get(), eye, nullptr,
                    bridge::InputPixels(frame) + eye * bridge::EyeBytes(w, h), w * 4, 0);
            }
            const auto tracking = Tracking(*frame);
            success = neural.Process(context.Get(), image.Get(), tracking, id, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            if (success) {
                context->CopyResource(readback.Get(), image.Get());
                for (unsigned eye = 0; eye != 2; ++eye) {
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    if (FAILED(context->Map(readback.Get(), eye, D3D11_MAP_READ, 0, &mapped))) {
                        success = false;
                        break;
                    }
                    auto* dst = bridge::OutputPixels(frame) + eye * bridge::EyeBytes(w, h);
                    for (unsigned row = 0; row != h; ++row) {
                        memcpy(dst + row * w * 4, static_cast<const std::uint8_t*>(mapped.pData) + row * mapped.RowPitch, w * 4);
                    }
                    context->Unmap(readback.Get(), eye);
                }
            }
        }
        frame->result = success ? 1u : 2u;
        frame->completed_revision = settings_revision;
        frame->completed_id = id;
        SetEvent(complete);
    }
    UnmapViewOfFile(frame);
    return 0;
}
