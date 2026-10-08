#include "render_world.hpp"

#include "camera_matrix_override.hpp"
#include "camera_capture.hpp"
#include "frame_presentation_gate.hpp"
#include "gameplay_contract.hpp"
#include "gameplay_bridge.hpp"
#include "hand_contact_probe.hpp"
#include "iat_hook.hpp"
#include "refraction_copy_gate.hpp"
#include "tool_socket_profile.hpp"
#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"
#include "opengl_eye_targets.hpp"
#include "opengl_eye_scissor.hpp"
#include "rel32_call_hook.hpp"
#include "stereo_render_policy.hpp"
#include "tool_visibility_tracking.hpp"
#include "vr_math.hpp"
#include "vr_panel_policy.hpp"
#include "vr_grab_pose.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_mechanism_policy.hpp"
#include "vr_rework_hand_profile.hpp"
#include "vr_settings.hpp"
#include "log.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {
namespace {

// All addresses/signatures below belong only to the initialized, allowlisted
// Requiem Steam image. The same HPL contracts have different Black Plague RVAs.
constexpr std::uintptr_t kRenderWorldCallRva = 0x000EDE90;
constexpr std::uintptr_t kRenderWorldRva = 0x0012D110;
constexpr std::array<std::uint8_t, 5> kRenderWorldCall{
    0xE8, 0x7B, 0xF2, 0x03, 0x00};
constexpr std::uintptr_t kUpdateRenderListCallRva = 0x000EDE04;
constexpr std::uintptr_t kUpdateRenderListRva = 0x0012AED0;
constexpr std::array<std::uint8_t, 5> kUpdateRenderListCall{
    0xE8, 0xC7, 0xD0, 0x03, 0x00};
constexpr std::uintptr_t kDrawAllCallRva = 0x000EDEC2;
constexpr std::uintptr_t kDrawAllRva = 0x000F5140;
constexpr std::uintptr_t kCopyContextToTextureSlotRva = 0x0028969C;
constexpr std::uintptr_t kCopyContextToTextureRva = 0x00160640;
constexpr std::array<std::uint8_t, 8> kCopyContextToTextureEntry{
    0x53, 0x55, 0x8B, 0x6C, 0x24, 0x0C, 0x85, 0xED};
constexpr std::array<std::uint8_t, 6> kRefractionClippedCopyCall{
    0xFF, 0x92, 0x74, 0x01, 0x00, 0x00};
constexpr std::uintptr_t kRefractionClippedCopyCallRva = 0x0012C279;
constexpr std::uintptr_t kRefractionFullCopyCallRva = 0x0012C2C8;
constexpr std::array<std::uint8_t, 5> kDrawAllCall{
    0xE8, 0x79, 0x72, 0x00, 0x00};
constexpr std::array<std::uint8_t, 8> kDrawAllEntry{
    0x81, 0xEC, 0x6C, 0x01, 0x00, 0x00, 0x53, 0x55};
constexpr std::uintptr_t kHandsUpdateSlotRva = 0x0027DCF4;
constexpr std::uintptr_t kHandsUpdateRva = 0x000A4280;
constexpr std::uintptr_t kToolMatrixCallRva = 0x000A47B3;
constexpr std::uintptr_t kEntitySetMatrixRva = 0x000CA890;
constexpr std::uintptr_t kLegacyStringEqualIatRva = 0x00273144;
constexpr std::uintptr_t kRaySlotRva = 0x00292DB8;
constexpr std::uintptr_t kCastRayRva = 0x0018A5A0;
constexpr std::uintptr_t kNormalUpdateRva = 0x000ADB40;
constexpr std::uintptr_t kNormalVtableRva = 0x0027E2A8;
constexpr std::uintptr_t kNormalRayCallRva = 0x000ADCCF;
constexpr std::uintptr_t kGetPickedBodyRva = 0x0009AB30;
constexpr std::uintptr_t kNormalInteractRva = 0x000ADFA0;
constexpr std::array<std::uint8_t, 10> kGetPickedBodyEntry{
    0x8B, 0x81, 0x80, 0x02, 0x00, 0x00, 0x8B, 0x40, 0x04, 0xC3};
constexpr std::array<std::uint8_t, 12> kNormalInteractEntry{
    0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x10, 0xE8, 0x85,
    0xCB, 0xFE, 0xFF, 0x85};
constexpr std::array<std::uint8_t, 3> kNormalRayCall{0xFF, 0x57, 0x68};
constexpr std::array<std::uint8_t, 8> kCastRayEntry{
    0x8A, 0x44, 0x24, 0x18, 0x8A, 0x54, 0x24, 0x14};
constexpr std::array<std::uint8_t, 8> kNormalUpdateEntry{
    0x83, 0xEC, 0x34, 0x53, 0x55, 0x56, 0x8B, 0xF1};
constexpr std::uintptr_t kGrabUpdateSlotRva = 0x0027E244;
constexpr std::uintptr_t kGrabEnterSlotRva = 0x0027E29C;
constexpr std::uintptr_t kGrabLeaveSlotRva = 0x0027E2A0;
constexpr std::uintptr_t kGrabUpdateRva = 0x000ABF60;
constexpr std::uintptr_t kGrabEnterRva = 0x000ACE40;
constexpr std::uintptr_t kGrabLeaveRva = 0x000AA920;
constexpr std::uintptr_t kGrabExitRva = 0x000AA3A0;
constexpr std::uintptr_t kMoveExitRva = 0x000AA490;
constexpr std::uintptr_t kPhysicsBodyVtableRva = 0x00293DD8;
constexpr std::uintptr_t kGetJointCountRva = 0x000CD6D0;
constexpr std::array<std::uint8_t, 8> kGetJointCountEntry{
    0x8B, 0x91, 0x54, 0x03, 0x00, 0x00, 0x85, 0xD2};
constexpr std::array<std::uint8_t, 8> kGrabUpdateEntry{
    0x81, 0xEC, 0xA0, 0x00, 0x00, 0x00, 0x53, 0x55};
constexpr std::array<std::uint8_t, 8> kGrabEnterEntry{
    0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF};
constexpr std::array<std::uint8_t, 8> kGrabLeaveEntry{
    0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1, 0x8B, 0x46};
constexpr std::array<std::uint8_t, 5> kGrabExitEntry{
    0x8B, 0x41, 0x28, 0x8B, 0x49};
constexpr std::array<std::uint8_t, 5> kMoveExitEntry{
    0x8B, 0x41, 0x60, 0x8B, 0x49};
constexpr std::array<std::uintptr_t,3> kMoveSlots{
    0x0027E0F4, 0x0027E14C, 0x0027E150};
constexpr std::array<std::uintptr_t,3> kMoveTargets{
    0x000AAAE0, 0x000AB0D0, 0x000AB320};
constexpr std::uintptr_t kGetBodyJointRva = 0x000CDA10;
constexpr std::uintptr_t kHingeVtableRva = 0x00294098;
constexpr std::uintptr_t kSliderVtableRva = 0x00294188;
constexpr std::uintptr_t kHingeGetTypeRva = 0x00156EE0;
constexpr std::uintptr_t kSliderGetTypeRva = 0x00107840;
constexpr std::array<std::uint8_t, 15> kGetBodyJointEntry{
    0x8B, 0x81, 0x54, 0x03, 0, 0, 0x8B, 0x4C,
    0x24, 0x04, 0x8D, 0x04, 0x88, 0x8B, 0x00};
constexpr std::array<std::array<std::uint8_t,8>,3> kMoveEntries{{
    {0x83,0xEC,0x24,0x56,0x8B,0xF1,0x8B,0x46},
    {0x83,0xEC,0x4C,0x55,0x56,0x8B,0xF1,0x8B},
    {0x56,0x8B,0xF1,0x8B,0x4E,0x54,0x8B,0x81}}};
constexpr std::uintptr_t kPushVtableRva = 0x0027E178;
constexpr std::array<std::uintptr_t,3> kPushSlots{
    0x0027E17C, 0x0027E1D4, 0x0027E1D8};
constexpr std::array<std::uintptr_t,3> kPushTargets{
    0x000AB480, 0x000AB7F0, 0x000ABB70};
constexpr std::array<std::array<std::uint8_t,8>,3> kPushEntries{{
    {0x83,0xEC,0x68,0x53,0x55,0x56,0x57,0x8B},
    {0x81,0xEC,0x94,0x00,0x00,0x00,0x53,0x56},
    {0x56,0x8B,0xF1,0x8B,0x46,0x50,0x8A,0x4E}}};
constexpr std::uintptr_t kPushExitRva = 0x000AB670;
constexpr std::uintptr_t kBodyAddForceRva = 0x0019D100;
constexpr std::array<std::uint8_t, 8> kHandsUpdateEntry{
    0x81, 0xEC, 0xA0, 0x02, 0x00, 0x00, 0x53, 0x55};
constexpr std::array<std::uint8_t, 5> kToolMatrixCall{
    0xE8, 0xD8, 0x60, 0x02, 0x00};
constexpr std::array<std::uint8_t, 8> kEntitySetMatrixEntry{
    0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0xC1};
constexpr float kVisibilityAngularGuardRadians = 0.087266463F;
constexpr ULONGLONG kMaximumVisibilityPoseAgeMs = 250;
constexpr float kMenuDistance = 1.75F;
constexpr float kGameplayOverlayDistance = 2.5F;
constexpr float kMenuWidth = 2.4F;
constexpr float kMenuCenterY = 0.0F;
constexpr auto kCameraLayout = kGameplayCameraLayout;
// Manifest-confirmed Requiem native lazy matrix getters (thiscall, const ref).
constexpr std::uintptr_t kGetViewMatrixRva = 0x00113880;
constexpr std::uintptr_t kGetProjectionMatrixRva = 0x001139C0;
constexpr std::array<std::uint8_t, 16> kGetViewMatrixEntry{
    0x81, 0xEC, 0x8C, 0, 0, 0, 0x53, 0x55,
    0x8B, 0xE9, 0x8A, 0x85, 0xD1, 0x08, 0, 0};
constexpr std::array<std::uint8_t, 8> kGetProjectionMatrixEntry{
    0x8B, 0xC1, 0x8A, 0x88, 0xD2, 0x08, 0, 0};
constexpr float kHplNearClip = 0.05F;

using RenderWorld = void(__thiscall*)(void*, void*, void*, float);
using UpdateRenderList = void(__thiscall*)(void*, void*, void*, float);
using DrawAll = void(__thiscall*)(void*);
using HandsUpdate = void(__thiscall*)(void*, float);
using EntitySetMatrix = void(__thiscall*)(void*, const runtime::VrMatrix44*);
using CopyContextToTexture = void(__thiscall*)(
    void*, void*, const void*, const void*, const void*);
using Ray = void(__thiscall*)(void*, void*,
    const std::array<float,3>*, const std::array<float,3>*,
    bool, bool, bool, bool);
using Transition = void(__thiscall*)(void*, void*);
hooks::Rel32CallHook g_hook;
hooks::Rel32CallHook g_visibility_hook;
hooks::Rel32CallHook g_draw_all_hook;
hooks::Rel32CallHook g_tool_matrix_hook;
hooks::IatHook g_hands_update_hook;
hooks::IatHook g_refraction_copy_hook;
hooks::IatHook g_ray_hook;
std::array<hooks::IatHook, 3> g_grab_hooks{};
std::array<hooks::IatHook, 3> g_move_state_hooks{};
std::array<hooks::IatHook, 3> g_push_state_hooks{};
std::atomic<RenderWorld> g_original{nullptr};
std::atomic<UpdateRenderList> g_original_visibility{nullptr};
std::atomic<DrawAll> g_original_draw_all{nullptr};
std::atomic<CopyContextToTexture> g_original_refraction_copy{nullptr};
std::atomic<runtime::OpenVrSession*> g_session{nullptr};
std::atomic<bool> g_presenting{false};
std::atomic<bool> g_destroy_requested{false};
std::atomic<bool> g_targets_destroyed{true};
std::atomic<std::uint32_t> g_active_calls{0};
std::atomic<bool> g_error_logged{false};
std::atomic<bool> g_first_submit_logged{false};
std::atomic<bool> g_first_visibility_logged{false};
std::atomic<bool> g_first_visibility_reuse_logged{false};
std::atomic<bool> g_first_menu_submit_logged{false};
std::atomic<bool> g_recenter_requested{false};
std::atomic<float> g_tracking_world_yaw{0.0F};
std::atomic<bool> g_world_ui_panel_pending{false};
std::atomic<std::uint64_t> g_presentation_generation{0};
std::atomic<std::uint64_t> g_world_timing_frames{0};
std::atomic<std::uint64_t> g_world_render_ticks{0};
std::array<std::atomic<std::uint64_t>, 2> g_eye_ticks{};
std::atomic<std::uint64_t> g_overlay_ticks{0};
std::atomic<std::uint64_t> g_submit_ticks{0};
std::atomic<std::uint64_t> g_selection_refreshes{0};
std::atomic<std::uint64_t> g_redirected_rays{0};
std::atomic<std::uint64_t> g_ray_hits{0};
std::atomic<std::uint64_t> g_ray_winners{0};
std::atomic<std::uint64_t> g_grab_enters{0};
std::atomic<std::uint64_t> g_native_grab_enters{0};
std::atomic<std::uint64_t> g_native_move_enters{0};
std::atomic<std::uint64_t> g_grabs_acquired{0};
std::atomic<std::uint64_t> g_grabs_released{0};
std::atomic<std::uint64_t> g_moves_acquired{0};
std::atomic<std::uint64_t> g_moves_released{0};
std::atomic<std::uint64_t> g_mechanism_acquired{0};
std::atomic<std::uint64_t> g_native_push_enters{0};
std::atomic<std::uint64_t> g_pushes_acquired{0};
std::atomic<std::uint64_t> g_push_force_ticks{0};
std::atomic<std::uint64_t> g_refraction_native_calls{0};
std::atomic<std::uint64_t> g_refraction_copy_attempts{0};
std::atomic<std::uint64_t> g_refraction_resize_attempts{0};
std::atomic<std::uint64_t> g_tools_attached{0};
std::atomic<std::uint64_t> g_tools_native{0};
std::atomic<std::uint64_t> g_tools_render_aligned{0};
std::atomic<std::uint64_t> g_tools_visibility_aligned{0};
std::atomic<std::uint64_t> g_tools_visibility_resolved_palm{0};
std::atomic<std::uint64_t> g_tools_visibility_raw_palm{0};
std::atomic<std::uint64_t> g_tools_visibility_palm_gap_over_2cm{0};
FramePresentationGate g_frame_presentation;
graphics::OpenGlEyeTargets g_targets;
graphics::OpenGlEyeTargets g_overlay_targets;
std::uint8_t* g_image = nullptr;
thread_local void* g_updating_hands = nullptr;
enum class ToolKind : std::uint8_t { none, flashlight, glowstick, flare };
struct ToolAttachment final {
    void* hands = nullptr;
    void* model = nullptr;
    void* entity = nullptr;
    std::size_t slot = 0;
    ToolKind kind = ToolKind::none;
    runtime::VrHand hand = runtime::VrHand::right;
    std::uint64_t updated_at = 0;
};
thread_local std::array<ToolAttachment, 2> g_tool_attachments{};
thread_local bool g_selection_refresh_active = false;
thread_local bool g_vr_selection_ready = false;
thread_local void* g_vr_selection_player = nullptr;
struct VrSelectionContact final {
    void* body = nullptr;
    std::array<float,3> world_point{};
    std::uint64_t sampled_at = 0;
};
thread_local VrSelectionContact g_vr_selection_contact{};
thread_local std::uint32_t g_refresh_entity_hits = 0;
thread_local float g_refresh_winner_distance = -1.0F;
thread_local float g_refresh_winner_mass = -1.0F;
std::atomic<std::uint64_t> g_world_yaw_epoch{0};
struct GrabHold final {
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    runtime::VrHand hand = runtime::VrHand::right;
    runtime::VrGrabPose pose;
    runtime::VrReleaseVelocity release_velocity;
    std::array<float,3> previous_palm{};
    std::array<float,3> previous_tracking{};
    std::uint64_t yaw_epoch = 0;
    float max_linear = 0.0F;
    float max_angular = 0.0F;
    bool collide_character = true;
};
thread_local void* g_pending_grab_state = nullptr;
thread_local runtime::VrHand g_pending_grab_hand = runtime::VrHand::right;
thread_local GrabHold g_grab_hold;
enum class MoveMode { free_body, slider, hinge };
struct MoveHold final {
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    runtime::VrHand hand = runtime::VrHand::right;
    MoveMode mode = MoveMode::free_body;
    std::array<float,3> local_body_contact{};
    std::array<float,3> local_hand_contact{};
    std::array<float,3> previous_palm{};
    std::array<float,3> previous_tracking{};
    runtime::VrMatrix44 previous_palm_pose{};
    std::uint64_t yaw_epoch = 0;
    std::array<float,3> joint_pin{};
    std::array<float,3> joint_pivot{};
    float hinge_lightness = 1.0F;
    float max_linear = 0.0F;
    float max_angular = 0.0F;
};
thread_local void* g_pending_move_state = nullptr;
thread_local runtime::VrHand g_pending_move_hand = runtime::VrHand::right;
thread_local MoveHold g_move_hold;
struct PushHold final {
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    runtime::VrHand hand = runtime::VrHand::right;
    std::array<float,3> body_relative_contact{};
    std::uint64_t yaw_epoch = 0;
};
thread_local void* g_pending_push_state = nullptr;
thread_local runtime::VrHand g_pending_push_hand = runtime::VrHand::right;
thread_local PushHold g_push_hold;
std::array<runtime::VrEyeConfiguration, 2> g_eyes{};
std::array<runtime::VrMatrix44, 2> g_projections{};
runtime::VrMatrix34 g_tracking_anchor{};
bool g_tracking_anchor_valid = false; // Render thread only.
runtime::VrSettings g_height_settings;
SRWLOCK g_height_settings_lock = SRWLOCK_INIT;
runtime::VrPlayModePolicy g_height_play_mode; // Render thread only.
SRWLOCK g_head_world_pose_lock = SRWLOCK_INIT;
runtime::VrMatrix44 g_head_world_pose{};
std::uint64_t g_head_world_pose_sampled_at_ms = 0;
bool g_head_world_pose_valid = false;
SRWLOCK g_tracking_world_lock = SRWLOCK_INIT;
runtime::VrMatrix44 g_world_from_tracking{};
std::uint64_t g_world_from_tracking_time = 0;
std::uint64_t g_world_from_tracking_yaw_epoch = 0;
runtime::VrTrackingSampleIdentity g_world_from_tracking_identity{};
float g_head_tracking_height = 0.0F;
float g_menu_tracking_height = 0.0F;
std::uint64_t g_menu_tracking_height_at = 0;
runtime::VrMatrix34 g_menu_anchor{};
bool g_menu_anchor_valid = false; // Render thread only.
runtime::VrMatrix44 g_world_panel_pose{}; // Render thread only.
NativeUiSurface g_world_panel_surface = NativeUiSurface::none;
bool g_world_panel_valid = false;
SRWLOCK g_menu_pointer_lock = SRWLOCK_INIT;
runtime::VrMatrix34 g_menu_pointer_anchor{};
float g_menu_pointer_aspect = 0.0F;
bool g_menu_pointer_world_panel = false;
runtime::VrMatrix44 g_menu_pointer_world_from_tracking{};
runtime::VrMatrix44 g_menu_pointer_world_pose{};
float g_menu_pointer_distance = kMenuDistance;
float g_menu_pointer_width = kMenuWidth;
float g_menu_pointer_center_y = kMenuCenterY;
struct VisibilityPresentation {
    void* renderer = nullptr;
    void* world = nullptr;
    void* camera = nullptr;
    runtime::VrHmdPose pose{};
    ULONGLONG sampled_at_ms = 0;
    std::uint64_t generation = 0;
    bool valid = false;
};
thread_local VisibilityPresentation g_pending_visibility;
thread_local bool g_inside_stereo_eye = false;
using RefractionEyeCopyContext = graphics::RefractionEyeCopyContext;
thread_local const RefractionEyeCopyContext* g_refraction_eye_copy = nullptr;
thread_local runtime::VrHmdPose g_pending_world_ui_pose;
thread_local bool g_pending_world_ui_pose_valid = false;
thread_local bool g_overlay_pending = false;
thread_local runtime::VrMatrix44 g_overlay_head_view{};
thread_local bool g_overlay_world_panel = false;
thread_local NativeUiSurface g_overlay_surface = NativeUiSurface::none;
thread_local runtime::VrMatrix44 g_overlay_panel_pose{};

class ActiveCall final {
public:
    ActiveCall() noexcept { g_active_calls.fetch_add(1, std::memory_order_acq_rel); }
    ~ActiveCall() { g_active_calls.fetch_sub(1, std::memory_order_acq_rel); }
    ActiveCall(const ActiveCall&) = delete;
    ActiveCall& operator=(const ActiveCall&) = delete;
};

class ScopedRefractionEyeCopy final {
public:
    ScopedRefractionEyeCopy(const graphics::OpenGlEyeTargets& targets,
        graphics::Eye eye, const graphics::OpenGlEyeBinding& binding) noexcept
        : previous_(g_refraction_eye_copy),
          context_{wglGetCurrentContext(),
              static_cast<GLint>(targets.target(eye).framebuffer),
              static_cast<GLint>(targets.width()),
              static_cast<GLint>(targets.height()),
              binding.previous_viewport[2],
              binding.previous_viewport[3]} {
        g_refraction_eye_copy = &context_;
    }
    ~ScopedRefractionEyeCopy() { g_refraction_eye_copy = previous_; }
    ScopedRefractionEyeCopy(const ScopedRefractionEyeCopy&) = delete;
    ScopedRefractionEyeCopy& operator=(const ScopedRefractionEyeCopy&) = delete;
private:
    const RefractionEyeCopyContext* previous_ = nullptr;
    RefractionEyeCopyContext context_{};
};

void __fastcall HookedCopyContextToTexture(void* graphics, void*,
    void* texture, const void* position, const void* size,
    const void* texture_offset) noexcept {
    ActiveCall active_call;
    const auto return_address = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto original = g_original_refraction_copy.load(
        std::memory_order_acquire);
    if (original == nullptr) return;
    original(graphics, texture, position, size, texture_offset);

    const auto* eye = g_refraction_eye_copy;
    if (texture == nullptr || eye == nullptr ||
        eye->context != wglGetCurrentContext() ||
        g_image == nullptr || !g_inside_stereo_eye) return;
    const auto image_base = reinterpret_cast<std::uintptr_t>(g_image);
    if (!IsRefractionCopyReturn(image_base, return_address)) return;
    g_refraction_native_calls.fetch_add(1, std::memory_order_relaxed);

    const auto result = graphics::CaptureBoundRefractionEye(*eye);
    if (result == graphics::RefractionEyeCopyResult::skipped) return;
    if (result == graphics::RefractionEyeCopyResult::resized)
        g_refraction_resize_attempts.fetch_add(1, std::memory_order_relaxed);
    g_refraction_copy_attempts.fetch_add(1, std::memory_order_relaxed);
}

void LogFailureOnce(const char* phase, const std::string& error) noexcept {
    if (!g_error_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem %s failed: %s", phase, error.c_str());
    }
}

[[nodiscard]] bool LooksLikeMappedGameplayCamera(
    const adapters::hpl1::CameraMatrixSnapshot& snapshot) noexcept {
    constexpr float kTolerance = 1.0e-3F;
    const auto& projection = snapshot.projection.values;
    const auto& view = snapshot.view.values;
    return snapshot.flags[0] == 1 &&
        snapshot.flags[1] <= 1 && snapshot.flags[2] <= 1 &&
        std::fabs(projection[10] + 1.0F) <= kTolerance &&
        projection[11] < 0.0F &&
        std::fabs(projection[14] + 1.0F) <= kTolerance &&
        std::fabs(projection[15]) <= kTolerance &&
        std::fabs(view[12]) <= kTolerance &&
        std::fabs(view[13]) <= kTolerance &&
        std::fabs(view[14]) <= kTolerance &&
        std::fabs(view[15] - 1.0F) <= kTolerance;
}

[[nodiscard]] bool CaptureNativeGameplayCamera(void* camera,
    adapters::hpl1::CameraMatrixSnapshot& snapshot,
    std::string& error) noexcept {
    return CaptureGameplayCamera(camera, [](void* native_camera) {
        using Getter = const runtime::VrMatrix44&(__thiscall*)(void*);
        static_cast<void>(reinterpret_cast<Getter>(g_image + kGetViewMatrixRva)(native_camera));
        static_cast<void>(reinterpret_cast<Getter>(g_image + kGetProjectionMatrixRva)(native_camera));
    }, snapshot, error);
}

[[nodiscard]] float TrackingWorldYaw(
    const runtime::VrMatrix44& native_view,
    const runtime::VrMatrix34& anchor) noexcept {
    const float game_forward_x = -native_view.values[8];
    const float game_forward_z = -native_view.values[10];
    const float anchor_forward_x = -anchor.values[2];
    const float anchor_forward_z = -anchor.values[10];
    return std::atan2(
        anchor_forward_z * game_forward_x -
            anchor_forward_x * game_forward_z,
        anchor_forward_x * game_forward_x +
            anchor_forward_z * game_forward_z);
}

[[nodiscard]] bool ComposeTrackedHeadView(
    const runtime::VrMatrix44& native_view,
    const runtime::VrHmdPose& pose,
    runtime::VrMatrix44& head_view,
    std::string& error) noexcept {
    if (g_recenter_requested.exchange(false, std::memory_order_acq_rel)) {
        g_tracking_anchor_valid = false;
        g_menu_anchor_valid = false;
        g_tracking_world_yaw.store(0.0F, std::memory_order_release);
        g_world_yaw_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    if (!g_tracking_anchor_valid) {
        g_tracking_anchor = pose.device_to_absolute;
        g_tracking_anchor_valid = true;
    }
    runtime::VrMatrix34 effective_anchor{};
    if (!runtime::RotateTrackingPoseYaw(
            g_tracking_anchor,
            -g_tracking_world_yaw.load(std::memory_order_acquire),
            effective_anchor, error)) return false;
    if (!runtime::ComposeYawRecenteredTrackedHeadView(
        native_view, effective_anchor, pose.device_to_absolute,
        0.0F, head_view, error)) return false;

    PublishGameplayHeadTracking(pose,
        TrackingWorldYaw(native_view, effective_anchor));

    const auto body = ReadGameplayTrackingSample();
    if (body.character_body == nullptr || !body.body_height_valid ||
        body.sampled_at_ms == 0) return true;

    runtime::VrMatrix34 native_view_rigid{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            native_view_rigid.values[row * 4U + column] =
                native_view.values[row * 4U + column];
        }
    }
    runtime::VrMatrix44 native_head_pose{};
    if (!runtime::InvertRigidTransform(
            native_view_rigid, native_head_pose, error)) return false;
    const float native_y = native_head_pose.values[7];
    const float tracking_y = pose.device_to_absolute.values[7];
    // The intro's scripted camera/crouch must never become a permanent VR
    // height offset. As in Rework/BP, feet + tracked HMD + user profile own Y.
    float tracked_y = 0.0F;
    AcquireSRWLockShared(&g_height_settings_lock);
    const auto height_settings = g_height_settings;
    ReleaseSRWLockShared(&g_height_settings_lock);
    if (!ComposeRequiemConfiguredHeadHeight(
            body.body_center_y, body.active_size_y, tracking_y,
            height_settings, g_height_play_mode, body.native_crouched,
            body.physical_crouch, tracked_y, error)) return false;
    const auto now = GetTickCount64();
    thread_local std::uint64_t logged_height_generation = 0;
    thread_local std::uint64_t logged_height_at = 0;
    if (logged_height_generation != body.body_generation ||
        now - logged_height_at >= 5000) {
        logged_height_generation = body.body_generation;
        logged_height_at = now;
        probe::WriteLog(
            "Requiem tracked height generation=%llu feet_y=%.3f native_camera_y=%.3f tracking_y=%.3f world_y=%.3f height_offset=%.3f seated_offset=%.3f native_crouched=%u physical_crouch=%u",
            static_cast<unsigned long long>(body.body_generation),
            body.body_center_y - body.active_size_y * 0.5F, native_y,
            tracking_y, tracked_y, height_settings.height_offset,
            g_height_play_mode.status().seated_offset,
            body.native_crouched ? 1U : 0U, body.physical_crouch ? 1U : 0U);
    }
    std::array<float, 3> translation{0.0F, tracked_y - native_y, 0.0F};
    if (body.room_scale_valid && body.sampled_at_ms != 0 &&
        now >= body.sampled_at_ms && now - body.sampled_at_ms <= 250 &&
        (body.tracking_identity.pose_epoch == 0 ||
            pose.identity.pose_epoch == 0 ||
            runtime::SameTrackingEpoch(
                body.tracking_identity, pose.identity))) {
        const float dx = pose.device_to_absolute.values[3] -
            body.observed_tracking_pose.values[3];
        const float dz = pose.device_to_absolute.values[11] -
            body.observed_tracking_pose.values[11];
        const float tracking_distance = std::hypot(dx, dz);
        const float body_head_distance = std::hypot(
            native_head_pose.values[3] - body.body_position[0],
            native_head_pose.values[11] - body.body_position[2]);
        if (std::isfinite(tracking_distance) &&
            tracking_distance <=
                runtime::vr_locomotion_policy::kMaximumHeadBodySeparation &&
            std::isfinite(body_head_distance) && body_head_distance <=
                runtime::vr_locomotion_policy::kMaximumHeadBodySeparation) {
            const float yaw = TrackingWorldYaw(native_view, effective_anchor);
            const float cosine = std::cos(yaw);
            const float sine = std::sin(yaw);
            const auto prediction = runtime::FilterPhysicalRenderPrediction(
                {cosine * dx + sine * dz, 0.0F,
                 -sine * dx + cosine * dz},
                body.physical_reconciliation);
            translation[0] = body.head_anchor[0] + prediction[0] -
                native_head_pose.values[3];
            translation[2] = body.head_anchor[2] + prediction[2] -
                native_head_pose.values[11];
        }
    }
    runtime::VrMatrix44 translated_view{};
    if (!runtime::ApplyWorldTranslationToView(head_view,
            translation,
            translated_view, error)) return false;
    head_view = translated_view;
    return true;
}

[[nodiscard]] runtime::VrMatrix34 CollapseRigidView(
    const runtime::VrMatrix44& matrix) noexcept {
    runtime::VrMatrix34 collapsed{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            collapsed.values[row * 4U + column] =
                matrix.values[row * 4U + column];
        }
    }
    return collapsed;
}

