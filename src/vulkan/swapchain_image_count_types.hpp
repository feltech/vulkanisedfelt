// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_image_count_types.hpp
 *
 * Swapchain image-count vocabulary.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct SwapchainImageCountTag;
/** Number of images requested for a swapchain. */
using swapchain_image_count = strong::type<std::uint32_t, SwapchainImageCountTag, strong::regular>;
}  // namespace vulkandemo::vulkan
