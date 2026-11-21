// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/errors.hpp
 *
 * Common failures for fallible Vulkan actions.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>

namespace vulkandemo::vulkan
{
/// Wrapped Vulkan operations that can report a failure.
enum class FallibleVulkanOperation
{
    /// Instance API version query.
    enumerate_instance_version,
    /// Binary semaphore creation.
    create_semaphore,
    /// Command pool creation.
    create_command_pool,
    /// Primary command buffer allocation.
    allocate_command_buffer,
    /// Framebuffer creation.
    create_framebuffer,
    /// Render pass creation.
    create_render_pass,
    /// Image view creation.
    create_image_view,
    /// Swapchain image query.
    get_swapchain_images,
    /// Swapchain creation.
    create_swapchain,
    /// Image-format support query.
    get_physical_device_image_format_properties,
    /// Instance creation.
    create_instance,
    /// Instance extension enumeration.
    enumerate_instance_extensions,
    /// Instance layer enumeration.
    enumerate_instance_layers,
    /// Physical device enumeration.
    enumerate_physical_devices,
    /// Device extension enumeration.
    enumerate_device_extensions,
    /// Queue-family presentation-support query.
    get_physical_device_surface_support,
    /// Surface capability query.
    get_physical_device_surface_capabilities,
    /// Surface format query.
    get_physical_device_surface_formats,
    /// Surface present-mode query.
    get_physical_device_surface_present_modes,
    /// Logical device creation.
    create_device
};

/// A failed Vulkan operation and its original status.
struct VulkanFailure
{
    /// Operation that failed.
    FallibleVulkanOperation operation;
    /// Original Vulkan status.
    VkResult result;
};

/// Common outcome of fallible Vulkan actions.
template <class T>
using vulkan_result = fnfelt::Result<T, VulkanFailure>;
}  // namespace vulkandemo::vulkan
