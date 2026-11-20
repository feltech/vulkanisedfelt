// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_images_types.hpp
 *
 * Presentable image handles with retained swapchain ownership.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/** Borrowed images whose lifetime is anchored by their swapchain owner. */
struct SwapchainImages
{
    /** Owner that must remain alive throughout use of the image handles. */
    swapchain_ptr swapchain;
    /** Images owned by Vulkan; never destroy these individually. */
    immer::array<VkImage> images;
};
}  // namespace vulkandemo::vulkan
