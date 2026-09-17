#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <cstdint>
namespace penumbra_vr::backends::black_plague {
struct SpatialDiagnostics {
    std::uint64_t tools_attached=0, tools_native=0, invalid_tool_pose=0, blocked_grabs=0;
    std::uint64_t grabs_acquired=0, grabs_released=0;
    std::uint64_t moves_acquired=0, moves_released=0;
    std::uint64_t guarded_releases=0, collision_restore_failures=0;
    std::uint64_t contact_rays=0;
    std::uint64_t nudge_queries=0, nudge_contacts=0, nudges_applied=0;
    std::uint64_t interact_presses=0, selection_refreshes=0;
    std::uint64_t selection_ray_batches=0, selection_rays=0;
    std::uint64_t selection_candidates=0, selection_discards=0;
    std::uint64_t selection_winner_distance_millimetres=0;
    std::uint64_t selection_central_ray=0, selection_auxiliary_ray=0;
    std::uint64_t grab_enters=0, move_enters=0;
    std::uint64_t grab_pending=0, move_pending=0;
    std::uint64_t magnetic_queries=0, magnetic_candidates=0;
    std::uint64_t magnetic_visibility_rays=0, magnetic_winners=0;
    std::uint64_t mechanism_acquired=0, mechanism_updates=0, mechanism_rejected=0;
};
[[nodiscard]] SpatialDiagnostics ConsumeSpatialDiagnostics() noexcept;
[[nodiscard]] bool InstallSpatialInteraction(std::string& error) noexcept;
[[nodiscard]] bool RemoveSpatialInteraction(std::string& error) noexcept;
// Called only by ButtonHandler on the native game thread, never by IPC.
void RefreshVrSelectionBeforeInteract(void* player) noexcept;
void ServiceSpatialInteraction(void* player, bool ui) noexcept;
// Runs from the native character-body update owner on the game thread.
void ServiceSpatialHandNudge(void* character_body) noexcept;
// Renderer-side presentation hint published by the game-thread tool owner.
// A stale value expires automatically when the attachment is unequipped.
[[nodiscard]] bool ReadAttachedToolGrip(
    std::size_t hand_index,
    float& pose_weight) noexcept;
// Latest ordinary physical pick target for Rework's bounded collision/contact
// assistance. Returns false for stale/no target; inventory magnetism is a
// separate policy and is intentionally not synthesized here.
[[nodiscard]] bool ReadSpatialInteractionTarget(
    std::size_t hand_index,
    std::array<float,3>& world_point) noexcept;
}
