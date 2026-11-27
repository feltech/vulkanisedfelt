// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_targets_types.hpp
 *
 * Render pass compatibility inputs for presentation-target assembly.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/** Existing clear-and-present render pass paired with the image format it was built for. */
struct CompatibleRenderPass
{
    /** Swapchain image format used by @c render_pass. */
    VkFormat format{VK_FORMAT_UNDEFINED};
    /** Existing compatible render pass, or null when a new pass must be created. */
    render_pass_ptr render_pass{};
};
}  // namespace vulkandemo::vulkan
