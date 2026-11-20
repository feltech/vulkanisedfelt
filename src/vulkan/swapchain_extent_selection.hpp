// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_extent_selection.hpp
 *
 * Pure selection of drawable swapchain extents.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

#include <vulkan/vulkan_core.h>

namespace vulkandemo::vulkan
{
/**
 * Selects the fixed surface extent or clamps the requested drawable pixel size.
 *
 * A zero drawable dimension is unavailable when the surface allows a variable extent.
 * Fixed extents are authoritative regardless of the requested size.
 *
 * @param capabilities Surface limits with ordered minimum and maximum extents.
 * @param drawable_extent Requested drawable size in pixels.
 * @return Nonzero extent, or no value while the selected drawable area is zero.
 */
[[nodiscard]] constexpr auto select_swapchain_extent(
    VkSurfaceCapabilitiesKHR capabilities, VkExtent2D drawable_extent)
    -> std::optional<VkExtent2D> pre(
        capabilities.minImageExtent.width <= capabilities.maxImageExtent.width)
        pre(capabilities.minImageExtent.height <= capabilities.maxImageExtent.height)
{
    constexpr std::uint32_t variable_extent = std::numeric_limits<std::uint32_t>::max();
    VkExtent2D selected_extent = capabilities.currentExtent;
    if (selected_extent.width == variable_extent)
    {
        if (drawable_extent.width == 0 || drawable_extent.height == 0)
        {
            return std::nullopt;
        }
        selected_extent = {
            std::clamp(
                drawable_extent.width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width),
            std::clamp(
                drawable_extent.height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height)};
    }
    if (selected_extent.width == 0 || selected_extent.height == 0)
    {
        return std::nullopt;
    }
    return selected_extent;
}
}  // namespace vulkandemo::vulkan
