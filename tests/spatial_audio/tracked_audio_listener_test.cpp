#include "tracked_audio_listener.hpp"
#include "tracked_listener.hpp"

#include <AL/al.h>
#include <AL/alc.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

namespace {
using namespace penumbra_vr;
bool focused = true;
bool ui = false;
bool presenting = true;
bool Eligible() noexcept { return focused && !ui && presenting; }
void Check(bool value, const std::string& message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
template <std::size_t N>
void Expect(int param, const std::array<float, N>& expected, const char* message) {
    std::array<float, N> actual{};
    alGetListenerfv(param, actual.data());
    for (std::size_t i = 0; i < N; ++i) {
        if (std::abs(actual[i] - expected[i]) >= 0.0001F)
            std::cerr << "param=" << param << " component=" << i
                << " expected=" << expected[i] << " actual=" << actual[i] << '\n';
        Check(std::abs(actual[i] - expected[i]) < 0.0001F, message);
    }
    Check(alGetError() == AL_NO_ERROR, "OpenAL listener operation failed");
}
const std::array<float, 3> native_position{10.0F, 2.0F, 30.0F};
const std::array<float, 3> native_velocity{0.5F, 0.0F, 0.25F};
const std::array<float, 6> native_orientation{0, 0, -1, 0, 1, 0};
// Independent geometric oracles, not extracted from the head-pose matrix.
const std::array<float, 6> yaw_orientation{-1, 0, 0, 0, 1, 0};
const std::array<float, 6> recentered_orientation{0, 0, -1, 0, 1, 0};
const std::array<float, 6> roll_orientation{0, 0, -1, -0.5F, 0.8660254F, 0};
using ListenerTarget = void (AL_APIENTRY*)(ALenum, const ALfloat*);
__declspec(noinline) ListenerTarget NativeListenerTarget() {
    // The native import slot is deliberately mutable. Do not let compiler
    // purity analysis fold two observations across hook install/removal.
    ListenerTarget volatile target = &alListenerfv;
    return target;
}
__declspec(noinline) void NativeListener(int param, const float* values) { alListenerfv(param, values); }
void WriteNative() {
    NativeListener(AL_POSITION, native_position.data());
    NativeListener(AL_VELOCITY, native_velocity.data());
    NativeListener(AL_ORIENTATION, native_orientation.data());
}
void ExpectNative() {
    Expect(AL_POSITION, native_position, "native listener position was not restored");
    Expect(AL_ORIENTATION, native_orientation, "native listener orientation was not restored");
    Expect(AL_VELOCITY, native_velocity, "native listener velocity was not restored");
}
runtime::VrMatrix44 Head(float x = 10, float y = 2, float z = 30) {
    auto pose = runtime::IdentityMatrix();
    // +90 degrees physical yaw, with the character orientation unchanged.
    pose.values[0] = 0; pose.values[2] = 1;
    pose.values[8] = -1; pose.values[10] = 0;
    pose.values[3] = x; pose.values[7] = y; pose.values[11] = z;
    return pose;
}
runtime::VrMatrix34 Collapse(const runtime::VrMatrix44& pose) {
    runtime::VrMatrix34 result;
    for (std::size_t i = 0; i < result.values.size(); ++i) result.values[i] = pose.values[i];
    return result;
}
void ExpectHead(const runtime::VrMatrix44& pose,
    const std::array<float, 6>& orientation = yaw_orientation) {
    Expect(AL_POSITION, std::array<float, 3>{pose.values[3], pose.values[7], pose.values[11]},
        "tracked translation did not reach OpenAL");
    Expect(AL_ORIENTATION, orientation,
        "tracked orientation disagrees with independent literal forward/up vectors");
    Expect(AL_VELOCITY, std::array<float, 3>{}, "tracked listener must retain Rework zero velocity");
}
__declspec(noinline) bool MakeCurrent(ALCcontext* context) {
    return alcMakeContextCurrent(context) != 0;
}
}

int main() {
    using namespace penumbra_vr;
    // A real OpenAL Soft context/consumer, using its null backend: no game or
    // audio hardware is launched. The executable imports exercise the real IAT.
    SetEnvironmentVariableA("ALSOFT_DRIVERS", "null");
    auto* device = alcOpenDevice(nullptr);
    Check(device != nullptr, "OpenAL Soft null device unavailable");
    auto* context = alcCreateContext(device, nullptr);
    Check(context && alcMakeContextCurrent(context), "OpenAL context unavailable");
    WriteNative();
    std::string error;
    const auto native_listener_target = NativeListenerTarget();
    Check(reinterpret_cast<void*>(native_listener_target) == reinterpret_cast<void*>(
        GetProcAddress(GetModuleHandleW(L"OpenAL32.dll"), "alListenerfv")),
        "native listener import does not target the loaded OpenAL export");
    Check(adapters::hpl1::InstallTrackedAudioListener(&Eligible, error), error);
    Check(NativeListenerTarget() != native_listener_target,
        "native listener import was not intercepted");
    ExpectNative(); // startup must not invent a tracked pose.
    Check(!adapters::hpl1::InstallTrackedAudioListener(&Eligible, error),
        "duplicate installation must not replace the original import owner");

    // Use the same shared composition/inversion consumed by the renderers,
    // starting with a fixed native character camera and an independent HMD.
    auto character = runtime::IdentityMatrix();
    character.values[3] = 10; character.values[7] = 2; character.values[11] = 30;
    runtime::VrMatrix44 native_view, head_view, head;
    Check(runtime::InvertRigidTransform(Collapse(character), native_view, error), error);
    Check(runtime::ComposeYawRecenteredTrackedHeadView(native_view,
        Collapse(runtime::IdentityMatrix()), Collapse(Head(0, 0, 0)),
        1.0F, head_view, error), error);
    Check(runtime::InvertRigidTransform(Collapse(head_view), head, error), error);
    Check(head.values == Head().values, "fixed character/tracked-head composition drifted");
    audio::TrackedListenerPose built;
    Check(audio::BuildTrackedListenerPose(head, built), "valid rigid head rejected");
    adapters::hpl1::PublishTrackedAudioListener(head);
    ExpectHead(head);
    ALuint source = 0;
    alGenSources(1, &source);
    alSource3f(source, AL_POSITION, 12.0F, 2.0F, 29.0F);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    std::thread native_updates([] { for (unsigned i = 0; i < 100; ++i) WriteNative(); });
    for (unsigned i = 0; i < 100; ++i)
        adapters::hpl1::PublishTrackedAudioListener(head);
    native_updates.join();
    adapters::hpl1::PublishTrackedAudioListener(head);
    ExpectHead(head);
    std::array<float, 3> source_position{};
    ALint relative = AL_TRUE;
    alGetSourcefv(source, AL_POSITION, source_position.data());
    alGetSourcei(source, AL_SOURCE_RELATIVE, &relative);
    Check(source_position == std::array<float, 3>{12, 2, 29} && relative == AL_FALSE,
        "listener tracking changed native world-source coordinates or relative mode");
    alDeleteSources(1, &source);
    Check(MakeCurrent(context), "reasserting current context failed");
    ExpectHead(head);
    // HPL writes its untracked character camera after publication. These real
    // native imports must preserve the tracked head at the actual consumer.
    WriteNative();
    ExpectHead(head);
    alListenerf(AL_GAIN, 0.4F);
    float gain = 0;
    alGetListenerf(AL_GAIN, &gain);
    Check(std::abs(gain - 0.4F) < 0.0001F, "unrelated listener gain changed");

    const auto translated = Head(11.25F, 1.1F, 27.5F);
    adapters::hpl1::PublishTrackedAudioListener(translated);
    ExpectHead(translated);
    Expect(AL_POSITION, std::array<float, 3>{11.25F, 1.1F, 27.5F},
        "translated listener disagrees with literal world position");
    // Recenter/world-origin changes arrive as a new composed pose, with no
    // accumulated listener delta and no dependency on temporary eye cameras.
    auto recentered = runtime::IdentityMatrix();
    recentered.values[3] = -50; recentered.values[7] = 3; recentered.values[11] = 14;
    adapters::hpl1::PublishTrackedAudioListener(recentered);
    ExpectHead(recentered, recentered_orientation);
    WriteNative();
    ExpectHead(recentered, recentered_orientation);
    auto tilted = recentered;
    // Known +30 degree physical roll: forward stays -Z, up tilts toward -X.
    tilted.values[0] = 0.8660254F; tilted.values[1] = -0.5F;
    tilted.values[4] = 0.5F; tilted.values[5] = 0.8660254F;
    adapters::hpl1::PublishTrackedAudioListener(tilted);
    ExpectHead(tilted, roll_orientation); // Up tracks physical roll independently.

    focused = false;
    NativeListener(AL_POSITION, native_position.data());
    ExpectNative(); // all three attributes restored on loss, not a mixed pose.
    focused = true;
    adapters::hpl1::PublishTrackedAudioListener(head);
    ui = true;
    WriteNative();
    ExpectNative();
    ui = false;
    // Returning to focus/gameplay requires a new publication.
    WriteNative();
    ExpectNative();
    adapters::hpl1::PublishTrackedAudioListener(head);
    adapters::hpl1::InvalidateTrackedAudioListener();
    ExpectNative(); // tracking lost / render fallback / transition.
    adapters::hpl1::PublishTrackedAudioListener(head);
    auto invalid = head;
    invalid.values[0] = std::numeric_limits<float>::quiet_NaN();
    adapters::hpl1::PublishTrackedAudioListener(invalid);
    ExpectNative();
    invalid = head;
    invalid.values[0] = 2.0F;
    adapters::hpl1::PublishTrackedAudioListener(invalid);
    ExpectNative();
    adapters::hpl1::PublishTrackedAudioListener(head);
    Sleep(275);
    WriteNative();
    ExpectNative(); // stale tracking cannot survive a stalled render loop.

    adapters::hpl1::PublishTrackedAudioListener(head);
    auto* secondary = alcCreateContext(device, nullptr);
    Check(secondary && MakeCurrent(secondary), "secondary context unavailable");
    WriteNative();
    ExpectNative();
    Check(MakeCurrent(context), "returning to original context failed");
    ExpectNative(); // switching away must restore the previous context first.
    alcDestroyContext(secondary);
    adapters::hpl1::PublishTrackedAudioListener(head);
    presenting = false;
    bool stopped = false;
    std::thread shutdown([&] { stopped = adapters::hpl1::RemoveTrackedAudioListener(error); });
    shutdown.join();
    Check(stopped, error); // backend control thread, with no context stealing.
    Check(NativeListenerTarget() == native_listener_target,
        "shutdown did not restore the original native OpenAL import target");
    ExpectNative();
    Check(adapters::hpl1::RemoveTrackedAudioListener(error), "repeated shutdown failed");
    presenting = true;
    Check(adapters::hpl1::InstallTrackedAudioListener(&Eligible, error), error);
    WriteNative();
    ExpectNative();
    adapters::hpl1::PublishTrackedAudioListener(head);
    // Context destruction must drop cached native/tracked state before a driver
    // can recycle the context address. No current context is created or stolen.
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    context = alcCreateContext(device, nullptr);
    Check(context && alcMakeContextCurrent(context), "replacement context unavailable");
    WriteNative();
    ExpectNative();
    adapters::hpl1::PublishTrackedAudioListener(translated);
    ExpectHead(translated);
    Check(adapters::hpl1::RemoveTrackedAudioListener(error), error);
    Check(NativeListenerTarget() == native_listener_target,
        "restart/shutdown did not restore the original native OpenAL import target");
    ExpectNative();
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    Check(alcCloseDevice(device) != 0, "device shutdown failed");
    std::cout << "Tracked audio listener: real OpenAL/IAT regression passed\n";
}
