// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/image_acquisition_types.hpp
 *
 * Acquired swapchain image indices and retained owners.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
struct SwapchainImageIndexTag;
/** An index in a swapchain's image snapshot. */
using swapchain_image_index = strong::type<std::uint32_t, SwapchainImageIndexTag, strong::regular>;

/** An acquired image with the owners needed through acquisition and image use. */
struct AcquiredSwapchainImage
{
    /** Owner of the indexed image. */
    swapchain_ptr swapchain;
    /** Optional acquisition semaphore; signaling may still be pending. */
    semaphore_ptr semaphore;
    /** Optional acquisition fence; signaling may still be pending. */
    fence_ptr fence;
    /** Index into the swapchain's retrieved images. */
    swapchain_image_index image_index;
    /** Whether acquisition succeeded with a suboptimal swapchain. */
    bool is_suboptimal;
};

}  // namespace vulkandemo::vulkan
