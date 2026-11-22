// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/quit_aware_frame.hpp
 *
 * Bounded clear-and-present with an SDL quit checkpoint.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/pending_events.hpp"
#include "../sdl/pending_events_types.hpp"
#include "../sdl/window_id_types.hpp"
#include "./synchronization_types.hpp"
#include "detail/quit_aware_frame.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a quit checkpoint before one bounded clear frame.
 *
 * The selected window ID must identify the targets' window. Global quit or its close request
 * produces `result::stopped` before any new frame effects; already-owned resources still receive
 * RAII cleanup. Polling consumes at most @p limit events, leaving the remainder queued, so a later
 * quit request is considered on a subsequent execution. Successful output preserves the batch for
 * further handling; stopped and error outcomes have no event payload.
 *
 * The environment owns targets and a command pool satisfying reader_io_clear_and_present's
 * ownership, queue serialization and failure-abandonment requirements. These resources remain
 * alive throughout polling and frame execution. Keep the API and serviced main context alive
 * through final resource release, and keep the API alive through any returned event-text owners.
 *
 * @tparam TEnvironment Execution capabilities and owned rendering resources.
 * @param window_id Selected live window identifier.
 * @param clear_color Floating-point clear color.
 * @param acquisition_timeout Image-acquisition timeout, excluding completion waits.
 * @param limit Nonzero event-collection bound.
 * @return ReaderIO providing completion and events, `result::stopped`, or the original Vulkan
 * failure.
 */
template <detail::quit_aware_frame_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_clear_and_present_unless_quit(
    sdl::window_id window_id,
    VkClearColorValue clear_color,
    timeout_nanoseconds acquisition_timeout,
    sdl::event_batch_limit limit)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    // GCC rejects preconditions on this deduced-return factory.
    contract_assert(value_of(limit) != 0 && value_of(window_id) != 0);

    // ReaderIO providing a bounded owned batch from the main thread.
    auto pending_events = sdl::reader_io_sdl_collect_pending_events<TEnvironment>(limit);

    // Checkpoint providing the batch only when no quit request cancels this frame.
    auto check_quit = std::bind_front(detail::stop_if_quit_requested, window_id);
    auto checked_events = reader_io::transform<"quit_before_frame"_ss>(
        std::move(pending_events), std::move(check_quit));

    // Continuation providing a bounded frame and pairing its completion with the collected events.
    auto present_and_retain_events = std::bind_front(
        detail::reader_io_present_and_retain_events<TEnvironment>,
        clear_color,
        acquisition_timeout);
    return reader_io::and_then<"present_unless_quit"_ss>(
        std::move(checked_events), std::move(present_and_retain_events));
}
}  // namespace vulkandemo::vulkan
