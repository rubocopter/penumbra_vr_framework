#include "penumbra_vr/black_plague_probe_lifecycle.hpp"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

using penumbra_vr::BlackPlagueProbeCapability;
using penumbra_vr::BlackPlagueProbeCapabilityMask;
using penumbra_vr::BlackPlagueProbeLifecycle;

constexpr std::uint32_t AllCapabilities() noexcept {
    std::uint32_t result = 0;
    for (const auto capability :
         penumbra_vr::kBlackPlagueProbeTeardownOrder) {
        result |= BlackPlagueProbeCapabilityMask(capability);
    }
    return result;
}

[[nodiscard]] bool TestCallbackAndReadyPolicy() {
    constexpr std::array states{
        BlackPlagueProbeLifecycle::clean,
        BlackPlagueProbeLifecycle::initializing,
        BlackPlagueProbeLifecycle::ready,
        BlackPlagueProbeLifecycle::shutting_down,
        BlackPlagueProbeLifecycle::partial,
    };
    for (const auto state : states) {
        if (penumbra_vr::BlackPlagueProbeCallbacksAllowed(state) !=
            (state == BlackPlagueProbeLifecycle::ready)) {
            return false;
        }
        if (penumbra_vr::BlackPlagueProbeCanBeginShutdown(state) !=
            (state == BlackPlagueProbeLifecycle::ready ||
             state == BlackPlagueProbeLifecycle::partial)) {
            return false;
        }
    }

    const auto required = penumbra_vr::kBlackPlagueProbeRequiredCapabilities;
    return penumbra_vr::BlackPlagueProbeRequiredSetInstalled(required) &&
        penumbra_vr::BlackPlagueProbeRequiredSetInstalled(AllCapabilities()) &&
        !penumbra_vr::BlackPlagueProbeRequiredSetInstalled(
            required & ~BlackPlagueProbeCapabilityMask(
                BlackPlagueProbeCapability::render_world));
}

[[nodiscard]] bool TestGraphicsBootstrapReadiness() {
    // HPL1 performs one SDL_GL_SwapBuffers call inside graphics Init. Deep
    // OpenGL/native hooks must wait for the next completed swap, which is the
    // first one reached from the normal Game::Run loop after GameInit returns.
    return !penumbra_vr::BlackPlagueDeepHookBootstrapReady(0) &&
        !penumbra_vr::BlackPlagueDeepHookBootstrapReady(1) &&
        penumbra_vr::BlackPlagueDeepHookBootstrapReady(2) &&
        penumbra_vr::BlackPlagueDeepHookBootstrapReady(3);
}

[[nodiscard]] bool TestEveryTeardownFailureCanRetry() {
    for (std::size_t failed_index = 0;
         failed_index < penumbra_vr::kBlackPlagueProbeTeardownOrder.size();
         ++failed_index) {
        std::uint32_t remaining = AllCapabilities();
        std::uint32_t cleanup_ledger = AllCapabilities();
        for (std::size_t index = 0; index < failed_index; ++index) {
            const auto bit = BlackPlagueProbeCapabilityMask(
                penumbra_vr::kBlackPlagueProbeTeardownOrder[index]);
            remaining &= ~bit;
            cleanup_ledger &= ~bit;
        }

        // A failed remover keeps its bit and every dependency below it. The
        // lifecycle must remain partial and permit another shutdown attempt.
        if (penumbra_vr::BlackPlagueProbeStateAfterTeardown(
                remaining, cleanup_ledger) !=
                BlackPlagueProbeLifecycle::partial ||
            !penumbra_vr::BlackPlagueProbeCanBeginShutdown(
                BlackPlagueProbeLifecycle::partial)) {
            return false;
        }

        // Retry starts at the first remaining component and durably clears all
        // later bits in the same shared reverse dependency order.
        for (std::size_t index = failed_index;
             index < penumbra_vr::kBlackPlagueProbeTeardownOrder.size();
             ++index) {
            const auto bit = BlackPlagueProbeCapabilityMask(
                penumbra_vr::kBlackPlagueProbeTeardownOrder[index]);
            if ((remaining & bit) == 0 || (cleanup_ledger & bit) == 0) {
                return false;
            }
            remaining &= ~bit;
            cleanup_ledger &= ~bit;
        }
        if (penumbra_vr::BlackPlagueProbeStateAfterTeardown(
                remaining, cleanup_ledger) !=
            BlackPlagueProbeLifecycle::clean) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool TestEveryStartupInstallFailure() {
    const auto& install_order = penumbra_vr::kBlackPlagueProbeInstallOrder;
    const auto& teardown_order = penumbra_vr::kBlackPlagueProbeTeardownOrder;
    for (std::size_t failed_index = 0;
         failed_index < install_order.size(); ++failed_index) {
        std::uint32_t capabilities = 0;
        std::uint32_t cleanup_ledger = 0;
        for (std::size_t index = 0; index < failed_index; ++index) {
            const auto bit = BlackPlagueProbeCapabilityMask(
                install_order[index]);
            capabilities |= bit;
            cleanup_ledger |= bit;
        }

        // InstallTrackedComponent marks cleanup before calling the installer.
        const auto failed_bit = BlackPlagueProbeCapabilityMask(
            install_order[failed_index]);
        cleanup_ledger |= failed_bit;

        // If the failed component cleans itself successfully, the subsequent
        // global rollback must unwind every earlier install to a clean state.
        auto clean_capabilities = capabilities;
        auto clean_ledger = cleanup_ledger & ~failed_bit;
        for (const auto capability : teardown_order) {
            const auto bit = BlackPlagueProbeCapabilityMask(capability);
            if ((clean_ledger & bit) == 0) continue;
            clean_capabilities &= ~bit;
            clean_ledger &= ~bit;
        }
        if (penumbra_vr::BlackPlagueProbeStateAfterTeardown(
                clean_capabilities, clean_ledger) !=
            BlackPlagueProbeLifecycle::clean) {
            return false;
        }

        // If that immediate cleanup fails, capability publication can still be
        // zero (especially for the first component), but the durable cleanup
        // bit means lifecycle is partial and shutdown must be retryable.
        if (penumbra_vr::BlackPlagueProbeStateAfterTeardown(
                capabilities, cleanup_ledger) !=
                BlackPlagueProbeLifecycle::partial ||
            !penumbra_vr::BlackPlagueProbeCanBeginShutdown(
                BlackPlagueProbeLifecycle::partial)) {
            return false;
        }
        for (const auto capability : teardown_order) {
            const auto bit = BlackPlagueProbeCapabilityMask(capability);
            if ((cleanup_ledger & bit) == 0) continue;
            capabilities &= ~bit;
            cleanup_ledger &= ~bit;
        }
        if (penumbra_vr::BlackPlagueProbeStateAfterTeardown(
                capabilities, cleanup_ledger) !=
            BlackPlagueProbeLifecycle::clean) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    if (!TestCallbackAndReadyPolicy()) {
        std::cerr << "probe callback/readiness lifecycle policy failed\n";
        return 1;
    }
    if (!TestGraphicsBootstrapReadiness()) {
        std::cerr << "probe deep-hook graphics bootstrap policy failed\n";
        return 2;
    }
    if (!TestEveryTeardownFailureCanRetry()) {
        std::cerr << "probe teardown retry ledger failed\n";
        return 3;
    }
    if (!TestEveryStartupInstallFailure()) {
        std::cerr << "probe startup install/rollback ledger failed\n";
        return 4;
    }
    std::cout << "probe partial lifecycle ledger passed\n";
    return 0;
}
