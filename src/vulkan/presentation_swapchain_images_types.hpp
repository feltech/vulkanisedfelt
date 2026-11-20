// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_swapchain_images_types.hpp
 *
 * Presentation resources with their swapchain-owned images.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./presentation_swapchain_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/** Presentation context retaining the owner of its borrowed images. */
struct PresentationSwapchainImages
{
    /** Configuration, queues and ownership required throughout image use. */
    PresentationSwapchain presentation_swapchain;
    /** Swapchain-owned handles; never destroy these individually. */
    immer::array<VkImage> images;
};

/** Outcome of presentation setup with retrieved image handles. */
using presentation_swapchain_images_result = renderer_result<PresentationSwapchainImages>;
}  // namespace vulkandemo::vulkan
