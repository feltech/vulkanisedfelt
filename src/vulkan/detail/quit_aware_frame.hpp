// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/quit_aware_frame.hpp
 *
 * Scoped event polling and a pre-frame quit checkpoint.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../../sdl/detail/pending_events.hpp"
#include "../../sdl/event_types.hpp"
#include "../../sdl/window_id_types.hpp"
#include "../clear_frame.hpp"
#include "../errors.hpp"
#include "../quit_aware_frame_types.hpp"
#include "../submit_and_present_types.hpp"
#include "../synchronization_types.hpp"
#include "./clear_frame.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed for event collection followed by a bounded clear frame. */
template <class TEnvironment>
concept quit_aware_frame_environment = clear_and_present_environment<TEnvironment> &&
    sdl::detail::sdl_collect_pending_events_environment<TEnvironment>;

/**
 * Cancels a frame when its collected input requests application exit.
 *
 * @param window_id Selected window identifier.
 * @param events Collected input in polling order.
 * @return Unchanged input batch or stopped for global quit or the selected window's close.
 */
auto stop_if_quit_requested(sdl::window_id window_id, immer::array<sdl::event> events)
    -> vulkan_result<immer::array<sdl::event>>;

/**
 * Pairs a completed presentation with previously collected input events.
 *
 * @param events Previously collected input batch.
 * @param completion Completed presentation summary.
 * @return Frame completion paired with its input batch.
 */
auto make_quit_aware_frame(immer::array<sdl::event> events, CompletedPresentation completion)
    -> QuitAwareFrame;

/**
 * Plans a bounded frame and pairs its completion with previously collected events.
 *
 * @param clear_color Floating-point clear color.
 * @param acquisition_timeout Image-acquisition timeout.
 * @param events Previously collected input batch to preserve on success.
 * @return ReaderIO providing completed presentation paired with @p events.
 */
template <class TEnvironment>
constexpr auto reader_io_present_and_retain_events(
    VkClearColorValue clear_color,
    timeout_nanoseconds acquisition_timeout,
    immer::array<sdl::event> events)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the frame's completion and original Vulkan error channels.
    auto presentation = reader_io_clear_and_present<TEnvironment>(clear_color, acquisition_timeout);

    // Transformer pairing successful completion with the already-collected events.
    auto retain_events = std::bind_front(make_quit_aware_frame, std::move(events));
    return reader_io::transform<"retain_frame_events"_ss>(
        std::move(presentation), std::move(retain_events));
}
}  // namespace vulkandemo::vulkan::detail
