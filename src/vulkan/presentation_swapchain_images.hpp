// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_swapchain_images.hpp
 *
 * Presentation swapchain setup with retained swapchain images.
 */
#pragma once

#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_swapchain.hpp"
#include "detail/presentation_swapchain_images.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans swapchain creation and retrieval of its presentable images, optionally replacing a prior
 * swapchain.
 *
 * The window must match the device's surface. Creation follows the same requirements as
 * reader_io_create_presentation_swapchain, including optional @p old_swapchain retirement. Returned
 * image handles borrow their lifetime from the returned presentation context; keep that owner
 * alive during image use. Retrieval does not acquire an image for rendering or submit GPU work. The
 * borrowed API and serviced main context must outlive all plans and owners.
 *
 * @param presentation_device Retained presentation device, surface and queues.
 * @param window Nonempty Vulkan-capable window associated with that surface.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @return Reusable ReaderIO providing images and ownership, stop, or renderer error.
 */
template <detail::create_presentation_swapchain_images_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_swapchain_images(
    PresentationDevice presentation_device,
    sdl::window_ptr window,
    swapchain_ptr old_swapchain = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the acquired swapchain and its selected presentation configuration.
    auto create_swapchain = reader_io_create_presentation_swapchain<TEnvironment>(
        std::move(presentation_device), std::move(window), std::move(old_swapchain));

    // ReaderIO providing borrowed images with all ownership required throughout their use.
    return reader_io::and_then<"get_presentation_swapchain_images"_ss>(
        std::move(create_swapchain),
        detail::reader_io_get_presentation_swapchain_images<TEnvironment>);
}
}  // namespace vulkandemo::vulkan
