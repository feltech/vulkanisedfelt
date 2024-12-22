// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL_vulkan.h>

#include <vulkan/vulkan_core.h>

#include "types.hpp"

namespace vulkandemo::draw
{

/**
 * Enqueue image presentation.
 *
 * Queue must support presentation, see vkGetPhysicalDeviceSurfaceSupportKHR.
 *
 * @param queue
 * @param swapchain
 * @param image_idx
 * @param wait_semaphore
 * @return
 */
bool submit_present_image_cmd(
	VkQueue queue,
	types::VulkanSwapchainPtr const & swapchain,
	types::VulkanImageIdx image_idx,
	types::VulkanSemaphorePtr const & wait_semaphore);

/**
 * Submit a single command buffer to a queue, with a single wait/signal semaphore pair.
 *
 * @param queue
 * @param command_buffer
 * @param wait_semaphore
 * @param signal_semaphore
 */
void submit_command_buffer(
	VkQueue queue,
	VkCommandBuffer command_buffer,
	types::VulkanSemaphorePtr const & wait_semaphore,
	types::VulkanSemaphorePtr const & signal_semaphore);

/**
 * Populate a command buffer with a render pass that simply clears the frame buffer.
 *
 * @param command_buffer
 * @param render_pass
 * @param frame_buffer
 * @param extent
 * @param clear_colour
 */
void populate_cmd_render_pass(
	VkCommandBuffer command_buffer,
	types::VulkanRenderPassPtr const & render_pass,
	types::VulkanFramebufferPtr const & frame_buffer,
	VkExtent2D extent,
	types::VulkanClearColour const & clear_colour);

/**
 * Acquire next swapchain image, returning empty optional if the swapchain is out of date and
 * needs re-creating.
 *
 * @param device
 * @param swapchain
 * @param semaphore
 * @return
 */
std::optional<types::VulkanImageIdx> acquire_next_swapchain_image(
	types::VulkanDevicePtr const & device,
	types::VulkanSwapchainPtr const & swapchain,
	types::VulkanSemaphorePtr const & semaphore);

}  // namespace vulkandemo::draw