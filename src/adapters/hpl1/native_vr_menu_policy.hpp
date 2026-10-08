#pragma once

namespace penumbra_vr::adapters::hpl1 {

inline bool NativeVrMenuRootBindingMatches(
    const void* root, const void* live_widget, const void* vtable,
    const void* native_button_vtable, int target_state) noexcept {
    return root != nullptr && root == live_widget &&
        native_button_vtable != nullptr && vtable == native_button_vtable &&
        target_state == 0x70000000;
}

inline bool NativeVrMenuNeedsRebindOnStateTransition(
    int next_state, bool has_root, bool same_owner) noexcept {
    return next_state == 0 && (!has_root || !same_owner);
}

} // namespace penumbra_vr::adapters::hpl1
