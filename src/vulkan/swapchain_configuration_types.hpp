// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_configuration_types.hpp
 *
 * Pure swapchain configuration values.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include "./swapchain_image_count_types.hpp"

namespace vulkandemo::vulkan
{
/** Supported choices for a drawable swapchain. */
struct SwapchainConfiguration
{
    /** Advertised image format and color-space pair. */
    VkSurfaceFormatKHR surface_format;
    /** Advertised presentation mode. */
    VkPresentModeKHR present_mode;
    /** Nonzero drawable size in pixels. */
    VkExtent2D extent;
    /** Requested image count within the surface's limits. */
    swapchain_image_count image_count;
};
}  // namespace vulkandemo::vulkan
