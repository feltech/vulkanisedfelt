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

#include <fnfelt/monad/state_io.hpp>

#include "./errors.hpp"
#include "./submit_and_present_types.hpp"
#include "./swapchain_generation_types.hpp"
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
[[nodiscard]] auto is_swapchain_out_of_date(VulkanFailure failure) noexcept -> bool;

/**
 * Whether a completed or failed bounded frame requires swapchain recreation before the next
 * frame.
 *
 * @param outcome Bounded frame presentation outcome.
 * @return True for suboptimal completion or out-of-date acquisition/presentation failures.
 */
[[nodiscard]] auto should_recreate_swapchain(completed_presentation_result const & outcome) noexcept
    -> bool;

/**
 * Plans initial creation or replacement of live presentation framebuffers from persistent state.
 *
 * Reads the retained `presentation_device` and `window` from the boxed resource state, so reuse
 * works cleanly across initial creation, resize, minimize/un-minimize transitions and post-failure
 * retries. Before executing recreation, complete all GPU work using any currently live
 * framebuffers (as established by `reader_io_clear_and_present`) and serialize host access to the
 * surface and swapchain.
 *
 * Pre-creation failures and zero drawable extents (`unavailable_extent`) do not invoke
 * `vkCreateSwapchainKHR` and leave any existing live swapchain unretired; zero extents succeed
 * with `SwapchainMinimized`. Once native `vkCreateSwapchainKHR` runs, any previous live
 * framebuffers are retired unconditionally in state (`latest_generation` increments and `live`
 * becomes empty), so a failure at or after creation preserves empty live state and allows a
 * subsequent call to recreate cleanly from `VK_NULL_HANDLE`. When no external snapshot retains
 * the prior bundle, old framebuffers and views destroy before `vkCreateSwapchainKHR` while the
 * prior swapchain is retained through the call and destroyed immediately after; format-unchanged
 * replacements reuse the prior render pass without recreating it.
 *
 * @tparam TState Persistent state type satisfying
 * `detail::recreate_presentation_framebuffers_state`.
 * @return Reusable StateIO yielding `SwapchainRecreated`, `SwapchainMinimized`, stop, or error.
 */
template <detail::recreate_presentation_framebuffers_state TState>
[[nodiscard]] constexpr auto state_io_recreate_presentation_framebuffers()
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO extracting long-lived device and window handles from boxed resource state.
    auto context = state_io::create<"read_recreation_context"_ss, TState>(
        detail::read_recreation_context_state_io_action<TState>);

    // StateIO providing validated surface, pixel and physical image-limit choices.
    auto prepared = state_io::and_then<"prepare_presentation_swapchain"_ss>(
        std::move(context), detail::state_io_prepare_swapchain_from_context<TState>);

    // StateIO retiring any live swapchain once before assembling replacement framebuffers.
    auto replaced = state_io::and_then<"replace_presentation_framebuffers"_ss>(
        std::move(prepared), detail::state_io_replace_prepared_framebuffers<TState>);

    // StateIO mapping zero drawable extents to SwapchainMinimized without retiring live state.
    return state_io::or_else<"handle_minimized_swapchain"_ss>(
        std::move(replaced), detail::state_io_recover_minimized_swapchain<TState>);
}

/**
 * Plans a generation guard that resolves current framebuffers only when @p expected is still live.
 *
 * @tparam TState Persistent state type satisfying `detail::swapchain_framebuffers_state`.
 * @param expected Swapchain framebuffer generation required by subsequent operations.
 * @return StateIO providing the live framebuffers or `StaleSwapchainGeneration`.
 */
template <detail::swapchain_framebuffers_state TState>
[[nodiscard]] constexpr auto state_io_require_live_framebuffers(swapchain_generation expected)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // Action validating that the persistent state's active framebuffers still belong to expected.
    auto check_live =
        std::bind_front(detail::require_live_framebuffers_state_io_action<TState>, expected);

    // StateIO guarding generation-bound consumers without modifying persistent state.
    return state_io::create<"require_live_framebuffers"_ss, TState>(std::move(check_live));
}
}  // namespace vulkandemo::vulkan
