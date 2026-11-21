// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/managed_handles_types.hpp
 *
 * Shared ownership types for Vulkan handles.
 */
#pragma once

#include <memory>
#include <type_traits>

#include <vulkan/vulkan_core.h>

namespace vulkandemo::vulkan
{
/** Shared instance owner. */
using instance_ptr = std::shared_ptr<VkInstance_T>;

/** Shared logical device owner. */
using device_ptr = std::shared_ptr<VkDevice_T>;

static_assert(
    std::is_pointer_v<VkCommandPool>, "Command pool owners require pointer-shaped Vulkan handles");

/** Shared command pool owner retaining its logical device. */
using command_pool_ptr = std::shared_ptr<std::remove_pointer_t<VkCommandPool>>;

/** Shared command buffer owner retaining its command pool and logical device. */
using command_buffer_ptr = std::shared_ptr<VkCommandBuffer_T>;

static_assert(
    std::is_pointer_v<VkFramebuffer>, "Framebuffer owners require pointer-shaped Vulkan handles");

/** Shared framebuffer owner retaining its attachment and render pass. */
using framebuffer_ptr = std::shared_ptr<std::remove_pointer_t<VkFramebuffer>>;

static_assert(
    std::is_pointer_v<VkRenderPass>, "Render pass owners require pointer-shaped Vulkan handles");

/** Shared render pass owner retaining its logical device. */
using render_pass_ptr = std::shared_ptr<std::remove_pointer_t<VkRenderPass>>;

static_assert(
    std::is_pointer_v<VkSurfaceKHR>, "Surface owners require pointer-shaped Vulkan handles");

/** Shared surface owner exposing its native Vulkan handle through get(). */
using surface_ptr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;

static_assert(
    std::is_pointer_v<VkSwapchainKHR>, "Swapchain owners require pointer-shaped Vulkan handles");

/** Shared swapchain owner exposing its native Vulkan handle through get(). */
using swapchain_ptr = std::shared_ptr<std::remove_pointer_t<VkSwapchainKHR>>;

static_assert(
    std::is_pointer_v<VkImageView>, "Image view owners require pointer-shaped Vulkan handles");

/** Shared image view owner retaining the image's swapchain. */
using image_view_ptr = std::shared_ptr<std::remove_pointer_t<VkImageView>>;
}  // namespace vulkandemo::vulkan
