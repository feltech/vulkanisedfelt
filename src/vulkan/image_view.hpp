// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/image_view.hpp
 *
 * Deferred creation of swapchain image views.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/image_view.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of a 2D color view for one swapchain image.
 *
 * The nonempty device and swapchain must own the image. Its selected format must support
 * a 2D color view with one mip and one layer. Keep the borrowed API alive through cleanup,
 * finish GPU work and synchronize host access before releasing the final view owner.
 *
 * @param device Retained logical device owning the image.
 * @param swapchain Retained swapchain owning the image.
 * @param image Borrowed swapchain image.
 * @param format Selected swapchain image format.
 * @return Reusable ReaderIO providing an owned image view or the original Vulkan failure.
 */
template <detail::vk_create_image_view_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_image_view(
    device_ptr device, swapchain_ptr swapchain, VkImage image, VkFormat format)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing image view creation with retained native parents.
    auto create_image_view = std::bind_front(
        detail::create_image_view_reader_io_action<TEnvironment>,
        std::move(device),
        std::move(swapchain),
        image,
        format);

    // ReaderIO providing a view whose owner preserves the viewed image's lifetime.
    return reader_io::create<"vk_create_image_view"_ss, TEnvironment>(std::move(create_image_view));
}
}  // namespace vulkandemo::vulkan