void ResetTrackedHeadWorldPose() noexcept {
    AcquireSRWLockExclusive(&g_head_world_pose_lock);
    g_head_world_pose = {};
    g_head_world_pose_sampled_at_ms = 0;
    g_head_world_pose_valid = false;
    g_head_tracking_height = 0.0F;
    ReleaseSRWLockExclusive(&g_head_world_pose_lock);
}

void PublishTrackedHeadWorldPose(
    const runtime::VrMatrix44& head_view,
    float tracking_height,
    std::uint64_t sampled_at_ms) noexcept {
    runtime::VrMatrix44 head_world_pose{};
    std::string error;
    const bool valid = runtime::InvertRigidTransform(
        CollapseRigidView(head_view), head_world_pose, error);

    AcquireSRWLockExclusive(&g_head_world_pose_lock);
    g_head_world_pose = valid ? head_world_pose : runtime::VrMatrix44{};
    g_head_world_pose_sampled_at_ms = valid ? sampled_at_ms : 0;
    g_head_world_pose_valid = valid;
    g_head_tracking_height = valid && std::isfinite(tracking_height)
        ? tracking_height : 0.0F;
    if (std::isfinite(tracking_height)) {
        g_menu_tracking_height = tracking_height;
        g_menu_tracking_height_at = sampled_at_ms;
    }
    ReleaseSRWLockExclusive(&g_head_world_pose_lock);
}

[[nodiscard]] std::array<float, 2> AlignToolsForRender(
    const std::array<runtime::VrMatrix44, 2>& palms,
    const std::array<bool, 2>& palm_valid,
    const runtime::VrHmdPose& head,
    const runtime::VrControllerFrame& frame,
    std::uint64_t now) noexcept;

