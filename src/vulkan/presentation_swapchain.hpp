// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_swapchain.hpp
 *
 * Live swapchain setup and replacement for a presentation device.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "detail/presentation_swapchain.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a presentation swapchain using live drawable pixels, optionally retiring a predecessor.
 *
 * The window must be the Vulkan-capable window associated with the device's surface. A nonempty
 * @p old_swapchain must be non-retired for that device and surface; otherwise the window must have
 * no existing swapchain. Pre-creation failures such as zero drawable extents (`unavailable_extent`)
 * do not invoke `vkCreateSwapchainKHR` and do not retire @p old_swapchain. Once native creation
 * runs, @p old_swapchain is retired unconditionally even if creation fails; never replay a
 * replacement plan with that retired handle. Default format, presentation and image-count policies
 * select ordinary per-image presentation. Unavailable extents, unsupported policy and image limits
 * report a renderer error; cancellation remains distinct. Synchronize host access to the surface
 * throughout execution. The borrowed API and serviced main context must outlive plans and owners;
 * complete GPU work and child cleanup before final release.
 *
 * @param presentation_device Retained device, surface and selected presentation queues.
 * @param window Nonempty window owner associated with that surface.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @return Reusable ReaderIO providing an owning presentation swapchain, stop, or renderer error.
 */
template <detail::create_presentation_swapchain_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_swapchain(
    PresentationDevice presentation_device,
    sdl::window_ptr window,
    swapchain_ptr old_swapchain = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing validated surface, pixel and physical image-limit choices.
    auto prepared = detail::reader_io_prepare_presentation_swapchain<TEnvironment>(
        presentation_device, std::move(window));

    // Continuation providing owned swapchain resources with optional predecessor handoff.
    auto create_swapchain = std::bind_front(
        detail::reader_io_create_prepared_swapchain<TEnvironment>,
        std::move(presentation_device),
        std::move(old_swapchain));

    // ReaderIO providing owned presentation resources with optional predecessor handoff.
    return reader_io::and_then<"create_presentation_swapchain"_ss>(
        std::move(prepared), std::move(create_swapchain));
}
}  // namespace vulkandemo::vulkan
