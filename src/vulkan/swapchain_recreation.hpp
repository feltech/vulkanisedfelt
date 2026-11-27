// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_recreation.hpp
 *
 * Pure swapchain recreation triggers and stateful generation-tracked replacement.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/state_io.hpp>
#include <fnfelt/result.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./errors.hpp"
#include "./presentation_device_types.hpp"
#include "./submit_and_present_types.hpp"
#include "./swapchain_generation_types.hpp"
#include "detail/presentation_swapchain.hpp"
#include "detail/swapchain_recreation.hpp"

namespace vulkandemo::vulkan
{
/**
 * Whether a Vulkan failure indicates an out-of-date swapchain needing recreation.
 *
 * @param failure Native Vulkan failure.
 * @return True when @p failure is `VK_ERROR_OUT_OF_DATE_KHR` from acquisition, presentation or
 * swapchain creation.
 */
[[nodiscard]] constexpr auto is_swapchain_out_of_date(VulkanFailure failure) noexcept -> bool
{
    return failure.result == VK_ERROR_OUT_OF_DATE_KHR &&
        (failure.operation == FallibleVulkanOperation::acquire_next_image ||
         failure.operation == FallibleVulkanOperation::queue_present ||
         failure.operation == FallibleVulkanOperation::create_swapchain);
}

/**
 * Whether a completed or failed bounded frame requires swapchain recreation before the next
 * frame.
 *
 * @param outcome Bounded frame presentation outcome.
 * @return True for suboptimal completion or out-of-date acquisition/presentation failures.
 */
[[nodiscard]] constexpr auto should_recreate_swapchain(
    completed_presentation_result const & outcome) noexcept -> bool
{
    namespace result = fnfelt::result;
    if (result::is_success(outcome))
    {
        return result::get_success(outcome).is_suboptimal;
    }
    if (result::is_error(outcome))
    {
        return is_swapchain_out_of_date(result::get_error(outcome));
    }
    return false;
}

/**
 * Plans initial creation or replacement of live presentation targets with generation tracking.
 *
 * Reusable across initial creation, resize, minimize/un-minimize transitions and post-failure
 * retries. Before executing recreation, complete all GPU work using any currently live targets (as
 * established by `reader_io_clear_and_present`) and serialize host access to the surface and
 * swapchain.
 *
 * Pre-creation failures and zero drawable extents (`unavailable_extent`) do not invoke
 * `vkCreateSwapchainKHR` and leave any existing live swapchain unretired; zero extents succeed
 * with `SwapchainMinimized`. Once native `vkCreateSwapchainKHR` runs, any previous live targets
 * are retired unconditionally in state (`latest_generation` increments and `live` becomes empty),
 * so a failure at or after creation preserves empty live targets and allows a subsequent call to
 * recreate cleanly from `VK_NULL_HANDLE`. When no external snapshot retains the prior targets,
 * old framebuffers and views destroy before `vkCreateSwapchainKHR` while the prior swapchain is
 * retained through the call and destroyed immediately after; format-unchanged replacements reuse
 * the prior render pass without recreating it.
 *
 * @tparam TState Persistent state type satisfying `detail::recreate_presentation_targets_state`.
 * @param presentation_device Retained device, surface and selected presentation queues.
 * @param window Nonempty Vulkan-capable window associated with that surface.
 * @return Reusable StateIO yielding `SwapchainRecreated`, `SwapchainMinimized`, stop, or error.
 */
template <detail::recreate_presentation_targets_state TState>
[[nodiscard]] constexpr auto state_io_recreate_presentation_targets(
    PresentationDevice presentation_device, sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO providing validated surface, pixel and physical image-limit choices.
    auto prepared = state_io::lift<"prepare_presentation_swapchain"_ss, TState>(
        detail::reader_io_prepare_presentation_swapchain<TState>(
            presentation_device, std::move(window)));

    // Continuation retiring any live swapchain once before assembling replacement targets.
    auto replace = std::bind_front(
        detail::state_io_replace_prepared_targets<TState>, std::move(presentation_device));

    // StateIO providing recreated target summaries or the first operational failure.
    auto replaced = state_io::and_then<"replace_presentation_targets"_ss>(
        std::move(prepared), std::move(replace));

    // StateIO mapping zero drawable extents to SwapchainMinimized without retiring live targets.
    return state_io::or_else<"handle_minimized_swapchain"_ss>(
        std::move(replaced), detail::state_io_recover_minimized_swapchain<TState>);
}

/**
 * Plans a generation guard that resolves current targets only when @p expected is still live.
 *
 * @tparam TState Persistent state type satisfying `detail::swapchain_targets_state`.
 * @param expected Swapchain target generation required by subsequent operations.
 * @return StateIO providing the live targets or `StaleSwapchainGeneration`.
 */
template <detail::swapchain_targets_state TState>
[[nodiscard]] constexpr auto state_io_require_live_targets(swapchain_generation expected)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // Action validating that the persistent state's active targets still belong to expected.
    auto check_live =
        std::bind_front(detail::require_live_targets_state_io_action<TState>, expected);

    // StateIO guarding generation-bound consumers without modifying persistent target state.
    return state_io::create<"require_live_targets"_ss, TState>(std::move(check_live));
}
}  // namespace vulkandemo::vulkan