void __fastcall HookedUpdateRenderList(void* renderer, void*,
    void* world, void* camera, float frame_time) noexcept {
    ActiveCall active_call;
    const auto original = g_original_visibility.load(std::memory_order_acquire);
    if (original == nullptr) return;
    if (!g_presenting.load(std::memory_order_acquire)) {
        original(renderer, world, camera, frame_time);
        return;
    }

    const bool inside_eye = g_inside_stereo_eye;
    if (!inside_eye) g_pending_visibility.valid = false;
    adapters::hpl1::CameraMatrixSnapshot camera_snapshot;
    std::string error;
    if (!CaptureNativeGameplayCamera(camera, camera_snapshot, error)) {
        LogFailureOnce("visibility camera capture", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        original(renderer, world, camera, frame_time);
        return;
    }

    runtime::VrHmdPose pose;
    runtime::VrMatrix44 head_view = camera_snapshot.view;
    ULONGLONG sampled_at_ms = 0;
    if (!inside_eye) {
        auto* session = g_session.load(std::memory_order_acquire);
        if (session == nullptr || !session->WaitForHmdPose(pose, error) ||
            !pose.device_connected || !pose.pose_valid ||
            !ComposeTrackedHeadView(
                camera_snapshot.view, pose, head_view, error)) {
            LogFailureOnce("visibility HMD pose", error.empty()
                ? "The HMD pose is not tracked" : error);
            original(renderer, world, camera, frame_time);
            return;
        }
        sampled_at_ms = GetTickCount64();
    }

    runtime::VrCullFrustum frustum;
    if (!runtime::BuildConservativeStereoCullFrustum(
            g_eyes, kVisibilityAngularGuardRadians, frustum, error)) {
        LogFailureOnce("visibility frustum", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    adapters::hpl1::CameraVisibilityOverride override(kCameraLayout);
    if (!override.Apply(camera, head_view, g_projections[0],
            frustum.vertical_fov_radians, frustum.aspect, error)) {
        LogFailureOnce("visibility camera override", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    if (!inside_eye) {
        const auto frame = ReadNativeControllerFrame();
        if (frame.focused && !NativeUiActive()) {
            runtime::VrMatrix44 world_from_tracking{};
            std::string tool_error;
            if (ComposeToolVisibilityTracking(head_view,
                    pose.device_to_absolute, world_from_tracking,
                    tool_error)) {
                // HPL gathers lights and billboards in UpdateRenderList.
                // Use the same collision-resolved palm as the visible hand.
                // A raw-only attachment visibly slides during stick motion.
                std::array<runtime::VrMatrix44, 2> palms{};
                std::array<bool, 2> palm_valid{};
                std::array<bool, 2> palm_resolved{};
                std::array<bool, 2> palm_gap_over_2cm{};
                const bool same_yaw_epoch = GameplayPalmYawEpoch() ==
                    g_world_yaw_epoch.load(std::memory_order_acquire);
                for (std::size_t index = 0; index < palms.size(); ++index) {
                    const auto& grip = frame.hands[index].grip;
                    if (!grip.device_connected || !grip.pose_valid) continue;
                    const auto raw = runtime::Multiply(world_from_tracking,
                        runtime::ExpandMatrix(grip.device_to_absolute));
                    runtime::VrMatrix44 resolved{};
                    const bool resolved_valid = same_yaw_epoch &&
                        ReadGameplayPalmPose(index, resolved);
                    palm_resolved[index] = resolved_valid;
                    if (resolved_valid) {
                        const float dx = raw.values[3] - resolved.values[3];
                        const float dy = raw.values[7] - resolved.values[7];
                        const float dz = raw.values[11] - resolved.values[11];
                        palm_gap_over_2cm[index] =
                            std::hypot(std::hypot(dx, dy), dz) > 0.02F;
                    }
                    palms[index] = SelectToolPalmPose(
                        raw, resolved, resolved_valid, same_yaw_epoch);
                    palm_valid[index] = true;
                }
                // Refresh the live native attachment before collection.
                const auto weights = AlignToolsForRender(
                    palms, palm_valid, pose, frame, sampled_at_ms);
                for (std::size_t index = 0; index < weights.size(); ++index) {
                    const float weight = weights[index];
                    if (weight > 0.0F) {
                        g_tools_visibility_aligned.fetch_add(
                            1, std::memory_order_relaxed);
                        (palm_resolved[index]
                            ? g_tools_visibility_resolved_palm
                            : g_tools_visibility_raw_palm).fetch_add(
                                1, std::memory_order_relaxed);
                        if (palm_gap_over_2cm[index])
                            g_tools_visibility_palm_gap_over_2cm.fetch_add(
                                1, std::memory_order_relaxed);
                    }
                }
            }
        }
    }
    original(renderer, world, camera, frame_time);
    if (!override.Restore(error)) {
        LogFailureOnce("visibility camera restore", error);
        return;
    }

    if (!inside_eye) {
        g_pending_visibility = {
            renderer, world, camera, pose, sampled_at_ms,
            g_presentation_generation.load(std::memory_order_acquire), true};
        if (!g_first_visibility_logged.exchange(true, std::memory_order_acq_rel)) {
            probe::WriteLog("Requiem first HMD visibility list prepared");
        }
    }
}

class StereoEyeScope final {
public:
    StereoEyeScope() noexcept : previous_(g_inside_stereo_eye) {
        g_inside_stereo_eye = true;
    }
    ~StereoEyeScope() { g_inside_stereo_eye = previous_; }
    StereoEyeScope(const StereoEyeScope&) = delete;
    StereoEyeScope& operator=(const StereoEyeScope&) = delete;
private:
    bool previous_ = false;
};

[[nodiscard]] bool RenderEye(RenderWorld original,
    void* renderer, void* world, void* camera,
    const runtime::VrMatrix44& head_view, std::size_t index,
    const std::array<graphics::TrackedHandVisual, 2>& hands,
    float frame_time, bool& world_rendered, std::string& error) noexcept {
    runtime::VrMatrix44 eye_view;
    if (!runtime::ComposeEyeViewFromHeadView(
            head_view, g_eyes[index].eye_to_head, eye_view, error)) return false;

    graphics::OpenGlEyeBinding binding;
    const graphics::Eye eye = index == 0
        ? graphics::Eye::left : graphics::Eye::right;
    if (!g_targets.BeginEye(eye, binding, error)) return false;

    adapters::hpl1::CameraMatrixOverride camera_override(kCameraLayout);
    const bool applied = camera_override.Apply(
        camera, eye_view, g_projections[index], error);
    if (applied) {
        hooks::ScopedEyeScissor scissor({
            binding.previous_viewport[2], binding.previous_viewport[3]});
        StereoEyeScope eye_scope;
        ScopedRefractionEyeCopy refraction_copy(g_targets, eye, binding);
        original(renderer, world, camera, frame_time);
        world_rendered = true;
        // Controller-anchored Rework mesh is presentation only. Native
        // interaction still requires a separately proven Requiem selection
        // boundary; drawing a hand must never gate world rendering.
        if (hands[0].visible || hands[1].visible) {
            std::string hand_error;
            static_cast<void>(graphics::DrawTrackedHands(
                hands, eye_view, g_projections[index], hand_error));
        }
    }

    std::string camera_restore_error;
    const bool camera_restored = !applied ||
        camera_override.Restore(camera_restore_error);
    std::string eye_restore_error;
    const bool eye_restored = g_targets.EndEye(binding, eye_restore_error);
    if (!camera_restored || !eye_restored) {
        error = !camera_restored
            ? "Could not restore Requiem camera: " + camera_restore_error
            : "Could not restore Requiem eye framebuffer: " + eye_restore_error;
    }
    return applied && camera_restored && eye_restored;
}

struct WorldPanelGeometry final {
    float distance = 0.0F;
    float width = 0.0F;
    float center_y = 0.0F;
};

[[nodiscard]] constexpr WorldPanelGeometry PanelGeometry(
    NativeUiSurface surface) noexcept {
    // Rework 23c890f / BP's 800x600 native inventory and notebook planes.
    return surface == NativeUiSurface::notebook
        ? WorldPanelGeometry{0.02F, 800.0F / 1450.0F, 0.0F}
        : WorldPanelGeometry{1.1F, 800.0F / 750.0F,
            -100.0F / 750.0F};
}

[[nodiscard]] bool RenderStereo(RenderWorld original,
    void* renderer, void* world, void* camera, float frame_time,
    bool& frame_time_consumed, std::string& error) noexcept {
    auto* session = g_session.load(std::memory_order_acquire);
    if (session == nullptr || wglGetCurrentContext() == nullptr) {
        error = "OpenVR session or OpenGL context is unavailable";
        return false;
    }
    const auto size = session->recommended_render_target_size();
    if (!g_targets.CreateOrResize(size.width, size.height, error)) return false;
    g_targets_destroyed.store(false, std::memory_order_release);
    if (!g_overlay_targets.CreateOrResize(800, 600, error)) return false;

    adapters::hpl1::CameraMatrixSnapshot camera_snapshot;
    if (!CaptureNativeGameplayCamera(camera, camera_snapshot, error)) return false;
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        error = "The Requiem world camera is not the mapped perspective camera";
        return false;
    }
    const VisibilityPresentation pending = g_pending_visibility;
    g_pending_visibility.valid = false;
    const ULONGLONG now = GetTickCount64();
    const bool reuse_visibility = pending.valid &&
        pending.renderer == renderer && pending.world == world &&
        pending.camera == camera &&
        pending.generation == g_presentation_generation.load(std::memory_order_acquire) &&
        now >= pending.sampled_at_ms &&
        now - pending.sampled_at_ms <= kMaximumVisibilityPoseAgeMs;
    runtime::VrMatrix44 head_view;
    runtime::VrHmdPose pose = pending.pose;
    if (reuse_visibility) {
        // Reuse the compositor pose, but align it to the camera as it stands
        // at RenderWorld. Native mouse yaw may change after visibility setup.
        if (!ComposeTrackedHeadView(camera_snapshot.view,
                pending.pose, head_view, error)) return false;
        if (!g_first_visibility_reuse_logged.exchange(
                true, std::memory_order_acq_rel)) {
            probe::WriteLog("Requiem stereo world reused its HMD visibility pose");
        }
    } else {
        if (!session->WaitForHmdPose(pose, error)) return false;
        if (!pose.device_connected || !pose.pose_valid) {
            error = "The HMD pose is not tracked";
            return false;
        }
        if (!ComposeTrackedHeadView(
                camera_snapshot.view, pose, head_view, error)) return false;
    }
    PublishTrackedHeadWorldPose(
        head_view, pose.device_to_absolute.values[7], now);

    std::array<graphics::TrackedHandVisual, 2> hands{};
    std::array<runtime::VrMatrix44, 2> raw_palms{};
    std::array<bool, 2> raw_palm_valid{};
    runtime::VrMatrix44 resolver_head_pose{};
    const auto controller_frame = ReadNativeControllerFrame();
    std::string hand_error;
    const bool resolver_head_valid = runtime::InvertRigidTransform(
        CollapseRigidView(head_view), resolver_head_pose, hand_error);
    runtime::VrMatrix44 tool_world_from_tracking{};
    bool tool_tracking_ready = false;
    if (controller_frame.focused && !NativeUiActive() &&
        resolver_head_valid) {
        if (ComposeToolVisibilityTracking(head_view,
                pose.device_to_absolute, tool_world_from_tracking, hand_error)) {
            tool_tracking_ready = true;
            AcquireSRWLockExclusive(&g_tracking_world_lock);
            g_world_from_tracking = tool_world_from_tracking;
            g_world_from_tracking_time = now;
            g_world_from_tracking_yaw_epoch = g_world_yaw_epoch.load(
                std::memory_order_acquire);
            g_world_from_tracking_identity = pose.identity;
            ReleaseSRWLockExclusive(&g_tracking_world_lock);
            for (std::size_t index = 0; index < hands.size(); ++index) {
                const auto& tracked = controller_frame.hands[index];
                if (!tracked.grip.device_connected ||
                    !tracked.grip.pose_valid) continue;
                auto& hand = hands[index];
                hand.visible = true;
                raw_palms[index] = runtime::Multiply(tool_world_from_tracking,
                    runtime::ExpandMatrix(tracked.grip.device_to_absolute));
                raw_palm_valid[index] = true;
                hand.palm = raw_palms[index];
                if (tracked.skeleton_valid) {
                    hand.curl = tracked.finger_curl;
                } else {
                    hand.curl.fill(0.1F);
                }
            }
        }
    }
    PublishGameplayPalmTracking(raw_palms, raw_palm_valid,
        resolver_head_pose, resolver_head_valid,
        g_world_yaw_epoch.load(std::memory_order_acquire));
    for (std::size_t index = 0; index < hands.size(); ++index) {
        runtime::VrMatrix44 resolved{};
        if (hands[index].visible && !NativeUiActive() &&
            ReadGameplayPalmPose(index, resolved)) {
            hands[index].palm = resolved;
        }
    }
    if (tool_tracking_ready) {
        std::array<runtime::VrMatrix44, 2> tool_palms{};
        std::array<bool, 2> tool_palm_valid{};
        for (std::size_t index = 0; index < hands.size(); ++index) {
            tool_palms[index] = hands[index].palm;
            tool_palm_valid[index] = hands[index].visible;
        }
        const auto tool_hold_weights = AlignToolsForRender(
            tool_palms, tool_palm_valid, pose, controller_frame, now);
        for (std::size_t index = 0; index < hands.size(); ++index)
            hands[index].hold_pose_weight = tool_hold_weights[index];
    }

    const auto plan = runtime::PlanStereoWorldRendering(
        frame_time, true, false);
    for (std::size_t index = 0; index < 2; ++index) {
        LARGE_INTEGER eye_started{}, eye_finished{};
        QueryPerformanceCounter(&eye_started);
        bool world_rendered = false;
        const bool rendered = RenderEye(original, renderer, world, camera,
            head_view, index, hands, plan.eye_frame_times[index],
            world_rendered, error);
        QueryPerformanceCounter(&eye_finished);
        if (eye_finished.QuadPart >= eye_started.QuadPart) {
            g_eye_ticks[index].fetch_add(static_cast<std::uint64_t>(
                eye_finished.QuadPart - eye_started.QuadPart),
                std::memory_order_relaxed);
        }
        if (!rendered) {
            if (world_rendered && index == 0) frame_time_consumed = true;
            return false;
        }
        if (index == 0) frame_time_consumed = true;
    }
    if (!adapters::hpl1::CameraMatchesSnapshot(
            camera, kCameraLayout, camera_snapshot)) {
        error = "Requiem camera changed across the stereo pair";
        return false;
    }

    const auto ui_surface = CurrentNativeUiSurface();
    const bool world_ui = ui_surface == NativeUiSurface::inventory ||
        ui_surface == NativeUiSurface::notebook;
    const bool closing_world_ui = !NativeUiActive() &&
        g_world_panel_valid;
    const auto closing_pose = g_world_panel_pose;
    const auto closing_surface = g_world_panel_surface;
    if (world_ui) {
        runtime::VrMatrix44 head_pose{}, tracking_from_head{};
        if (!runtime::InvertRigidTransform(
                CollapseRigidView(head_view), head_pose, error) ||
            !runtime::InvertRigidTransform(
                pose.device_to_absolute, tracking_from_head, error)) {
            return false;
        }
        const auto world_from_tracking = runtime::Multiply(
            head_pose, tracking_from_head);
        if (ui_surface == NativeUiSurface::inventory) {
            if (!g_world_panel_valid ||
                g_world_panel_surface != NativeUiSurface::inventory)
                g_world_panel_pose = head_pose;
        } else {
            const auto& frame = ReadNativeControllerFrame();
            const std::size_t off_hand = frame.interact_source ==
                runtime::VrHand::left ? 1U : 0U;
            const auto& grip = frame.hands[off_hand].grip;
            if (frame.focused && grip.device_connected && grip.pose_valid) {
                runtime::VrMatrix44 pitch = runtime::IdentityMatrix();
                pitch.values[5] = 0.0F;
                pitch.values[6] = 1.0F;
                pitch.values[9] = -1.0F;
                pitch.values[10] = 0.0F;
                runtime::VrMatrix44 offset = runtime::IdentityMatrix();
                offset.values[3] = (off_hand == 0U ? 175.0F :
                    -175.0F) / 1450.0F;
                g_world_panel_pose = runtime::Multiply(
                    runtime::Multiply(world_from_tracking,
                        runtime::Multiply(runtime::ExpandMatrix(
                            grip.device_to_absolute), pitch)), offset);
            } else if (!g_world_panel_valid ||
                       g_world_panel_surface != NativeUiSurface::notebook) {
                g_world_panel_pose = head_pose;
            }
        }
        g_world_panel_valid = true;
        g_world_panel_surface = ui_surface;
        const auto panel = PanelGeometry(ui_surface);
        AcquireSRWLockExclusive(&g_menu_pointer_lock);
        g_menu_pointer_world_panel = true;
        g_menu_pointer_world_from_tracking = world_from_tracking;
        g_menu_pointer_world_pose = g_world_panel_pose;
        g_menu_pointer_aspect = 4.0F / 3.0F;
        g_menu_pointer_distance = panel.distance;
        g_menu_pointer_width = panel.width;
        g_menu_pointer_center_y = panel.center_y;
        ReleaseSRWLockExclusive(&g_menu_pointer_lock);
    } else {
        g_world_panel_valid = false;
        g_world_panel_surface = NativeUiSurface::none;
    }
    g_overlay_world_panel = world_ui || closing_world_ui;
    g_overlay_surface = world_ui ? ui_surface : closing_surface;
    if (g_overlay_world_panel)
        g_overlay_panel_pose = world_ui ? g_world_panel_pose : closing_pose;

    const std::array<std::uint32_t, 2> textures{
        g_targets.target(graphics::Eye::left).color_texture,
        g_targets.target(graphics::Eye::right).color_texture};
    if (NativeUiActive() && !world_ui) {
        // HPL draws the native inventory/notebook after RenderWorld. Defer the
        // compositor frame until SDL swap can capture that finished 2D queue.
        // Clear the otherwise stale monitor buffer before HPL draws the UI.
        glPushAttrib(GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.015F, 0.015F, 0.02F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        glPopAttrib();
        g_pending_world_ui_pose = pose;
        g_pending_world_ui_pose_valid = true;
        g_world_ui_panel_pending.store(true, std::memory_order_release);
        return true;
    }
    g_pending_world_ui_pose_valid = false;
    if (g_draw_all_hook.installed()) {
        // Requiem draws messages/subtitles in DrawAll after RenderWorld. The
        // native queue is consumed once into an alpha target before submit.
        g_overlay_head_view = head_view;
        g_overlay_pending = true;
        return true;
    }
    LARGE_INTEGER submit_started{}, submit_finished{};
    QueryPerformanceCounter(&submit_started);
    const bool submitted = session->SubmitOpenGlEyeTextures(textures, error);
    QueryPerformanceCounter(&submit_finished);
    if (submit_finished.QuadPart >= submit_started.QuadPart)
        g_submit_ticks.fetch_add(static_cast<std::uint64_t>(
            submit_finished.QuadPart - submit_started.QuadPart),
            std::memory_order_relaxed);
    if (!submitted) return false;
    glFlush();
    if (!g_first_submit_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem first stereo world pair submitted (%lu x %lu)",
            static_cast<unsigned long>(size.width),
            static_cast<unsigned long>(size.height));
    }
    return true;
}

template <typename T>
[[nodiscard]] T ReadNative(const void* base, std::size_t offset) noexcept {
    if (base == nullptr) return T{};
    __try {
        return *reinterpret_cast<const T*>(
            static_cast<const std::uint8_t*>(base) + offset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
}

[[nodiscard]] bool StoreNativeBool(void* base, std::size_t offset,
    bool value) noexcept {
    if (base == nullptr) return false;
    __try {
        *reinterpret_cast<bool*>(static_cast<std::uint8_t*>(base) + offset) =
            value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
[[nodiscard]] bool StoreNative(void* base, std::size_t offset,
    const T& value) noexcept {
    if (base == nullptr) return false;
    __try {
        *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + offset) =
            value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

[[nodiscard]] bool TrackedControllerPose(runtime::VrHand hand, bool aim,
    runtime::VrMatrix44& pose) noexcept {
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused || NativeUiActive()) return false;
    const std::size_t index = hand == runtime::VrHand::left ? 0U : 1U;
    const auto& sample = frame.hands[index];
    const auto& tracked = aim && sample.aim.pose_valid
        ? sample.aim : sample.grip;
    if (!tracked.device_connected || !tracked.pose_valid) return false;
    runtime::VrMatrix44 world_from_tracking{};
    std::uint64_t sampled_at = 0;
    runtime::VrTrackingSampleIdentity identity{};
    AcquireSRWLockShared(&g_tracking_world_lock);
    world_from_tracking = g_world_from_tracking;
    sampled_at = g_world_from_tracking_time;
    identity = g_world_from_tracking_identity;
    ReleaseSRWLockShared(&g_tracking_world_lock);
    const std::uint64_t now = GetTickCount64();
    if (sampled_at == 0 || now < sampled_at || now - sampled_at > 100 ||
        (identity.pose_epoch != 0 && tracked.identity.pose_epoch != 0 &&
            !runtime::SameTrackingEpoch(identity, tracked.identity))) return false;
    pose = runtime::Multiply(world_from_tracking,
        runtime::ExpandMatrix(tracked.device_to_absolute));
    return true;
}

[[nodiscard]] bool TrackedToolGripPose(runtime::VrHand hand,
    float radius, runtime::VrMatrix44& pose) noexcept {
    runtime::VrMatrix44 raw{};
    if (!TrackedControllerPose(hand, false, raw)) return false;
    runtime::VrMatrix44 resolved{};
    const bool same_yaw_epoch = GameplayPalmYawEpoch() ==
        g_world_yaw_epoch.load(std::memory_order_acquire);
    const bool resolved_valid = same_yaw_epoch &&
        ReadGameplayPalmPose(hand == runtime::VrHand::left ? 0U : 1U,
            resolved);
    pose = runtime::rework_hand_profile::ApplyAttachmentGripLocalPose(
        SelectToolPalmPose(raw, resolved, resolved_valid, same_yaw_epoch),
        hand == runtime::VrHand::left,
        runtime::vr_interaction_policy::GripOpenCentreOffset(radius));
    return true;
}

[[nodiscard]] bool InteractionPalm(runtime::VrHand hand,
    runtime::VrMatrix44& pose) noexcept;
[[nodiscard]] bool RequiemBodyMatches(void* body) noexcept;

// Black Plague's ranked ray adapter preserves the native eligibility callback
// while gathering all hits from the palm centre and four adjacent fingers.
struct RankedRayCallback final {
    struct VTable {
        bool (__thiscall* before)(void*, void*);
        bool (__thiscall* intersect)(void*, void*, void*);
    };
    struct Hit {
        float t = 0.0F;
        float distance = 0.0F;
        std::array<float,3> normal{};
        std::array<float,3> point{};
    };
    VTable* vtable = nullptr;
    void* original = nullptr;
    void* best_body = nullptr;
    Hit best{};
    float best_score = INFINITY;
    std::size_t ray_index = 0;

    static bool __fastcall Before(void* self, void*, void* body) noexcept {
        auto* proxy = static_cast<RankedRayCallback*>(self);
        auto* native = ReadNative<void*>(proxy->original, 0);
        if (native == nullptr) return false;
        auto before = reinterpret_cast<VTable*>(native)->before;
        return before != nullptr && before(proxy->original, body);
    }
    static bool __fastcall Intersect(void* self, void*, void* body,
        void* params) noexcept {
        auto* proxy = static_cast<RankedRayCallback*>(self);
        if (body == nullptr || params == nullptr) return true;
        const Hit hit = ReadNative<Hit>(params, 0);
        if (!std::isfinite(hit.distance) || hit.distance < 0.0F)
            return true;
        g_ray_hits.fetch_add(1, std::memory_order_relaxed);
        // Rework's pick callback clears mpPickedBody when the nearest body
        // has no game entity. The five-ray rank must not replay a floor/wall
        // hit as its sole winner and mask a valid object on another ray.
        if (!RequiemBodyMatches(body)) return true;
        void* const entity = ReadNative<void*>(body, 0x414);
        if (entity == nullptr || !ReadNative<bool>(entity, 0x14))
            return true;
        if (g_selection_refresh_active) ++g_refresh_entity_hits;
        const float score = hit.distance +
            (proxy->ray_index == 0 ? 0.0F : 0.002F);
        if (score < proxy->best_score) {
            proxy->best_body = body;
            proxy->best = hit;
            proxy->best_score = score;
        }
        return true;
    }
};

struct PalmRaySegment final {
    std::array<float,3> from{};
    std::array<float,3> to{};
};

struct PalmSelectionCandidate final {
    void* body = nullptr;
    std::array<float,3> point{};
    float distance = INFINITY;
};

[[nodiscard]] bool SafeNativeRayBefore(void* callback, void* body) noexcept {
    if (callback == nullptr || body == nullptr) return false;
    __try {
        auto** vtable = *reinterpret_cast<void***>(callback);
        using Before = bool(__thiscall*)(void*, void*);
        return vtable != nullptr && vtable[0] != nullptr &&
            reinterpret_cast<Before>(vtable[0])(callback, body);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

[[nodiscard]] bool FindPalmOverlapTarget(void* callback,
    runtime::VrHand hand, PalmSelectionCandidate& selected) noexcept {
    selected = {};
    const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
    runtime::VrMatrix44 resolved{};
    if (!ReadGameplayPalmPose(hand_index, resolved)) return false;
    GameplayPalmOverlapResult overlap{};
    if (!QueryGameplayPalmOverlaps(hand_index, resolved, overlap) ||
        !overlap.valid) return false;
    const std::array<float,3> centre{
        resolved.values[3], resolved.values[7], resolved.values[11]};
    for (std::size_t hit_index = 0; hit_index < overlap.hit_count;
         ++hit_index) {
        const auto& hit = overlap.hits[hit_index];
        if (hit.body == nullptr || hit.contact_count == 0 ||
            !RequiemBodyMatches(hit.body) ||
            !SafeNativeRayBefore(callback, hit.body)) continue;
        void* const entity = ReadNative<void*>(hit.body, 0x414);
        if (entity == nullptr || !ReadNative<bool>(entity, 0x14)) continue;
        std::array<float,3> point{};
        for (std::size_t axis = 0; axis < point.size(); ++axis) {
            point[axis] = hit.contact_sum[axis] /
                static_cast<float>(hit.contact_count);
        }
        const float distance = std::hypot(
            std::hypot(point[0] - centre[0], point[1] - centre[1]),
            point[2] - centre[2]);
        if (!std::isfinite(distance) || distance >= selected.distance) continue;
        selected = {hit.body, point, distance};
    }
    return selected.body != nullptr;
}

[[nodiscard]] std::array<PalmRaySegment,5> InteractionRaySegments(
    const runtime::VrMatrix44& palm, float native_length) noexcept {
    using namespace runtime::vr_interaction_policy;
    const float forward = ClampPhysicalInteractionReach(native_length);
    constexpr std::array<std::array<float,2>,5> offsets{{
        {0.0F,0.0F},{1.0F,0.0F},{-1.0F,0.0F},
        {0.0F,1.0F},{0.0F,-1.0F}}};
    std::array<PalmRaySegment,5> rays{};
    for (std::size_t index = 0; index < rays.size(); ++index) {
        const float x = offsets[index][0] * kCollisionSizeX * 0.25F;
        const float y = offsets[index][1] * kCollisionSizeY * 0.35F;
        const auto point = [&](float z) -> std::array<float,3> {
            return {
                palm.values[0] * x + palm.values[1] * y +
                    palm.values[2] * z + palm.values[3],
                palm.values[4] * x + palm.values[5] * y +
                    palm.values[6] * z + palm.values[7],
                palm.values[8] * x + palm.values[9] * y +
                    palm.values[10] * z + palm.values[11]};
        };
        rays[index] = {point(kCollisionSizeZ * 0.5F), point(-forward)};
    }
    return rays;
}

void __fastcall HookedRay(void* world, void*, void* callback,
    const std::array<float,3>* origin,
    const std::array<float,3>* end,
    bool distance, bool normal, bool point, bool prefilter) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<Ray>(g_image + kCastRayRva);
    const auto return_rva = reinterpret_cast<std::uintptr_t>(
        _ReturnAddress()) - reinterpret_cast<std::uintptr_t>(g_image);
    if (g_presenting.load(std::memory_order_acquire) &&
        !NativeUiActive() && callback != nullptr &&
        return_rva == kNormalRayCallRva + kNormalRayCall.size() &&
        origin != nullptr && end != nullptr) {
        const auto frame = ReadNativeControllerFrame();
        runtime::VrMatrix44 palm{};
        if (InteractionPalm(frame.interact_source, palm)) {
            PalmSelectionCandidate contact{};
            if (FindPalmOverlapTarget(callback, frame.interact_source, contact)) {
                auto* native = reinterpret_cast<RankedRayCallback::VTable*>(
                    ReadNative<void*>(callback, 0));
                if (native != nullptr && native->intersect != nullptr) {
                    RankedRayCallback::Hit hit{};
                    hit.distance = contact.distance;
                    hit.point = contact.point;
                    native->intersect(callback, contact.body, &hit);
                    g_vr_selection_ready = true;
                    if (point) g_vr_selection_contact = {
                        contact.body, contact.point, GetTickCount64()};
                    if (g_selection_refresh_active) {
                        g_refresh_winner_distance = contact.distance;
                        g_refresh_winner_mass = ReadNative<float>(
                            contact.body, 0x434);
                    }
                    g_ray_winners.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
            const auto native_length = std::hypot(
                std::hypot((*end)[0] - (*origin)[0],
                           (*end)[1] - (*origin)[1]),
                (*end)[2] - (*origin)[2]);
            if (std::isfinite(native_length) && native_length > 0.0F &&
                native_length <= 20.0F) {
                RankedRayCallback::VTable table{
                    reinterpret_cast<bool(__thiscall*)(void*,void*)>(
                        RankedRayCallback::Before),
                    reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(
                        RankedRayCallback::Intersect)};
                RankedRayCallback ranked{&table, callback};
                const auto rays = InteractionRaySegments(palm,
                    native_length);
                const std::size_t count = g_selection_refresh_active
                    ? rays.size() : 1U;
                for (std::size_t index = 0; index < count; ++index) {
                    ranked.ray_index = index;
                    g_redirected_rays.fetch_add(1,
                        std::memory_order_relaxed);
                    original(world, &ranked, &rays[index].from,
                        &rays[index].to, distance, normal, point, prefilter);
                }
                if (ranked.best_body != nullptr) {
                    auto* native = reinterpret_cast<
                        RankedRayCallback::VTable*>(
                        ReadNative<void*>(callback, 0));
                    if (native != nullptr && native->intersect != nullptr) {
                        native->intersect(callback, ranked.best_body,
                            &ranked.best);
                        g_vr_selection_ready = true;
                        if (g_selection_refresh_active) {
                            if (point) g_vr_selection_contact = {
                                ranked.best_body, ranked.best.point,
                                GetTickCount64()};
                            g_refresh_winner_distance = ranked.best.distance;
                            g_refresh_winner_mass = ReadNative<float>(
                                ranked.best_body, 0x434);
                        }
                        g_ray_winners.fetch_add(1,
                            std::memory_order_relaxed);
                    }
                }
                return;
            }
        }
    }
    original(world, callback, origin, end,
        distance, normal, point, prefilter);
}

[[nodiscard]] bool RequiemBodyMatches(void* body) noexcept {
    return body != nullptr && ReadNative<void*>(body, 0) ==
        g_image + kPhysicsBodyVtableRva;
}

[[nodiscard]] bool InteractionPalm(runtime::VrHand hand,
    runtime::VrMatrix44& pose) noexcept {
    runtime::VrMatrix44 raw{};
    if (!TrackedControllerPose(hand, false, raw)) return false;
    pose = raw;
    runtime::VrMatrix44 resolved{};
    const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
    if (ReadGameplayPalmPose(hand_index, resolved)) {
        const std::array<float,3> reach{
            raw.values[3] - resolved.values[3],
            raw.values[7] - resolved.values[7],
            raw.values[11] - resolved.values[11]};
        const float distance = std::hypot(
            std::hypot(reach[0], reach[1]), reach[2]);
        if (!std::isfinite(distance)) return false;
        const float scale = distance >
                runtime::vr_interaction_policy::kMaximumCollisionInteractionReach &&
                distance > 0.0F
            ? runtime::vr_interaction_policy::kMaximumCollisionInteractionReach /
                distance
            : 1.0F;
        pose.values[3] = resolved.values[3] + reach[0] * scale;
        pose.values[7] = resolved.values[7] + reach[1] * scale;
        pose.values[11] = resolved.values[11] + reach[2] * scale;
    }
    pose = runtime::rework_hand_profile::ApplyVisualLocalPose(pose);
    return true;
}

[[nodiscard]] bool TrackingGripPosition(runtime::VrHand hand,
    std::array<float,3>& position) noexcept {
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused) return false;
    const auto& grip = frame.hands[hand == runtime::VrHand::left
        ? 0U : 1U].grip;
    if (!grip.device_connected || !grip.pose_valid) return false;
    position = {grip.device_to_absolute.values[3],
        grip.device_to_absolute.values[7],
        grip.device_to_absolute.values[11]};
    return std::all_of(position.begin(), position.end(),
        [](float value) { return std::isfinite(value); });
}

[[nodiscard]] std::size_t HandIndex(runtime::VrHand hand) noexcept {
    return hand == runtime::VrHand::left ? 0U : 1U;
}

[[nodiscard]] bool RefreshHeldPalm(void* player, runtime::VrHand hand,
    runtime::VrMatrix44& pose, std::uint64_t minimum_generation = 0,
    bool require_new_generation = false) noexcept {
    if (player == nullptr) return false;
    auto* const character_body = ReadNative<void*>(
        player, GameplayContract().character_body_offset);
    if (character_body == nullptr) return false;
    ServiceGameplayPalmResolver(g_image, character_body);
    const std::size_t hand_index = HandIndex(hand);
    if (require_new_generation &&
        GameplayPalmPoseGeneration(hand_index) <= minimum_generation) {
        return false;
    }
    return InteractionPalm(hand, pose);
}

using Vec3 = std::array<float,3>;
[[nodiscard]] Vec3 TransformPoint(const runtime::VrMatrix44& matrix,
    const Vec3& point) noexcept {
    return {
        matrix.values[0] * point[0] + matrix.values[1] * point[1] +
            matrix.values[2] * point[2] + matrix.values[3],
        matrix.values[4] * point[0] + matrix.values[5] * point[1] +
            matrix.values[6] * point[2] + matrix.values[7],
        matrix.values[8] * point[0] + matrix.values[9] * point[1] +
            matrix.values[10] * point[2] + matrix.values[11]};
}

[[nodiscard]] Vec3 InverseTransformPoint(
    const runtime::VrMatrix44& matrix, const Vec3& point) noexcept {
    const Vec3 delta{point[0] - matrix.values[3],
        point[1] - matrix.values[7], point[2] - matrix.values[11]};
    return {
        matrix.values[0] * delta[0] + matrix.values[4] * delta[1] +
            matrix.values[8] * delta[2],
        matrix.values[1] * delta[0] + matrix.values[5] * delta[1] +
            matrix.values[9] * delta[2],
        matrix.values[2] * delta[0] + matrix.values[6] * delta[1] +
            matrix.values[10] * delta[2]};
}

[[nodiscard]] int NativeJointCount(void* body) noexcept {
    if (!RequiemBodyMatches(body)) return -1;
    __try {
        return reinterpret_cast<int(__thiscall*)(void*)>(
            g_image + kGetJointCountRva)(body);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

[[nodiscard]] void* NativeJoint(void* body, int index) noexcept {
    if (!RequiemBodyMatches(body) || index < 0 || index > 255)
        return nullptr;
    __try {
        return reinterpret_cast<void*(__thiscall*)(void*,int)>(
            g_image + kGetBodyJointRva)(body, index);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

[[nodiscard]] bool BindNoJointSlider(void* body,
    MoveHold& hold) noexcept {
    auto* entity = ReadNative<void*>(body, 0x414);
    if (entity == nullptr || !ReadNative<bool>(entity, 0x14))
        return false;
    auto** begin = ReadNative<void**>(entity, 0x14C);
    auto** end = ReadNative<void**>(entity, 0x150);
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    if (first == 0 || last <= first || (last - first) % sizeof(void*) != 0)
        return false;
    const std::size_t count = (last - first) / sizeof(void*);
    if (count < 2 || count > 256) return false;
    const auto body_pose = ReadNative<runtime::VrMatrix44>(body, 0x34);
    const Vec3 body_position{body_pose.values[3],
        body_pose.values[7], body_pose.values[11]};
    if (!runtime::vr_mechanism_policy::Finite(body_position)) return false;
    for (std::size_t index = 0; index < count; ++index) {
        auto* candidate = ReadNative<void*>(begin, index * sizeof(void*));
        if (candidate == body || !RequiemBodyMatches(candidate) ||
            ReadNative<void*>(candidate, 0x414) != entity ||
            ReadNative<float>(candidate, 0x434) != 0.0F) continue;
        const auto frame = ReadNative<runtime::VrMatrix44>(candidate, 0x34);
        const Vec3 frame_position{
            frame.values[3], frame.values[7], frame.values[11]};
        const auto axis = runtime::vr_mechanism_policy::Subtract(
            body_position, frame_position);
        const float length = runtime::vr_mechanism_policy::Length(axis);
        if (!std::isfinite(length) || length <= 0.01F) continue;
        hold.mode = MoveMode::slider;
        hold.joint_pin = runtime::vr_mechanism_policy::Scale(
            axis, 1.0F / length);
        return true;
    }
    return false;
}

[[nodiscard]] bool BindNativeJoint(void* body,
    MoveHold& hold) noexcept {
    const int count = NativeJointCount(body);
    if (count <= 0 || count > 256) return false;
    void* joint = nullptr;
    for (int index = 0; index < count; ++index) {
        auto* candidate = NativeJoint(body, index);
        if (candidate == nullptr) continue;
        if (joint == nullptr) joint = candidate;
        if (ReadNative<void*>(candidate, 0x30) == body) {
            joint = candidate;
            break;
        }
    }
    // Rework/BP lever bodies can be the parent of the drive body.
    if (joint != nullptr && ReadNative<void*>(joint, 0x30) != body) {
        for (int index = 0; index < count; ++index) {
            auto* link = NativeJoint(body, index);
            if (link == nullptr ||
                ReadNative<void*>(link, 0x2C) != body) continue;
            auto* child = ReadNative<void*>(link, 0x30);
            const int child_count = NativeJointCount(child);
            for (int child_index = 0;
                child_index < child_count && child_index < 256;
                ++child_index) {
                auto* drive = NativeJoint(child, child_index);
                if (drive != nullptr && drive != link &&
                    ReadNative<void*>(drive, 0x30) == child) {
                    joint = drive;
                    break;
                }
            }
        }
    }
    if (joint == nullptr) return false;
    auto* vtable = ReadNative<void*>(joint, 0);
    int expected_type = 0;
    std::uintptr_t type_rva = 0;
    if (vtable == g_image + kHingeVtableRva &&
        ReadNative<void*>(vtable, 0x14) ==
            g_image + kHingeGetTypeRva) {
        hold.mode = MoveMode::hinge;
        expected_type = 1;
        type_rva = kHingeGetTypeRva;
    } else if (vtable == g_image + kSliderVtableRva &&
        ReadNative<void*>(vtable, 0x14) ==
            g_image + kSliderGetTypeRva) {
        hold.mode = MoveMode::slider;
        expected_type = 2;
        type_rva = kSliderGetTypeRva;
    } else return false;
    __try {
        if (reinterpret_cast<int(__thiscall*)(void*)>(
                g_image + type_rva)(joint) != expected_type) return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    hold.joint_pin = ReadNative<Vec3>(joint, 0xB8);
    hold.joint_pivot = ReadNative<Vec3>(joint, 0xC4);
    return runtime::vr_mechanism_policy::Finite(hold.joint_pin) &&
        runtime::vr_mechanism_policy::Finite(hold.joint_pivot) &&
        runtime::vr_mechanism_policy::Length(hold.joint_pin) > 1.0e-6F;
}

void SetBodyVelocity(void* body, std::uintptr_t rva,
    const std::array<float,3>& velocity) noexcept {
    reinterpret_cast<void(__thiscall*)(void*, const std::array<float,3>*)>(
        g_image + rva)(body, &velocity);
}

void RestoreGrab(const GrabHold& hold) noexcept {
    if (!RequiemBodyMatches(hold.body)) return;
    if (!StoreNativeBool(hold.body, 0x3C8,
            hold.collide_character)) return;
    reinterpret_cast<void(__thiscall*)(void*, float)>(
        g_image + 0x19CAC0)(hold.body, hold.max_linear);
    reinterpret_cast<void(__thiscall*)(void*, float)>(
        g_image + 0x19CAE0)(hold.body, hold.max_angular);
    const auto frame = ReadNativeControllerFrame();
    const auto& grip = frame.hands[hold.hand == runtime::VrHand::left
        ? 0U : 1U].grip;
    std::array<float,3> linear{}, angular{};
    if (frame.focused && grip.pose_valid && grip.device_connected) {
        runtime::VrMatrix44 transform{};
        AcquireSRWLockShared(&g_tracking_world_lock);
        transform = g_world_from_tracking;
        const auto sampled_at = g_world_from_tracking_time;
        ReleaseSRWLockShared(&g_tracking_world_lock);
        const auto now = GetTickCount64();
        if (sampled_at != 0 && now >= sampled_at &&
            now - sampled_at <= 100) {
            for (std::size_t row = 0; row < 3; ++row) {
                for (std::size_t column = 0; column < 3; ++column) {
                    linear[row] += transform.values[row * 4 + column] *
                        grip.velocity[column];
                    angular[row] += transform.values[row * 4 + column] *
                        grip.angular_velocity[column];
                }
            }
        } else hold.release_velocity.Estimate(linear, angular);
    } else hold.release_velocity.Estimate(linear, angular);
    SetBodyVelocity(hold.body, 0x19CA00,
        runtime::LimitTrackedVelocity(linear, 1.25F, 9.0F));
    SetBodyVelocity(hold.body, 0x19CA20,
        runtime::LimitTrackedVelocity(angular, 0.5F, 6.0F));
}

void __fastcall HookedGrabEnter(void* state, void*, void* previous) noexcept {
    ActiveCall active_call;
    auto* player = ReadNative<void*>(state, 0x10);
    const auto frame = ReadNativeControllerFrame();
    // Native Grab is the authority for accepting the target. BP tags the
    // redirected VR press even if its auxiliary picker found no winner: the
    // native callback may still accept a body in the same input update.
    const bool vr_origin = player != nullptr && frame.focused &&
        frame.input.state.interact.just_pressed;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    reinterpret_cast<Transition>(g_image + kGrabEnterRva)(state, previous);
    g_native_grab_enters.fetch_add(1, std::memory_order_relaxed);
    if (vr_origin) {
        g_grab_enters.fetch_add(1, std::memory_order_relaxed);
        g_pending_grab_state = state;
        g_pending_grab_hand = frame.interact_source;
    }
}

void __fastcall HookedGrabLeave(void* state, void*, void* next) noexcept {
    ActiveCall active_call;
    if (g_pending_grab_state == state) g_pending_grab_state = nullptr;
    const bool owned = g_grab_hold.state == state;
    GrabHold hold{};
    if (owned) {
        hold = g_grab_hold;
        g_grab_hold = {};
        PublishGameplayPalmHeldBody(HandIndex(hold.hand), nullptr);
    }
    reinterpret_cast<Transition>(g_image + kGrabLeaveRva)(state, next);
    if (owned) RestoreGrab(hold);
    if (owned) g_grabs_released.fetch_add(1, std::memory_order_relaxed);
}

void __fastcall HookedGrabUpdate(void* state, void*, float dt) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<HandsUpdate>(
        g_image + kGrabUpdateRva);
    if (state == g_pending_grab_state) {
        auto* player = ReadNative<void*>(state, 0x10);
        auto* body = ReadNative<void*>(state, 0x20);
        const auto frame = ReadNativeControllerFrame();
        runtime::VrMatrix44 palm{};
        std::array<float,3> tracked{};
        bool held_body_published = false;
        bool valid = player != nullptr &&
            ReadNative<int>(player, 0x2C0) == 6 &&
            ReadNative<void*>(ReadNative<void*>(player, 0x2C8),
                6 * sizeof(void*)) == state &&
            RequiemBodyMatches(body) &&
            ReadNative<void*>(body, 0x330) == nullptr &&
            ReadNative<void*>(body, 0x10) == nullptr &&
            NativeJointCount(body) == 0 &&
            frame.focused && frame.input.state.interact.pressed &&
            frame.interact_source == g_pending_grab_hand;
        if (valid) {
            const std::size_t hand_index = HandIndex(g_pending_grab_hand);
            const std::uint64_t previous_generation =
                GameplayPalmPoseGeneration(hand_index);
            PublishGameplayPalmHeldBody(hand_index, body);
            held_body_published = true;
            valid = RefreshHeldPalm(player, g_pending_grab_hand, palm,
                        previous_generation, true) &&
                TrackingGripPosition(g_pending_grab_hand, tracked);
        }
        if (valid) {
            const auto body_pose = ReadNative<runtime::VrMatrix44>(
                body, 0x34);
            const float max_linear = ReadNative<float>(body, 0x42C);
            const float max_angular = ReadNative<float>(body, 0x430);
            const float mass = ReadNative<float>(body, 0x434);
            const auto selected = g_vr_selection_contact;
            const auto now = GetTickCount64();
            std::array<float,3> local_contact{};
            bool surface_contact = false;
            if (selected.body == body && selected.sampled_at != 0 &&
                now >= selected.sampled_at &&
                now - selected.sampled_at <= 100) {
                const auto& point = selected.world_point;
                const float distance = std::hypot(
                    std::hypot(point[0] - palm.values[3],
                               point[1] - palm.values[7]),
                    point[2] - palm.values[11]);
                if (std::isfinite(distance) && distance <=
                    runtime::vr_interaction_policy::kInteractionTargetDistance) {
                    local_contact = InverseTransformPoint(body_pose, point);
                    surface_contact = runtime::vr_mechanism_policy::Finite(
                        local_contact);
                }
            }
            std::string error;
            GrabHold hold{};
            if (std::isfinite(max_linear) && max_linear >= 0.0F &&
                std::isfinite(max_angular) && max_angular >= 0.0F &&
                std::isfinite(mass) && mass > 0.0F &&
                hold.pose.Begin(palm, body_pose,
                    surface_contact ? local_contact : std::array<float,3>{},
                    true, error)) {
                hold.state = state;
                hold.player = player;
                hold.body = body;
                hold.hand = g_pending_grab_hand;
                hold.previous_palm = {
                    palm.values[3], palm.values[7], palm.values[11]};
                hold.previous_tracking = tracked;
                hold.yaw_epoch = GameplayPalmYawEpoch();
                hold.max_linear = max_linear;
                hold.max_angular = max_angular;
                hold.collide_character = ReadNative<bool>(body, 0x3C8);
                if (!StoreNativeBool(body, 0x3C8, false)) {
                    PublishGameplayPalmHeldBody(
                        HandIndex(g_pending_grab_hand), nullptr);
                    g_pending_grab_state = nullptr;
                    return;
                }
                g_grab_hold = hold;
                g_grabs_acquired.fetch_add(1, std::memory_order_relaxed);
                probe::WriteLog("Requiem VR grab acquired mass=%.3f surface=%u",
                    mass, surface_contact ? 1U : 0U);
                reinterpret_cast<void(__thiscall*)(void*, float)>(
                    g_image + 0x19CAC0)(body, 20.0F);
                reinterpret_cast<void(__thiscall*)(void*, float)>(
                    g_image + 0x19CAE0)(body, 30.0F);
            }
        }
        g_pending_grab_state = nullptr;
        if (g_grab_hold.state != state) {
            if (held_body_published)
                PublishGameplayPalmHeldBody(
                    HandIndex(g_pending_grab_hand), nullptr);
            // Use the mapped native exit; mouse-relative Grab cannot take over.
            reinterpret_cast<void(__thiscall*)(void*)>(
                g_image + kGrabExitRva)(state);
            return;
        }
    }
    if (g_grab_hold.state != state) {
        original(state, dt);
        return;
    }
    if (!std::isfinite(dt) || dt <= 0.0F || dt > 0.25F) return;
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused || !frame.input.state.interact.pressed ||
        frame.interact_source != g_grab_hold.hand ||
        !RequiemBodyMatches(g_grab_hold.body) ||
        ReadNative<void*>(state, 0x10) != g_grab_hold.player ||
        ReadNative<void*>(state, 0x20) != g_grab_hold.body ||
        ReadNative<void*>(g_grab_hold.player, 0x2C8) == nullptr ||
        ReadNative<int>(g_grab_hold.player, 0x2C0) != 6) {
        reinterpret_cast<void(__thiscall*)(void*)>(
            g_image + kGrabExitRva)(state);
        return;
    }
    runtime::VrMatrix44 palm{}, destination{};
    std::array<float,3> tracking{};
    std::string error;
    if (!RefreshHeldPalm(g_grab_hold.player, g_grab_hold.hand, palm) ||
        !TrackingGripPosition(g_grab_hold.hand, tracking) ||
        !g_grab_hold.pose.Update(palm, destination, error)) return;
    const std::uint64_t yaw_epoch = GameplayPalmYawEpoch();
    const float tracked_distance = std::hypot(
        std::hypot(tracking[0] - g_grab_hold.previous_tracking[0],
                   tracking[1] - g_grab_hold.previous_tracking[1]),
        tracking[2] - g_grab_hold.previous_tracking[2]);
    const float palm_distance = std::hypot(
        std::hypot(palm.values[3] - g_grab_hold.previous_palm[0],
                   palm.values[7] - g_grab_hold.previous_palm[1]),
        palm.values[11] - g_grab_hold.previous_palm[2]);
    if (!std::isfinite(tracked_distance) || tracked_distance > 0.35F ||
        (!std::isfinite(palm_distance) ||
         (palm_distance > 0.35F && yaw_epoch == g_grab_hold.yaw_epoch)))
        return;
    g_grab_hold.previous_tracking = tracking;
    g_grab_hold.previous_palm = {
        palm.values[3], palm.values[7], palm.values[11]};
    g_grab_hold.yaw_epoch = yaw_epoch;
    // Native state retains pickup/release ownership. Rework's shared rigid
    // grab pose owns only the free body's hand-relative transform.
    reinterpret_cast<void(__thiscall*)(void*, bool)>(
        g_image + 0x19CCF0)(g_grab_hold.body, false);
    if (!StoreNativeBool(g_grab_hold.body, 0x31, true)) {
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, bool)>(
        g_image + 0x19CB50)(g_grab_hold.body, true);
    reinterpret_cast<void(__thiscall*)(void*, bool)>(
        g_image + 0x19CBB0)(g_grab_hold.body, false);
    reinterpret_cast<EntitySetMatrix>(g_image + kEntitySetMatrixRva)(
        g_grab_hold.body, &destination);
    SetBodyVelocity(g_grab_hold.body, 0x19CA00, {});
    SetBodyVelocity(g_grab_hold.body, 0x19CA20, {});
}

void __fastcall HookedMoveEnter(void* state, void*, void* previous) noexcept {
    ActiveCall active_call;
    auto* player = ReadNative<void*>(state, 0x10);
    const auto frame = ReadNativeControllerFrame();
    const bool vr_origin = player != nullptr && frame.focused &&
        frame.input.state.interact.just_pressed;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    reinterpret_cast<Transition>(g_image + kMoveTargets[1])(
        state, previous);
    g_native_move_enters.fetch_add(1, std::memory_order_relaxed);
    if (vr_origin) {
        g_pending_move_state = state;
        g_pending_move_hand = frame.interact_source;
    }
}

void __fastcall HookedPushEnter(void* state, void*, void* previous) noexcept {
    ActiveCall active_call;
    auto* player = ReadNative<void*>(state, 0x10);
    const auto frame = ReadNativeControllerFrame();
    const bool vr_origin = player != nullptr && frame.focused &&
        frame.input.state.interact.just_pressed;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    reinterpret_cast<Transition>(g_image + kPushTargets[1])(
        state, previous);
    g_native_push_enters.fetch_add(1, std::memory_order_relaxed);
    // cPlayer::ChangeState (0x9CEE0) publishes player+0x2C0 at 0x9CF1B,
    // after the virtual Enter call at 0x9CF11. Validate the committed state
    // in HookedPushUpdate instead of rejecting every VR-origin Push here.
    if (vr_origin) {
        g_pending_push_state = state;
        g_pending_push_hand = frame.interact_source;
    }
}

void __fastcall HookedPushLeave(void* state, void*, void* next) noexcept {
    ActiveCall active_call;
    if (g_pending_push_state == state) g_pending_push_state = nullptr;
    if (g_push_hold.state == state) {
        PublishGameplayPalmHeldBody(HandIndex(g_push_hold.hand), nullptr);
        g_push_hold = {};
    }
    reinterpret_cast<Transition>(g_image + kPushTargets[2])(state, next);
}

void __fastcall HookedPushUpdate(void* state, void*, float dt) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<HandsUpdate>(
        g_image + kPushTargets[0]);
    if (state == g_pending_push_state) {
        auto* player = ReadNative<void*>(state, 0x10);
        auto* body = ReadNative<void*>(state, 0x50);
        const auto frame = ReadNativeControllerFrame();
        bool valid = player != nullptr && RequiemBodyMatches(body) &&
            ReadNative<int>(player, 0x2C0) == 1 &&
            ReadNative<void*>(ReadNative<void*>(player, 0x2C8),
                sizeof(void*)) == state &&
            frame.focused && frame.input.state.interact.pressed &&
            frame.interact_source == g_pending_push_hand;
        runtime::VrMatrix44 palm{};
        bool held_body_published = false;
        if (valid) {
            const std::size_t hand_index = HandIndex(g_pending_push_hand);
            const auto previous_generation = GameplayPalmPoseGeneration(
                hand_index);
            PublishGameplayPalmHeldBody(hand_index, body);
            held_body_published = true;
            valid = RefreshHeldPalm(player, g_pending_push_hand, palm,
                previous_generation, true);
        }
        if (valid) {
            const auto body_pose = ReadNative<runtime::VrMatrix44>(
                body, 0x34);
            const Vec3 palm_position{
                palm.values[3], palm.values[7], palm.values[11]};
            const Vec3 body_position{
                body_pose.values[3], body_pose.values[7],
                body_pose.values[11]};
            if (runtime::vr_mechanism_policy::Finite(palm_position) &&
                runtime::vr_mechanism_policy::Finite(body_position)) {
                g_push_hold = {state, player, body, g_pending_push_hand,
                    runtime::vr_mechanism_policy::Subtract(
                        palm_position, body_position),
                    GameplayPalmYawEpoch()};
                g_pushes_acquired.fetch_add(1, std::memory_order_relaxed);
                probe::WriteLog("Requiem VR push acquired");
            }
        }
        g_pending_push_state = nullptr;
        if (g_push_hold.state != state) {
            probe::WriteLog("Requiem VR push acquisition rejected state=%ld body=%u focus=%u hold=%u palm=%u",
                static_cast<long>(ReadNative<int>(player, 0x2C0)),
                RequiemBodyMatches(body) ? 1U : 0U,
                frame.focused ? 1U : 0U,
                frame.input.state.interact.pressed ? 1U : 0U,
                valid ? 1U : 0U);
            if (held_body_published)
                PublishGameplayPalmHeldBody(
                    HandIndex(g_pending_push_hand), nullptr);
            reinterpret_cast<void(__thiscall*)(void*)>(
                g_image + kPushExitRva)(state);
            return;
        }
    }
    if (g_push_hold.state != state) {
        original(state, dt);
        return;
    }
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused || !frame.input.state.interact.pressed ||
        frame.interact_source != g_push_hold.hand ||
        !RequiemBodyMatches(g_push_hold.body) ||
        ReadNative<void*>(state, 0x10) != g_push_hold.player ||
        ReadNative<void*>(state, 0x50) != g_push_hold.body ||
        ReadNative<int>(g_push_hold.player, 0x2C0) != 1) {
        reinterpret_cast<void(__thiscall*)(void*)>(
            g_image + kPushExitRva)(state);
        return;
    }
    runtime::VrMatrix44 palm{};
    if (!RefreshHeldPalm(g_push_hold.player,
            g_push_hold.hand, palm)) return;
    const auto body_pose = ReadNative<runtime::VrMatrix44>(
        g_push_hold.body, 0x34);
    if (g_push_hold.yaw_epoch != GameplayPalmYawEpoch()) {
        g_push_hold.body_relative_contact =
            runtime::vr_mechanism_policy::Subtract(
                Vec3{palm.values[3], palm.values[7], palm.values[11]},
                Vec3{body_pose.values[3], body_pose.values[7],
                    body_pose.values[11]});
        g_push_hold.yaw_epoch = GameplayPalmYawEpoch();
        return;
    }
    const Vec3 force = RequiemPushHandForce(
        {palm.values[3], palm.values[7], palm.values[11]},
        {body_pose.values[3], body_pose.values[7], body_pose.values[11]},
        g_push_hold.body_relative_contact);
    if (force[0] == 0.0F && force[2] == 0.0F) return;
    reinterpret_cast<void(__thiscall*)(void*,const Vec3*)>(
        g_image + kBodyAddForceRva)(g_push_hold.body, &force);
    g_push_force_ticks.fetch_add(1, std::memory_order_relaxed);
}

void __fastcall HookedMoveLeave(void* state, void*, void* next) noexcept {
    ActiveCall active_call;
    if (g_pending_move_state == state) g_pending_move_state = nullptr;
    const bool owned = g_move_hold.state == state;
    MoveHold hold{};
    if (owned) {
        hold = g_move_hold;
        g_move_hold = {};
        PublishGameplayPalmHeldBody(HandIndex(hold.hand), nullptr);
    }
    reinterpret_cast<Transition>(g_image + kMoveTargets[2])(state, next);
    if (owned && RequiemBodyMatches(hold.body)) {
        reinterpret_cast<void(__thiscall*)(void*,float)>(
            g_image + 0x19CAC0)(hold.body, hold.max_linear);
        reinterpret_cast<void(__thiscall*)(void*,float)>(
            g_image + 0x19CAE0)(hold.body, hold.max_angular);
        reinterpret_cast<void(__thiscall*)(void*,bool)>(
            g_image + 0x19CBB0)(hold.body, true);
        g_moves_released.fetch_add(1, std::memory_order_relaxed);
    }
}

void __fastcall HookedMoveUpdate(void* state, void*, float dt) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<HandsUpdate>(
        g_image + kMoveTargets[0]);
    if (state == g_pending_move_state) {
        auto* player = ReadNative<void*>(state, 0x10);
        auto* body = ReadNative<void*>(state, 0x54);
        const auto frame = ReadNativeControllerFrame();
        MoveHold hold{};
        runtime::VrMatrix44 interaction_palm{};
        runtime::VrMatrix44 palm{};
        Vec3 tracking{};
        bool held_body_published = false;
        const int joint_count = NativeJointCount(body);
        bool valid = player != nullptr &&
            ReadNative<int>(player, 0x2C0) == 2 &&
            ReadNative<void*>(ReadNative<void*>(player, 0x2C8),
                2 * sizeof(void*)) == state &&
            RequiemBodyMatches(body) && joint_count >= 0 &&
            frame.focused && frame.input.state.interact.pressed &&
            frame.interact_source == g_pending_move_hand &&
            InteractionPalm(g_pending_move_hand, interaction_palm);
        if (valid && joint_count == 0) {
            if (!BindNoJointSlider(body, hold) &&
                (ReadNative<void*>(body, 0x330) != nullptr ||
                 ReadNative<void*>(body, 0x10) != nullptr)) valid = false;
        } else if (valid) valid = BindNativeJoint(body, hold);
        if (valid) {
            const auto body_pose = ReadNative<runtime::VrMatrix44>(
                body, 0x34);
            hold.local_body_contact = ReadNative<Vec3>(state, 0x38);
            auto world_contact = TransformPoint(
                body_pose, hold.local_body_contact);
            const auto selected = g_vr_selection_contact;
            const auto now = GetTickCount64();
            const bool fresh_surface_contact =
                selected.body == body && selected.sampled_at != 0 &&
                now >= selected.sampled_at &&
                now - selected.sampled_at <= 100 &&
                runtime::vr_mechanism_policy::Finite(selected.world_point);
            if (fresh_surface_contact) {
                world_contact = selected.world_point;
                hold.local_body_contact = InverseTransformPoint(
                    body_pose, world_contact);
            }
            const Vec3 hand_position{
                interaction_palm.values[3], interaction_palm.values[7],
                interaction_palm.values[11]};
            const float distance = runtime::vr_mechanism_policy::Length(
                runtime::vr_mechanism_policy::Subtract(
                    world_contact, hand_position));
            const float mass = ReadNative<float>(body, 0x434);
            hold.max_linear = ReadNative<float>(body, 0x42C);
            hold.max_angular = ReadNative<float>(body, 0x430);
            valid = runtime::vr_mechanism_policy::Finite(world_contact) &&
                std::isfinite(distance) && distance <=
                    runtime::vr_interaction_policy::kInteractionTargetDistance &&
                std::isfinite(mass) && mass > 0.0F &&
                std::isfinite(hold.max_linear) && hold.max_linear >= 0.0F &&
                std::isfinite(hold.max_angular) && hold.max_angular >= 0.0F;
            if (valid && hold.mode == MoveMode::hinge) {
                hold.hinge_lightness = mass > 10.0F ? 2.25F :
                    mass >= 5.0F ? 1.75F : 1.35F;
            }
            if (valid) {
                hold.state = state;
                hold.player = player;
                hold.body = body;
                hold.hand = g_pending_move_hand;
                const std::size_t hand_index = HandIndex(hold.hand);
                const std::uint64_t previous_generation =
                    GameplayPalmPoseGeneration(hand_index);
                PublishGameplayPalmHeldBody(hand_index, body);
                held_body_published = true;
                valid = RefreshHeldPalm(player, hold.hand, palm,
                            previous_generation, true) &&
                    TrackingGripPosition(hold.hand, tracking);
            }
            if (valid) {
                hold.local_hand_contact = InverseTransformPoint(
                    palm, world_contact);
                hold.previous_palm = {
                    palm.values[3], palm.values[7], palm.values[11]};
                hold.previous_tracking = tracking;
                hold.previous_palm_pose = palm;
                hold.yaw_epoch = GameplayPalmYawEpoch();
                g_move_hold = hold;
                reinterpret_cast<void(__thiscall*)(void*,bool)>(
                    g_image + 0x19CBB0)(body, false);
                const float linear_limit = hold.mode == MoveMode::free_body
                    ? runtime::vr_mechanism_policy::kFreeMoveMaximumLinearSpeed
                    : runtime::vr_mechanism_policy::kJointedMaximumLinearSpeed;
                const float angular_limit = hold.mode == MoveMode::free_body
                    ? runtime::vr_mechanism_policy::kFreeMoveMaximumAngularSpeed
                    : runtime::vr_mechanism_policy::kJointedMaximumAngularSpeed *
                        hold.hinge_lightness;
                reinterpret_cast<void(__thiscall*)(void*,float)>(
                    g_image + 0x19CAC0)(body, linear_limit);
                reinterpret_cast<void(__thiscall*)(void*,float)>(
                    g_image + 0x19CAE0)(body, angular_limit);
                g_moves_acquired.fetch_add(1, std::memory_order_relaxed);
                if (hold.mode != MoveMode::free_body)
                    g_mechanism_acquired.fetch_add(1,
                        std::memory_order_relaxed);
            }
        }
        g_pending_move_state = nullptr;
        if (g_move_hold.state != state) {
            if (held_body_published)
                PublishGameplayPalmHeldBody(
                    HandIndex(g_pending_move_hand), nullptr);
            reinterpret_cast<void(__thiscall*)(void*)>(
                g_image + kMoveExitRva)(state);
            return;
        }
    }
    if (g_move_hold.state != state) {
        original(state, dt);
        return;
    }
    if (!std::isfinite(dt) || dt <= 0.0F || dt > 0.25F) return;
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused || !frame.input.state.interact.pressed ||
        frame.interact_source != g_move_hold.hand ||
        !RequiemBodyMatches(g_move_hold.body) ||
        ReadNative<void*>(state, 0x10) != g_move_hold.player ||
        ReadNative<void*>(state, 0x54) != g_move_hold.body ||
        ReadNative<int>(g_move_hold.player, 0x2C0) != 2) {
        reinterpret_cast<void(__thiscall*)(void*)>(
            g_image + kMoveExitRva)(state);
        return;
    }
    runtime::VrMatrix44 palm{};
    Vec3 tracking{};
    if (!RefreshHeldPalm(g_move_hold.player, g_move_hold.hand, palm) ||
        !TrackingGripPosition(g_move_hold.hand, tracking)) return;
    auto body_pose = ReadNative<runtime::VrMatrix44>(
        g_move_hold.body, 0x34);
    const std::uint64_t yaw_epoch = GameplayPalmYawEpoch();
    const bool yaw_changed = yaw_epoch != g_move_hold.yaw_epoch;
    const Vec3 palm_position{palm.values[3],
        palm.values[7], palm.values[11]};
    const float physical_distance = runtime::vr_mechanism_policy::Length(
        runtime::vr_mechanism_policy::Subtract(
            tracking, g_move_hold.previous_tracking));
    const float world_distance = runtime::vr_mechanism_policy::Length(
        runtime::vr_mechanism_policy::Subtract(
            palm_position, g_move_hold.previous_palm));
    if (!std::isfinite(physical_distance) || physical_distance > 0.35F ||
        !std::isfinite(world_distance) ||
        (world_distance > 0.35F && !yaw_changed)) return;
    if (yaw_changed && g_move_hold.mode == MoveMode::free_body) {
        runtime::VrMatrix34 previous{};
        std::copy_n(g_move_hold.previous_palm_pose.values.begin(),
            previous.values.size(), previous.values.begin());
        runtime::VrMatrix44 inverse{};
        std::string error;
        if (!runtime::InvertRigidTransform(previous, inverse, error)) return;
        body_pose = runtime::Multiply(
            runtime::Multiply(palm, inverse), body_pose);
        reinterpret_cast<EntitySetMatrix>(
            g_image + kEntitySetMatrixRva)(g_move_hold.body, &body_pose);
        SetBodyVelocity(g_move_hold.body, 0x19CA00, {});
        SetBodyVelocity(g_move_hold.body, 0x19CA20, {});
    } else if (yaw_changed) {
        g_move_hold.local_hand_contact = InverseTransformPoint(
            palm, TransformPoint(
                body_pose, g_move_hold.local_body_contact));
    }
    g_move_hold.previous_palm = palm_position;
    g_move_hold.previous_tracking = tracking;
    g_move_hold.previous_palm_pose = palm;
    g_move_hold.yaw_epoch = yaw_epoch;
    const auto current = TransformPoint(
        body_pose, g_move_hold.local_body_contact);
    const auto target = TransformPoint(
        palm, g_move_hold.local_hand_contact);
    const auto delta = runtime::vr_mechanism_policy::Subtract(target, current);
    const float mass = ReadNative<float>(g_move_hold.body, 0x434);
    if (!runtime::vr_mechanism_policy::Finite(delta) ||
        !std::isfinite(mass) || mass <= 0.0F) return;
    if (g_move_hold.mode == MoveMode::free_body) {
        const auto force = yaw_changed ? Vec3{} :
            runtime::vr_mechanism_policy::Scale(delta, 1250.0F * mass);
        reinterpret_cast<void(__thiscall*)(void*,
            const Vec3*,const Vec3*)>(g_image + 0x19D140)(
                g_move_hold.body, &force, &current);
    } else {
        const auto plan = g_move_hold.mode == MoveMode::slider
            ? runtime::vr_mechanism_policy::PlanSlider(
                delta, g_move_hold.joint_pin)
            : runtime::vr_mechanism_policy::PlanHinge(
                delta, g_move_hold.joint_pin,
                g_move_hold.joint_pivot, current,
                {body_pose.values[3], body_pose.values[7],
                 body_pose.values[11]}, g_move_hold.hinge_lightness);
        if (!plan.valid) return;
        SetBodyVelocity(g_move_hold.body, 0x19CA00,
            plan.linear_velocity);
        SetBodyVelocity(g_move_hold.body, 0x19CA20,
            plan.angular_velocity);
    }
    const int move_count = 20;
    static_cast<void>(StoreNative(state, 0x44, current));
    static_cast<void>(StoreNative(state, 0x58, move_count));
}

struct ToolProfile final {
    float radius = 0.0F;
    std::array<float,3> grip_point{};
    float rotation_x = 0.0F;
    float scale = 1.0F;
};

[[nodiscard]] ToolProfile ProfileForTool(ToolKind kind) noexcept {
    switch (kind) {
    case ToolKind::flashlight:
        return {kInstalledFlashlightGripRadius, {}, 0.0F, 1.0F};
    case ToolKind::glowstick:
        return {0.0125F, {0.0F,0.0078F,-0.078F}, 4.71F, 1.55F};
    case ToolKind::flare:
        return {0.018F, {0.001F,-0.0002F,-0.090F}, 4.71F, 1.8F};
    default:
        return {};
    }
}

[[nodiscard]] runtime::VrMatrix44 ReworkToolPose(
    const runtime::VrMatrix44& grip_pose,
    const ToolProfile& profile) noexcept {
    // Rework PlayerHands.cpp: grip * T(-VrGripPoint) * R(VrRotOffset) * S.
    runtime::VrMatrix44 grip = runtime::IdentityMatrix();
    grip.values[3] = -profile.grip_point[0];
    grip.values[7] = -profile.grip_point[1];
    grip.values[11] = -profile.grip_point[2];
    const float c = std::cos(profile.rotation_x);
    const float s = std::sin(profile.rotation_x);
    runtime::VrMatrix44 rotation = runtime::IdentityMatrix();
    rotation.values[5] = c;
    rotation.values[6] = -s;
    rotation.values[9] = s;
    rotation.values[10] = c;
    runtime::VrMatrix44 scale = runtime::IdentityMatrix();
    scale.values[0] = profile.scale;
    scale.values[5] = profile.scale;
    scale.values[10] = profile.scale;
    return runtime::Multiply(runtime::Multiply(runtime::Multiply(
        grip_pose, grip), rotation), scale);
}

[[nodiscard]] runtime::VrMatrix44 ToolPose(
    const runtime::VrMatrix44& grip_pose, ToolKind kind,
    const ToolProfile& profile) noexcept {
    return kind == ToolKind::flashlight
        ? InstalledFlashlightToolPose(grip_pose)
        : ReworkToolPose(grip_pose, profile);
}

[[nodiscard]] std::array<float, 2> AlignToolsForRender(
    const std::array<runtime::VrMatrix44, 2>& palms,
    const std::array<bool, 2>& palm_valid,
    const runtime::VrHmdPose& head,
    const runtime::VrControllerFrame& frame,
    std::uint64_t now) noexcept {
    // Native Hands::Update precedes visibility collection and RenderWorld.
    // Refresh only live attachments against the current tracking sample at
    // both boundaries: before HPL collects child lights/billboards, and again
    // before the eyes draw the tool mesh and visible fingers.
    std::array<float, 2> hold_weights{};
    for (const auto& tool : g_tool_attachments) {
        if (tool.kind == ToolKind::none || tool.hands == nullptr ||
            tool.model == nullptr || tool.entity == nullptr ||
            tool.updated_at == 0 || now < tool.updated_at ||
            now - tool.updated_at > 100 ||
            ReadNative<int>(tool.hands, 0x74) != 2 ||
            ReadNative<void*>(tool.hands, tool.slot) != tool.model ||
            ReadNative<void*>(tool.model, 0x118) != tool.entity)
            continue;
        const auto& grip = frame.hands[tool.hand == runtime::VrHand::left
            ? 0U : 1U].grip;
        if (!grip.device_connected || !grip.pose_valid ||
            (head.identity.pose_epoch != 0 &&
             grip.identity.pose_epoch != 0 &&
             !runtime::SameTrackingEpoch(head.identity, grip.identity)))
            continue;
        const auto profile = ProfileForTool(tool.kind);
        const auto hand_index = tool.hand == runtime::VrHand::left ? 0U : 1U;
        if (!palm_valid[hand_index]) continue;
        const auto socket = runtime::rework_hand_profile::
            ApplyAttachmentGripLocalPose(palms[hand_index],
                tool.hand == runtime::VrHand::left,
                runtime::vr_interaction_policy::GripOpenCentreOffset(
                    profile.radius));
        const auto matrix = ToolPose(socket, tool.kind, profile);
        reinterpret_cast<EntitySetMatrix>(g_image + kEntitySetMatrixRva)(
            tool.entity, &matrix);
        // Rework's attached-tool grip and BP's shared hold-pose weight keep
        // the visible fingers wrapped around the live native attachment.
        hold_weights[hand_index] = std::max(hold_weights[hand_index],
            runtime::vr_interaction_policy::GripPoseWeight(profile.radius));
        g_tools_render_aligned.fetch_add(1, std::memory_order_relaxed);
    }
    return hold_weights;
}

void __fastcall HookedHandsUpdate(void* hands, void*, float dt) noexcept {
    ActiveCall active_call;
    void* previous = g_updating_hands;
    g_updating_hands = hands;
    g_tool_attachments = {};
    reinterpret_cast<HandsUpdate>(g_image + kHandsUpdateRva)(hands, dt);
    g_updating_hands = previous;
}

void __fastcall HookedToolMatrix(void* entity, void*,
    const runtime::VrMatrix44* native_matrix) noexcept {
    ActiveCall active_call;
    runtime::VrMatrix44 attached{};
    const runtime::VrMatrix44* selected = native_matrix;
    if (g_presenting.load(std::memory_order_acquire) &&
        g_updating_hands != nullptr && !NativeUiActive() &&
        ReadNative<int>(g_updating_hands, 0x74) == 2) {
        using Equal = bool(__cdecl*)(const void*, const char*);
        const auto equal = reinterpret_cast<Equal>(
            ReadNative<void*>(g_image, kLegacyStringEqualIatRva));
        for (const auto slot : {0x6CU, 0x70U}) {
            auto* model = ReadNative<void*>(g_updating_hands, slot);
            if (model == nullptr ||
                ReadNative<void*>(model, 0x118) != entity || equal == nullptr)
                continue;
            const auto* name = static_cast<const std::uint8_t*>(model) + 4;
            const bool flashlight = equal(name, "Flashlight");
            const bool glowstick = equal(name, "Glowstick");
            const bool flare = equal(name, "Flare");
            if (!flashlight && !glowstick && !flare) break;
            // Glowstick/flare use the Rework HUD profiles. The flashlight
            // uses the installed DAE's measured grip with Requiem orientation.
            const ToolKind kind = flashlight ? ToolKind::flashlight
                : glowstick ? ToolKind::glowstick : ToolKind::flare;
            const ToolProfile profile = ProfileForTool(kind);
            const auto frame = ReadNativeControllerFrame();
            const auto tool_hand = frame.interact_source == runtime::VrHand::left
                ? runtime::VrHand::right : runtime::VrHand::left;
            runtime::VrMatrix44 grip{};
            if (!TrackedToolGripPose(tool_hand, profile.radius,
                    grip)) break;
            attached = ToolPose(grip, kind, profile);
            selected = &attached;
            g_tool_attachments[slot == 0x6CU ? 0U : 1U] = {
                g_updating_hands, model, entity, slot, kind, tool_hand,
                GetTickCount64()};
            break;
        }
    }
    (selected == native_matrix ? g_tools_native : g_tools_attached)
        .fetch_add(1, std::memory_order_relaxed);
    reinterpret_cast<EntitySetMatrix>(g_image + kEntitySetMatrixRva)(
        entity, selected);
}

void __fastcall HookedDrawAll(void* drawer, void*) noexcept {
    ActiveCall active_call;
    const auto original = g_original_draw_all.load(std::memory_order_acquire);
    if (original == nullptr) return;
    if (!g_overlay_pending || !g_presenting.load(std::memory_order_acquire)) {
        original(drawer);
        return;
    }
    g_overlay_pending = false;
    LARGE_INTEGER overlay_started{}, overlay_finished{};
    QueryPerformanceCounter(&overlay_started);
    std::string error;
    graphics::OpenGlEyeBinding overlay_binding;
    bool captured = false;
    if (g_overlay_targets.BeginEye(graphics::Eye::left,
            overlay_binding, error)) {
        glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT | GL_SCISSOR_BIT);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0, 0, 0, 0);
        glClearDepth(1.0);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);
        glPopAttrib();
        original(drawer);
        std::string restore_error;
        captured = g_overlay_targets.EndEye(overlay_binding, restore_error);
        if (!captured) error = restore_error;
    } else {
        // The HPL queue has one owner and must still be drained once.
        original(drawer);
    }
    if (captured) {
        runtime::VrMatrix44 head_pose{};
        captured = runtime::InvertRigidTransform(
            CollapseRigidView(g_overlay_head_view), head_pose, error);
        for (std::size_t index = 0; index < 2 && captured; ++index) {
            runtime::VrMatrix44 eye_view{};
            captured = runtime::ComposeEyeViewFromHeadView(
                g_overlay_head_view, g_eyes[index].eye_to_head,
                eye_view, error);
            if (!captured) break;
            graphics::OpenGlEyeBinding eye_binding;
            captured = g_targets.BeginEye(index == 0
                ? graphics::Eye::left : graphics::Eye::right,
                eye_binding, error);
            if (!captured) break;
            const auto model_view = runtime::Multiply(eye_view,
                g_overlay_world_panel ? g_overlay_panel_pose : head_pose);
            const auto panel = PanelGeometry(g_overlay_surface);
            // Rework scales text inside its 800x600 message plane. Requiem
            // captures text and diary animation together, so scaling the
            // entire plane by 3.375 at 1.75 m made both fill the headset.
            // Use the proven 1/750 m pixel pitch with a 2.5 scale at 2.5 m:
            // the full plane is ~44 degrees high and remains in view.
            constexpr float subtitle_scale = 2.5F;
            const float half_width = g_overlay_world_panel
                ? panel.width * 0.5F : 400.0F / 750.0F * subtitle_scale;
            const float half_height = g_overlay_world_panel
                ? panel.width * 0.375F : 300.0F / 750.0F * subtitle_scale;
            const float center_y = g_overlay_world_panel
                ? panel.center_y : 0.0F;
            captured = graphics::DrawTransparentOverlay(
                g_overlay_targets.target(graphics::Eye::left).color_texture,
                model_view, g_projections[index],
                -half_width, half_width,
                center_y - half_height, center_y + half_height,
                g_overlay_world_panel ? panel.distance :
                    kGameplayOverlayDistance,
                error);
            std::string restore_error;
            if (!g_targets.EndEye(eye_binding, restore_error)) {
                captured = false;
                error = restore_error;
            }
        }
    }
    if (!captured) LogFailureOnce("gameplay 2D overlay", error);
    QueryPerformanceCounter(&overlay_finished);
    if (overlay_finished.QuadPart >= overlay_started.QuadPart)
        g_overlay_ticks.fetch_add(static_cast<std::uint64_t>(
            overlay_finished.QuadPart - overlay_started.QuadPart),
            std::memory_order_relaxed);
    auto* session = g_session.load(std::memory_order_acquire);
    if (session != nullptr) {
        const std::array<std::uint32_t, 2> textures{
            g_targets.target(graphics::Eye::left).color_texture,
            g_targets.target(graphics::Eye::right).color_texture};
        LARGE_INTEGER submit_started{}, submit_finished{};
        QueryPerformanceCounter(&submit_started);
        const bool submitted = session->SubmitOpenGlEyeTextures(textures, error);
        QueryPerformanceCounter(&submit_finished);
        if (submit_finished.QuadPart >= submit_started.QuadPart)
            g_submit_ticks.fetch_add(static_cast<std::uint64_t>(
                submit_finished.QuadPart - submit_started.QuadPart),
                std::memory_order_relaxed);
        if (!submitted) {
            LogFailureOnce("deferred world submit", error);
        } else {
            glFlush();
        }
    }
}

void __fastcall HookedRenderWorld(void* renderer, void*,
    void* world, void* camera, float frame_time) noexcept {
    ActiveCall active_call;
    const auto original = g_original.load(std::memory_order_acquire);
    if (original == nullptr) return;
    if (!g_presenting.load(std::memory_order_acquire)) {
        original(renderer, world, camera, frame_time);
        return;
    }
    bool frame_time_consumed = false;
    std::string error;
    LARGE_INTEGER started{}, finished{};
    QueryPerformanceCounter(&started);
    const bool presented = RenderStereo(original, renderer, world, camera,
        frame_time, frame_time_consumed, error);
    QueryPerformanceCounter(&finished);
    if (finished.QuadPart >= started.QuadPart) {
        g_world_timing_frames.fetch_add(1, std::memory_order_relaxed);
        g_world_render_ticks.fetch_add(
            static_cast<std::uint64_t>(finished.QuadPart - started.QuadPart),
            std::memory_order_relaxed);
    }
    if (presented) {
        g_frame_presentation.MarkWorldPresented();
        return;
    }
    LogFailureOnce("stereo world", error);
    original(renderer, world, camera,
        frame_time_consumed ? 0.0F : frame_time);
}

} // namespace

bool InstallRenderWorld(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed() || g_visibility_hook.installed() ||
        g_draw_all_hook.installed() || g_hands_update_hook.installed() ||
        g_refraction_copy_hook.installed() ||
        g_tool_matrix_hook.installed() || g_ray_hook.installed() ||
        std::any_of(g_grab_hooks.begin(), g_grab_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        std::any_of(g_move_state_hooks.begin(), g_move_state_hooks.end(),
            [](const auto& hook) { return hook.installed(); })) {
        error = "Requiem render hooks are already installed";
        return false;
    }
    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "Requiem main image is unavailable";
        return false;
    }
    if (g_image != nullptr && g_image != image) {
        error = "Requiem image base changed after tool-hook publication";
        return false;
    }
    auto* call = image + kRenderWorldCallRva;
    auto* visibility_call = image + kUpdateRenderListCallRva;
    auto* draw_all_call = image + kDrawAllCallRva;
    if (!std::equal(kGetViewMatrixEntry.begin(), kGetViewMatrixEntry.end(),
            image + kGetViewMatrixRva) ||
        !std::equal(kGetProjectionMatrixEntry.begin(), kGetProjectionMatrixEntry.end(),
            image + kGetProjectionMatrixRva)) {
        error = "Requiem native camera getters do not match the exact build";
        return false;
    }
    if (!std::equal(kRenderWorldCall.begin(), kRenderWorldCall.end(), call)) {
        error = "Requiem initialized RenderWorld call does not match the exact build";
        return false;
    }
    if (!std::equal(kUpdateRenderListCall.begin(),
            kUpdateRenderListCall.end(), visibility_call)) {
        error = "Requiem initialized UpdateRenderList call does not match the exact build";
        return false;
    }
    if (!std::equal(kDrawAllCall.begin(), kDrawAllCall.end(),
            draw_all_call) ||
        !std::equal(kDrawAllEntry.begin(), kDrawAllEntry.end(),
            image + kDrawAllRva)) {
        error = "Requiem DrawAll boundary does not match the exact build";
        return false;
    }
    if (ReadNative<void*>(image, kCopyContextToTextureSlotRva) !=
            image + kCopyContextToTextureRva ||
        !std::equal(kCopyContextToTextureEntry.begin(),
            kCopyContextToTextureEntry.end(),
            image + kCopyContextToTextureRva) ||
        !std::equal(kRefractionClippedCopyCall.begin(),
            kRefractionClippedCopyCall.end(),
            image + kRefractionClippedCopyCallRva) ||
        !std::equal(kRefractionClippedCopyCall.begin(),
            kRefractionClippedCopyCall.end(),
            image + kRefractionFullCopyCallRva)) {
        error = "Requiem exact-build refraction screen-copy boundary mismatch";
        return false;
    }
    const auto* hands_slot = image + kHandsUpdateSlotRva;
    if (ReadNative<void*>(hands_slot, 0) != image + kHandsUpdateRva ||
        !std::equal(kHandsUpdateEntry.begin(), kHandsUpdateEntry.end(),
            image + kHandsUpdateRva) ||
        !std::equal(kToolMatrixCall.begin(), kToolMatrixCall.end(),
            image + kToolMatrixCallRva) ||
        !std::equal(kEntitySetMatrixEntry.begin(),
            kEntitySetMatrixEntry.end(), image + kEntitySetMatrixRva) ||
        ReadNative<void*>(image, kRaySlotRva) !=
            image + kCastRayRva ||
        !std::equal(kCastRayEntry.begin(), kCastRayEntry.end(),
            image + kCastRayRva) ||
        !std::equal(kNormalUpdateEntry.begin(),
            kNormalUpdateEntry.end(), image + kNormalUpdateRva) ||
        ReadNative<void*>(image, kNormalVtableRva + 4) !=
            image + kNormalUpdateRva ||
        ReadNative<void*>(image, kNormalVtableRva + 0x10) !=
            image + kNormalInteractRva ||
        !std::equal(kGetPickedBodyEntry.begin(),
            kGetPickedBodyEntry.end(), image + kGetPickedBodyRva) ||
        !std::equal(kNormalInteractEntry.begin(),
            kNormalInteractEntry.end(), image + kNormalInteractRva) ||
        !std::equal(kNormalRayCall.begin(), kNormalRayCall.end(),
            image + kNormalRayCallRva) ||
        !std::equal(kGetJointCountEntry.begin(),
            kGetJointCountEntry.end(), image + kGetJointCountRva) ||
        !std::equal(kGrabUpdateEntry.begin(),
            kGrabUpdateEntry.end(), image + kGrabUpdateRva) ||
        !std::equal(kGrabEnterEntry.begin(),
            kGrabEnterEntry.end(), image + kGrabEnterRva) ||
        !std::equal(kGrabLeaveEntry.begin(),
            kGrabLeaveEntry.end(), image + kGrabLeaveRva) ||
        !std::equal(kGrabExitEntry.begin(),
            kGrabExitEntry.end(), image + kGrabExitRva) ||
        !std::equal(kMoveExitEntry.begin(),
            kMoveExitEntry.end(), image + kMoveExitRva) ||
        ReadNative<void*>(image, kGrabUpdateSlotRva) !=
            image + kGrabUpdateRva ||
        ReadNative<void*>(image, kGrabEnterSlotRva) !=
            image + kGrabEnterRva ||
        ReadNative<void*>(image, kGrabLeaveSlotRva) !=
            image + kGrabLeaveRva) {
        error = "Requiem native hand/tool boundary does not match the exact build";
        return false;
    }
    constexpr std::array<std::array<std::uintptr_t,2>, 7> body_methods{{
        {0x34, 0x19CA00}, {0x3C, 0x19CA20},
        {0x54, 0x19CAC0}, {0x5C, 0x19CAE0},
        {0x8C, 0x19CB50}, {0x94, 0x19CBB0},
        {0xBC, 0x19CCF0}}};
    for (const auto& method : body_methods) {
        if (ReadNative<void*>(image + kPhysicsBodyVtableRva,
                method[0]) != image + method[1]) {
            error = "Requiem free-body method boundary mismatch";
            return false;
        }
    }
    if (ReadNative<void*>(image + kPhysicsBodyVtableRva, 0x7C) !=
            image + 0x19D140 ||
        !std::equal(kGetBodyJointEntry.begin(),
            kGetBodyJointEntry.end(), image + kGetBodyJointRva) ||
        ReadNative<void*>(image + kHingeVtableRva, 0x14) !=
            image + kHingeGetTypeRva ||
        ReadNative<void*>(image + kSliderVtableRva, 0x14) !=
            image + kSliderGetTypeRva) {
        error = "Requiem native Move/joint boundary mismatch";
        return false;
    }
    constexpr std::array<std::array<std::uintptr_t,2>,4> joint_fields{{
        {0x19F957,0xB8}, {0x19F96B,0xC4},
        {0x19FE7C,0xB8}, {0x19FE90,0xC4}}};
    for (const auto& field : joint_fields) {
        if (image[field[0]] != 0x89 || image[field[0]+1] != 0x86 ||
            ReadNative<std::uint32_t>(image, field[0]+2) != field[1]) {
            error = "Requiem native joint pin/pivot field mismatch";
            return false;
        }
    }
    for (std::size_t index = 0; index < kMoveSlots.size(); ++index) {
        if (ReadNative<void*>(image, kMoveSlots[index]) !=
                image + kMoveTargets[index] ||
            !std::equal(kMoveEntries[index].begin(),
                kMoveEntries[index].end(),
                image + kMoveTargets[index])) {
            error = "Requiem native Move state boundary mismatch";
            return false;
        }
    }
    if (ReadNative<void*>(image + kPhysicsBodyVtableRva, 0x78) !=
            image + kBodyAddForceRva ||
        image[kPushExitRva] != 0x83 ||
        image[kPushExitRva + 1] != 0xEC ||
        ReadNative<void*>(image, kPushVtableRva + 0x4C) !=
            image + 0xAD190 ||
        ReadNative<void*>(image, kPushVtableRva + 0x50) !=
            image + 0xAB710) {
        error = "Requiem native Push force/movement boundary mismatch";
        return false;
    }
    for (std::size_t index = 0; index < kPushSlots.size(); ++index) {
        if (ReadNative<void*>(image, kPushSlots[index]) !=
                image + kPushTargets[index] ||
            !std::equal(kPushEntries[index].begin(),
                kPushEntries[index].end(),
                image + kPushTargets[index])) {
            error = "Requiem native Push state boundary mismatch";
            return false;
        }
    }
    const auto crt = GetModuleHandleW(L"MSVCP71.dll");
    const auto compare = crt ? GetProcAddress(crt,
        "??$?8DU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PBD@Z")
        : nullptr;
    if (compare == nullptr ||
        ReadNative<void*>(image, kLegacyStringEqualIatRva) !=
            reinterpret_cast<void*>(compare)) {
        error = "Requiem HUD string comparison import mismatch";
        return false;
    }
    g_image = image;
    g_original.store(reinterpret_cast<RenderWorld>(image + kRenderWorldRva),
        std::memory_order_release);
    g_original_visibility.store(
        reinterpret_cast<UpdateRenderList>(image + kUpdateRenderListRva),
        std::memory_order_release);
    g_original_draw_all.store(reinterpret_cast<DrawAll>(image + kDrawAllRva),
        std::memory_order_release);
    if (!hooks::InstallRel32CallHook(visibility_call, kUpdateRenderListCall,
            reinterpret_cast<void*>(&HookedUpdateRenderList),
            g_visibility_hook, error)) {
        g_original.store(nullptr, std::memory_order_release);
        g_original_visibility.store(nullptr, std::memory_order_release);
        return false;
    }
    if (!hooks::InstallRel32CallHook(call, kRenderWorldCall,
            reinterpret_cast<void*>(&HookedRenderWorld), g_hook, error)) {
        std::string rollback_error;
        if (!hooks::RemoveRel32CallHook(g_visibility_hook, rollback_error)) {
            error += "; visibility rollback failed: " + rollback_error;
        } else {
            g_original.store(nullptr, std::memory_order_release);
            g_original_visibility.store(nullptr, std::memory_order_release);
        }
        return false;
    }
    if (!hooks::InstallRel32CallHook(draw_all_call, kDrawAllCall,
            reinterpret_cast<void*>(&HookedDrawAll), g_draw_all_hook, error)) {
        std::string rollback_error;
        const bool render_removed = hooks::RemoveRel32CallHook(g_hook, rollback_error);
        const bool visibility_removed = hooks::RemoveRel32CallHook(
            g_visibility_hook, rollback_error);
        if (!render_removed || !visibility_removed) {
            error += "; render hook rollback failed: " + rollback_error;
        }
        return false;
    }
    if (!hooks::InstallPointerHook(
            reinterpret_cast<void**>(image + kHandsUpdateSlotRva),
            image + kHandsUpdateRva,
            reinterpret_cast<void*>(&HookedHandsUpdate),
            g_hands_update_hook, error) ||
        !hooks::InstallRel32CallHook(image + kToolMatrixCallRva,
            kToolMatrixCall, reinterpret_cast<void*>(&HookedToolMatrix),
            g_tool_matrix_hook, error)) {
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    if (!hooks::InstallPointerHook(
            reinterpret_cast<void**>(image + kRaySlotRva),
            image + kCastRayRva,
            reinterpret_cast<void*>(&HookedRay), g_ray_hook, error)) {
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    constexpr std::array<std::uintptr_t,3> grab_slots{
        kGrabUpdateSlotRva, kGrabEnterSlotRva, kGrabLeaveSlotRva};
    constexpr std::array<std::uintptr_t,3> grab_targets{
        kGrabUpdateRva, kGrabEnterRva, kGrabLeaveRva};
    const std::array<void*,3> grab_replacements{
        reinterpret_cast<void*>(&HookedGrabUpdate),
        reinterpret_cast<void*>(&HookedGrabEnter),
        reinterpret_cast<void*>(&HookedGrabLeave)};
    for (std::size_t index = 0; index < grab_slots.size(); ++index) {
        if (hooks::InstallPointerHook(
                reinterpret_cast<void**>(image + grab_slots[index]),
                image + grab_targets[index], grab_replacements[index],
                g_grab_hooks[index], error)) continue;
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    const std::array<void*,3> move_replacements{
        reinterpret_cast<void*>(&HookedMoveUpdate),
        reinterpret_cast<void*>(&HookedMoveEnter),
        reinterpret_cast<void*>(&HookedMoveLeave)};
    for (std::size_t index = 0; index < kMoveSlots.size(); ++index) {
        if (hooks::InstallPointerHook(
                reinterpret_cast<void**>(image + kMoveSlots[index]),
                image + kMoveTargets[index], move_replacements[index],
                g_move_state_hooks[index], error)) continue;
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    const std::array<void*,3> push_replacements{
        reinterpret_cast<void*>(&HookedPushUpdate),
        reinterpret_cast<void*>(&HookedPushEnter),
        reinterpret_cast<void*>(&HookedPushLeave)};
    for (std::size_t index = 0; index < kPushSlots.size(); ++index) {
        if (hooks::InstallPointerHook(
                reinterpret_cast<void**>(image + kPushSlots[index]),
                image + kPushTargets[index], push_replacements[index],
                g_push_state_hooks[index], error)) continue;
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    g_original_refraction_copy.store(
        reinterpret_cast<CopyContextToTexture>(
            image + kCopyContextToTextureRva), std::memory_order_release);
    if (!hooks::InstallPointerHook(
            reinterpret_cast<void**>(image + kCopyContextToTextureSlotRva),
            image + kCopyContextToTextureRva,
            reinterpret_cast<void*>(&HookedCopyContextToTexture),
            g_refraction_copy_hook, error)) {
        const std::string install_error = error;
        std::string rollback_error;
        const bool rolled_back = RemoveRenderWorld(rollback_error);
        error = install_error;
        if (!rolled_back) error += "; rollback failed: " + rollback_error;
        return false;
    }
    return true;
}

bool RenderHooksInstalled() noexcept {
    return g_hook.installed() || g_visibility_hook.installed() ||
        g_draw_all_hook.installed() || g_hands_update_hook.installed() ||
        g_refraction_copy_hook.installed() ||
        g_tool_matrix_hook.installed() || g_ray_hook.installed() ||
        std::any_of(g_grab_hooks.begin(), g_grab_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        std::any_of(g_move_state_hooks.begin(), g_move_state_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        std::any_of(g_push_state_hooks.begin(), g_push_state_hooks.end(),
            [](const auto& hook) { return hook.installed(); });
}

bool RemoveRenderWorld(std::string& error) noexcept {
    error.clear();
    if (g_presenting.load(std::memory_order_acquire) ||
        !g_targets_destroyed.load(std::memory_order_acquire)) {
        error = "Requiem presentation still owns OpenGL targets";
        return false;
    }
    if (g_refraction_copy_hook.installed() &&
        !hooks::RemoveIatHook(g_refraction_copy_hook, error)) return false;
    if (g_ray_hook.installed() &&
        !hooks::RemoveIatHook(g_ray_hook, error)) return false;
    for (auto& hook : g_grab_hooks)
        if (hook.installed() && !hooks::RemoveIatHook(hook, error))
            return false;
    for (auto& hook : g_move_state_hooks)
        if (hook.installed() && !hooks::RemoveIatHook(hook, error))
            return false;
    for (auto& hook : g_push_state_hooks)
        if (hook.installed() && !hooks::RemoveIatHook(hook, error))
            return false;
    if (!hooks::RemoveRel32CallHook(g_tool_matrix_hook, error)) return false;
    if (g_hands_update_hook.installed() &&
        !hooks::RemoveIatHook(g_hands_update_hook, error)) return false;
    if (!hooks::RemoveRel32CallHook(g_draw_all_hook, error)) return false;
    if (!hooks::RemoveRel32CallHook(g_hook, error)) return false;
    if (!hooks::RemoveRel32CallHook(g_visibility_hook, error)) return false;
    for (unsigned elapsed = 0; elapsed < 2000; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original.store(nullptr, std::memory_order_release);
            g_original_visibility.store(nullptr, std::memory_order_release);
            g_original_draw_all.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for Requiem RenderWorld callbacks";
    return false;
}

void ConfigurePresentationSettings(runtime::VrSettings settings) noexcept {
    runtime::NormalizeVrSettings(settings);
    AcquireSRWLockExclusive(&g_height_settings_lock);
    g_height_settings = settings;
    ReleaseSRWLockExclusive(&g_height_settings_lock);
}

bool StartPresentation(runtime::OpenVrSession& session,
    std::string& error) noexcept {
    error.clear();
    if (!g_hook.installed() || !g_visibility_hook.installed() ||
        !g_draw_all_hook.installed() ||
        !g_refraction_copy_hook.installed() ||
        !g_hands_update_hook.installed() || !g_tool_matrix_hook.installed() ||
        !g_ray_hook.installed() ||
        !std::all_of(g_grab_hooks.begin(), g_grab_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        !std::all_of(g_move_state_hooks.begin(),
            g_move_state_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        !std::all_of(g_push_state_hooks.begin(),
            g_push_state_hooks.end(),
            [](const auto& hook) { return hook.installed(); }) ||
        !session.initialized()) {
        error = "Requiem render hooks or OpenVR session are unavailable";
        return false;
    }
    if (g_presenting.load(std::memory_order_acquire)) {
        error = "Requiem presentation is already active";
        return false;
    }
    if (!session.ReadEyeConfiguration(g_eyes, error)) return false;
    for (std::size_t index = 0; index < 2; ++index) {
        if (!runtime::BuildHplInfiniteProjection(
                g_eyes[index], kHplNearClip, g_projections[index], error)) {
            return false;
        }
    }
    g_tracking_anchor_valid = false;
    g_tracking_world_yaw.store(0.0F, std::memory_order_release);
    g_world_timing_frames.store(0, std::memory_order_relaxed);
    g_world_render_ticks.store(0, std::memory_order_relaxed);
    g_refraction_native_calls.store(0, std::memory_order_relaxed);
    g_refraction_copy_attempts.store(0, std::memory_order_relaxed);
    g_refraction_resize_attempts.store(0, std::memory_order_relaxed);
    for (auto& ticks : g_eye_ticks) ticks.store(0, std::memory_order_relaxed);
    g_overlay_ticks.store(0, std::memory_order_relaxed);
    g_submit_ticks.store(0, std::memory_order_relaxed);
    g_height_play_mode.Reset();
    ResetTrackedHeadWorldPose();
    AcquireSRWLockExclusive(&g_head_world_pose_lock);
    g_menu_tracking_height = 0.0F;
    g_menu_tracking_height_at = 0;
    ReleaseSRWLockExclusive(&g_head_world_pose_lock);
    AcquireSRWLockExclusive(&g_tracking_world_lock);
    g_world_from_tracking_time = 0;
    g_world_from_tracking_yaw_epoch = 0;
    g_world_from_tracking_identity = {};
    ReleaseSRWLockExclusive(&g_tracking_world_lock);
    g_menu_anchor_valid = false;
    g_recenter_requested.store(false, std::memory_order_release);
    g_world_panel_valid = false;
    g_world_panel_surface = NativeUiSurface::none;
    g_tool_attachments = {};
    g_world_ui_panel_pending.store(false, std::memory_order_release);
    g_overlay_pending = false;
    g_frame_presentation.Reset();
    g_error_logged.store(false, std::memory_order_release);
    g_first_submit_logged.store(false, std::memory_order_release);
    g_first_visibility_logged.store(false, std::memory_order_release);
    g_first_visibility_reuse_logged.store(false, std::memory_order_release);
    g_first_menu_submit_logged.store(false, std::memory_order_release);
    g_presentation_generation.fetch_add(1, std::memory_order_acq_rel);
    g_destroy_requested.store(false, std::memory_order_release);
    g_session.store(&session, std::memory_order_release);
    g_presenting.store(true, std::memory_order_release);
    return true;
}

bool StopPresentation(std::string& error) noexcept {
    error.clear();
    g_presenting.store(false, std::memory_order_release);
    ResetTrackedHeadWorldPose();
    AcquireSRWLockExclusive(&g_tracking_world_lock);
    g_world_from_tracking_time = 0;
    ReleaseSRWLockExclusive(&g_tracking_world_lock);
    g_destroy_requested.store(true, std::memory_order_release);
    for (unsigned elapsed = 0; elapsed < 2000; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0 &&
            g_targets_destroyed.load(std::memory_order_acquire)) {
            g_session.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for Requiem eye targets to close on the GL thread";
    return false;
}

bool TrackedHeadWorldPose(runtime::VrMatrix44& pose) noexcept {
    if (!g_presenting.load(std::memory_order_acquire)) return false;
    std::uint64_t sampled_at_ms = 0;
    bool valid = false;
    AcquireSRWLockShared(&g_head_world_pose_lock);
    pose = g_head_world_pose;
    sampled_at_ms = g_head_world_pose_sampled_at_ms;
    valid = g_head_world_pose_valid;
    ReleaseSRWLockShared(&g_head_world_pose_lock);
    return TrackedHeadWorldPoseFresh(
        valid, sampled_at_ms, GetTickCount64());
}

bool TrackedHeadTrackingHeight(float& height) noexcept {
    if (!g_presenting.load(std::memory_order_acquire)) return false;
    std::uint64_t sampled_at_ms = 0;
    bool valid = false;
    AcquireSRWLockShared(&g_head_world_pose_lock);
    height = g_head_tracking_height;
    sampled_at_ms = g_head_world_pose_sampled_at_ms;
    valid = g_head_world_pose_valid;
    ReleaseSRWLockShared(&g_head_world_pose_lock);
    return TrackedHeadWorldPoseFresh(
        valid, sampled_at_ms, GetTickCount64()) && std::isfinite(height);
}

bool TrackedHeadTrackingHeightForMenu(float& height) noexcept {
    if (!g_presenting.load(std::memory_order_acquire)) return false;
    AcquireSRWLockShared(&g_head_world_pose_lock);
    height = g_menu_tracking_height;
    const auto sampled_at = g_menu_tracking_height_at;
    ReleaseSRWLockShared(&g_head_world_pose_lock);
    const auto now = GetTickCount64();
    return sampled_at != 0 && now >= sampled_at && now - sampled_at <= 500 &&
        std::isfinite(height);
}

RequiemPresentationTiming ConsumePresentationTiming() noexcept {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return {
        g_world_timing_frames.exchange(0, std::memory_order_relaxed),
        g_world_render_ticks.exchange(0, std::memory_order_relaxed),
        g_eye_ticks[0].exchange(0, std::memory_order_relaxed),
        g_eye_ticks[1].exchange(0, std::memory_order_relaxed),
        g_overlay_ticks.exchange(0, std::memory_order_relaxed),
        g_submit_ticks.exchange(0, std::memory_order_relaxed),
        frequency.QuadPart > 0
            ? static_cast<std::uint64_t>(frequency.QuadPart) : 0,
    };
}

RequiemInteractionCounters ConsumeInteractionCounters() noexcept {
    return {
        g_selection_refreshes.exchange(0, std::memory_order_relaxed),
        g_redirected_rays.exchange(0, std::memory_order_relaxed),
        g_ray_hits.exchange(0, std::memory_order_relaxed),
        g_ray_winners.exchange(0, std::memory_order_relaxed),
        g_native_grab_enters.exchange(0, std::memory_order_relaxed),
        g_native_move_enters.exchange(0, std::memory_order_relaxed),
        g_grab_enters.exchange(0, std::memory_order_relaxed),
        g_grabs_acquired.exchange(0, std::memory_order_relaxed),
        g_grabs_released.exchange(0, std::memory_order_relaxed),
        g_moves_acquired.exchange(0, std::memory_order_relaxed),
        g_moves_released.exchange(0, std::memory_order_relaxed),
        g_mechanism_acquired.exchange(0, std::memory_order_relaxed),
        g_tools_attached.exchange(0, std::memory_order_relaxed),
        g_tools_native.exchange(0, std::memory_order_relaxed),
        g_tools_render_aligned.exchange(0, std::memory_order_relaxed),
        g_tools_visibility_aligned.exchange(0, std::memory_order_relaxed),
        g_tools_visibility_resolved_palm.exchange(
            0, std::memory_order_relaxed),
        g_tools_visibility_raw_palm.exchange(
            0, std::memory_order_relaxed),
        g_tools_visibility_palm_gap_over_2cm.exchange(
            0, std::memory_order_relaxed),
        g_native_push_enters.exchange(0, std::memory_order_relaxed),
        g_pushes_acquired.exchange(0, std::memory_order_relaxed),
        g_push_force_ticks.exchange(0, std::memory_order_relaxed),
        g_refraction_native_calls.exchange(0, std::memory_order_relaxed),
        g_refraction_copy_attempts.exchange(0, std::memory_order_relaxed),
        g_refraction_resize_attempts.exchange(0, std::memory_order_relaxed),
    };
}

void OnSdlSwap(std::uint64_t) noexcept {
    if (g_destroy_requested.load(std::memory_order_acquire)) {
        if (g_active_calls.load(std::memory_order_acquire) != 0 ||
            g_targets_destroyed.load(std::memory_order_acquire)) return;
        std::string error;
        if (g_targets.Destroy(error) && g_overlay_targets.Destroy(error)) {
            g_tracking_anchor_valid = false;
            g_height_play_mode.Reset();
            ResetTrackedHeadWorldPose();
            g_menu_anchor_valid = false;
            g_world_panel_valid = false;
            g_world_panel_surface = NativeUiSurface::none;
            g_frame_presentation.Reset();
            g_targets_destroyed.store(true, std::memory_order_release);
        } else {
            LogFailureOnce("eye-target teardown", error);
        }
        return;
    }

    if (!g_presenting.load(std::memory_order_acquire)) return;
    const bool world_presented = g_frame_presentation.ConsumeWorldPresentedAtSwap();
    const bool world_ui_panel = g_world_ui_panel_pending.exchange(
        false, std::memory_order_acq_rel);
    if (world_presented && !world_ui_panel) {
        // The stereo world rendered into eye targets. Present the finished
        // left eye on the desktop before SDL swaps its otherwise stale buffer.
        std::string error;
        if (!graphics::DrawMonitorMirror(
                g_targets.target(graphics::Eye::left).color_texture, error)) {
            LogFailureOnce("gameplay monitor mirror", error);
        }
        g_menu_anchor_valid = runtime::PlanStablePanelAnchor(
            false, false, g_menu_anchor_valid).anchor_valid_after;
        if (CurrentNativeUiSurface() != NativeUiSurface::inventory &&
            CurrentNativeUiSurface() != NativeUiSurface::notebook) {
            AcquireSRWLockExclusive(&g_menu_pointer_lock);
            g_menu_pointer_world_panel = false;
            g_menu_pointer_aspect = 0.0F;
            ReleaseSRWLockExclusive(&g_menu_pointer_lock);
        }
        return;
    }

    auto* session = g_session.load(std::memory_order_acquire);
    if (session == nullptr || wglGetCurrentContext() == nullptr) return;

    std::string error;
    runtime::VrHmdPose pose;
    if (world_ui_panel && g_pending_world_ui_pose_valid) {
        pose = g_pending_world_ui_pose;
    } else if (!session->WaitForHmdPose(pose, error)) {
        LogFailureOnce("menu HMD pose", error);
        return;
    }
    g_pending_world_ui_pose_valid = false;
    if (!pose.device_connected || !pose.pose_valid) {
        LogFailureOnce("menu HMD pose", "The HMD pose is not tracked");
        return;
    }

    AcquireSRWLockExclusive(&g_head_world_pose_lock);
    g_menu_tracking_height = pose.device_to_absolute.values[7];
    g_menu_tracking_height_at = GetTickCount64();
    ReleaseSRWLockExclusive(&g_head_world_pose_lock);

    const bool recenter = g_recenter_requested.exchange(
        false, std::memory_order_acq_rel);
    const auto anchor_plan = runtime::PlanStablePanelAnchor(
        true, recenter, g_menu_anchor_valid);
    if (anchor_plan.capture_current_pose) {
        g_menu_anchor = pose.device_to_absolute;
    }
    g_menu_anchor_valid = anchor_plan.anchor_valid_after;
    std::array<GLint, 4> desktop_viewport{};
    glGetIntegerv(GL_VIEWPORT, desktop_viewport.data());
    AcquireSRWLockExclusive(&g_menu_pointer_lock);
    g_menu_pointer_anchor = g_menu_anchor;
    g_menu_pointer_world_panel = false;
    g_menu_pointer_aspect = desktop_viewport[3] > 0
        ? static_cast<float>(desktop_viewport[2]) /
            static_cast<float>(desktop_viewport[3]) : 0.0F;
    g_menu_pointer_distance = kMenuDistance;
    g_menu_pointer_width = kMenuWidth;
    g_menu_pointer_center_y = kMenuCenterY;
    ReleaseSRWLockExclusive(&g_menu_pointer_lock);

    const auto size = session->recommended_render_target_size();
    if (!g_targets.CreateOrResize(size.width, size.height, error)) {
        LogFailureOnce("menu eye targets", error);
        return;
    }
    g_targets_destroyed.store(false, std::memory_order_release);

    runtime::VrMatrix44 head_view;
    if (!runtime::ComposeYawRecenteredTrackedHeadView(
            runtime::IdentityMatrix(), g_menu_anchor,
            pose.device_to_absolute, 1.0F, head_view, error)) {
        LogFailureOnce("menu head view", error);
        return;
    }

    graphics::OpenGlMenuFrame menu;
    if (!menu.Capture(error)) {
        LogFailureOnce("menu capture", error);
        return;
    }

    bool success = true;
    for (std::size_t index = 0; index < 2 && success; ++index) {
        runtime::VrMatrix44 eye_view;
        success = runtime::ComposeEyeViewFromHeadView(
            head_view, g_eyes[index].eye_to_head, eye_view, error);
        graphics::OpenGlEyeBinding binding;
        if (success) {
            success = g_targets.BeginEye(
                index == 0 ? graphics::Eye::left : graphics::Eye::right,
                binding, error);
        }
        if (success) {
            success = menu.Draw(eye_view, g_projections[index],
                kMenuDistance, kMenuWidth, kMenuCenterY, error);
        }
        if (binding.active) {
            std::string restore_error;
            if (!g_targets.EndEye(binding, restore_error)) {
                success = false;
                error = "Could not restore Requiem menu eye framebuffer: " +
                    restore_error;
            }
        }
    }
    if (!success) {
        LogFailureOnce("menu eye render", error);
        return;
    }

    const std::array<std::uint32_t, 2> textures{
        g_targets.target(graphics::Eye::left).color_texture,
        g_targets.target(graphics::Eye::right).color_texture};
    if (!session->SubmitOpenGlEyeTextures(textures, error)) {
        LogFailureOnce("menu submit", error);
        return;
    }
    glFlush();
    if (!g_first_menu_submit_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem first tracked menu frame submitted (%lu x %lu)",
            static_cast<unsigned long>(size.width),
            static_cast<unsigned long>(size.height));
    }
}

void RequestTrackedRecenter() noexcept {
    g_recenter_requested.store(true, std::memory_order_release);
}

void AddTrackedWorldYaw(float radians) noexcept {
    if (!std::isfinite(radians)) return;
    const float previous = g_tracking_world_yaw.load(std::memory_order_acquire);
    g_tracking_world_yaw.store(std::remainder(previous + radians,
        6.28318530717958647692F), std::memory_order_release);
    g_world_yaw_epoch.fetch_add(1, std::memory_order_acq_rel);
}

void RefreshVrSelectionBeforeInteract(void* player) noexcept {
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    g_vr_selection_contact = {};
    if (player == nullptr || g_image == nullptr ||
        !g_presenting.load(std::memory_order_acquire) ||
        !g_ray_hook.installed() || NativeUiActive() ||
        ReadNative<int>(player, 0x2C0) != 0) return;
    auto* states = ReadNative<void*>(player, 0x2C8);
    auto* normal = ReadNative<void*>(states, 0);
    if (normal == nullptr ||
        ReadNative<void*>(normal, 0) !=
            g_image + kNormalVtableRva ||
        ReadNative<void*>(normal, 0x10) != player) return;
    g_selection_refresh_active = true;
    g_refresh_entity_hits = 0;
    g_refresh_winner_distance = -1.0F;
    g_refresh_winner_mass = -1.0F;
    g_selection_refreshes.fetch_add(1, std::memory_order_relaxed);
    reinterpret_cast<HandsUpdate>(g_image + kNormalUpdateRva)(normal, 0.0F);
    g_selection_refresh_active = false;
    if (g_vr_selection_ready) g_vr_selection_player = player;
    // The ray winner is only a candidate. Requiem's pick callback copies it
    // into pick+4 after its own eligibility/range checks; OnStartInteract
    // consumes that native field, not our ranked-ray result.
    void* const pick = ReadNative<void*>(player, 0x280);
    void* const accepted_body = ReadNative<void*>(pick, 4);
    void* const accepted_entity = ReadNative<void*>(accepted_body, 0x414);
    probe::WriteLog(
        "Requiem VR select press entity_hits=%lu winner=%u accepted=%u picked=%u type=%ld distance=%.3f picked_distance=%.3f mass=%.3f entity_reach=%.3f",
        static_cast<unsigned long>(g_refresh_entity_hits),
        g_vr_selection_ready ? 1U : 0U,
        g_vr_selection_ready && accepted_body != nullptr &&
            accepted_body == g_vr_selection_contact.body ? 1U : 0U,
        accepted_body != nullptr ? 1U : 0U,
        accepted_entity != nullptr
            ? static_cast<long>(ReadNative<int>(accepted_entity, 0xC0)) : -1L,
        g_refresh_winner_distance,
        pick != nullptr ? ReadNative<float>(pick, 0x10) : -1.0F,
        g_refresh_winner_mass,
        accepted_entity != nullptr
            ? ReadNative<float>(accepted_entity, 0xBC) : -1.0F);
}

bool TrackedMenuPointer(const runtime::VrHmdPose& pointer_pose,
    std::array<float, 2>& uv) noexcept {
    uv = {};
    if (!pointer_pose.pose_valid || !pointer_pose.device_connected) return false;
    AcquireSRWLockShared(&g_menu_pointer_lock);
    const auto anchor = g_menu_pointer_anchor;
    const float aspect = g_menu_pointer_aspect;
    const bool world_panel = g_menu_pointer_world_panel;
    const auto world_from_tracking = g_menu_pointer_world_from_tracking;
    const auto panel_pose = g_menu_pointer_world_pose;
    const float distance = g_menu_pointer_distance;
    const float width = g_menu_pointer_width;
    const float center_y = g_menu_pointer_center_y;
    ReleaseSRWLockShared(&g_menu_pointer_lock);
    if (aspect <= 0.0F) return false;
    if (world_panel) return runtime::ProjectAimOnWorldPanel(
        world_from_tracking, panel_pose,
        pointer_pose.device_to_absolute, aspect,
        distance, width, uv, center_y);
    return runtime::ProjectAimOnMenu(
        anchor, pointer_pose.device_to_absolute, aspect,
        distance, width, uv, center_y);
}

} // namespace penumbra_vr::backends::requiem
