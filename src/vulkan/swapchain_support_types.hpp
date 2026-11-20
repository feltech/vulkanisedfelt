// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_support_types.hpp
 *
 * Owned presentation surface support snapshots.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./errors.hpp"

namespace vulkandemo::vulkan
{
/** Surface facts available for swapchain configuration. */
struct SwapchainSupport
{
    /** Limits and supported surface operations. */
    VkSurfaceCapabilitiesKHR capabilities;
    /** Supported image format and color-space combinations. */
    immer::array<VkSurfaceFormatKHR> formats;
    /** Supported presentation modes. */
    immer::array<VkPresentModeKHR> present_modes;
};

/** Outcome of collecting swapchain support. */
using swapchain_support_result = vulkan_result<SwapchainSupport>;
}  // namespace vulkandemo::vulkan
