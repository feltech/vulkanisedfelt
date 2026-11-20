// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_image_count_selection.hpp
 *
 * Pure selection of swapchain image counts.
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include <vulkan/vulkan_core.h>

#include "./swapchain_image_count_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Clamps the preferred image count to the surface's supported interval.
 *
 * A zero maximum leaves the count unbounded above.
 *
 * @param capabilities Surface limits with a positive minimum and a consistent maximum.
 * @param preferred_count Desired image count, defaulting to double buffering.
 * @return Supported requested image count.
 */
[[nodiscard]] constexpr auto select_swapchain_image_count(
    VkSurfaceCapabilitiesKHR capabilities,
    swapchain_image_count preferred_count = swapchain_image_count{2}) -> swapchain_image_count
    pre(capabilities.minImageCount > 0)
        pre(capabilities.maxImageCount == 0 ||
            capabilities.minImageCount <= capabilities.maxImageCount)
{
    std::uint32_t selected_count = std::max(value_of(preferred_count), capabilities.minImageCount);
    if (capabilities.maxImageCount != 0)
    {
        selected_count = std::min(selected_count, capabilities.maxImageCount);
    }
    return swapchain_image_count{selected_count};
}
}  // namespace vulkandemo::vulkan
