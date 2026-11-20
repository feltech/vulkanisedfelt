// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_configuration.hpp
 *
 * Pure selection of supported swapchain configurations.
 */
#pragma once

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./swapchain_configuration_types.hpp"
#include "./swapchain_image_count_types.hpp"
#include "./swapchain_support_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Selects supported choices for an ordinary per-image swapchain.
 *
 * Surface limits must describe valid ordered intervals. An unavailable choice remains an
 * ordinary empty optional, without assigning an error or cancellation policy.
 *
 * @param support Queried surface facts.
 * @param drawable_extent Requested drawable size in pixels.
 * @param acceptable_formats Image-format allowlist, preserving advertised pair order.
 * @param preferred_present_modes Ordinary per-image presentation modes in preference order.
 * @param preferred_image_count Desired image count, defaulting to double buffering.
 * @return Supported configuration, or no value when a format, mode or drawable area is unavailable.
 */
[[nodiscard]] auto select_swapchain_configuration(
    SwapchainSupport const & support,
    VkExtent2D drawable_extent,
    immer::array<VkFormat> const & acceptable_formats =
        {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM},
    immer::array<VkPresentModeKHR> const & preferred_present_modes =
        {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR},
    swapchain_image_count preferred_image_count = swapchain_image_count{2})
    -> std::optional<SwapchainConfiguration>;
}  // namespace vulkandemo::vulkan
