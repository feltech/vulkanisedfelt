// SPDX-License-Identifier: MIT
// Copyright 2024-2025 David Feltell
#pragma once

#include <span>
#include <tuple>
#include <utility>

#include <immer/array.hpp>

#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../types.hpp"

namespace vulkandemo::setup
{

types::VulkanPipelineLayoutPtr create_minimal_pipeline_layout(
	types::VulkanDevicePtr const & device);

/**
 * Create a semaphore.
 *
 * @param device
 * @return
 */
types::VulkanSemaphorePtr create_semaphore(types::VulkanDevicePtr const & device);

/**
 * Create command buffers of primary level from a given pool.
 *
 * @param device
 * @param pool
 * @param count
 * @return
 */
types::VulkanCommandBuffersPtr create_primary_command_buffers(
	types::VulkanDevicePtr device,
	types::VulkanCommandPoolPtr pool,
	types::VulkanCommandBufferCount count);

/**
 * Create a command pool serving resettable command buffers for a given device queue family.
 *
 * @param device
 * @param queue_family_idx
 * @return
 */
types::VulkanCommandPoolPtr create_command_pool(
	types::VulkanDevicePtr device, types::VulkanQueueFamilyIdx queue_family_idx);

/**
 * Create a list of frame buffers, one-one mapped to a list of image views.
 *
 * @param device
 * @param render_pass
 * @param image_views
 * @param size
 * @return
 */
immer::array<types::VulkanFramebufferPtr> create_per_image_frame_buffers(
	types::VulkanDevicePtr const & device,
	types::VulkanRenderPassPtr const & render_pass,
	std::span<types::VulkanImageViewPtr const> image_views,
	VkExtent2D size);

/**
 * Create a simple render pass of a single subpass that hosts a single color attachment whose
 * final layout is appropriate for presentation on a surface.
 *
 * @param surface_format
 * @param device
 * @return
 */
types::VulkanRenderPassPtr create_single_presentation_subpass_render_pass(
	VkFormat surface_format, types::VulkanDevicePtr const & device);

/**
 * Create swapchain and (double-buffer) image views for given device.
 *
 * Many parameters are hardcoded.
 *
 * @param logger
 * @param physical_device
 * @param device
 * @param surface
 * @param surface_format
 * @param previous_swapchain
 * @return
 */
std::tuple<types::VulkanSwapchainPtr, immer::array<types::VulkanImageViewPtr>>
create_exclusive_double_buffer_swapchain_and_image_views(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanDevicePtr const & device,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR surface_format,
	types::VulkanSwapchainPtr const & previous_swapchain = nullptr);

/**
 * Create a Vulkan swapchain for a device and surface.
 *
 * @param device
 * @param create_info
 * @return
 */
types::VulkanSwapchainPtr create_swapchain(
	types::VulkanDevicePtr const & device, VkSwapchainCreateInfoKHR const & create_info);

/**
 * Query raw images associated with swapchain.
 *
 * @param device
 * @param swapchain
 * @return
 */
immer::array<VkImage> query_swapchain_images(
	types::VulkanDevicePtr const & device, types::VulkanSwapchainPtr const & swapchain);

/**
 * Create image views for swapchain images.
 *
 * @param device
 * @param surface_format
 * @param images
 * @return
 */
immer::array<types::VulkanImageViewPtr> create_colour_aspect_single_mip_single_layer_image_views(
	types::VulkanDevicePtr const & device,
	VkSurfaceFormatKHR surface_format,
	std::span<VkImage const> images);

/**
 * Given a physical device, desired queue types, and desired extensions, get a logical
 * device and corresponding queues.
 *
 * @param physical_device
 * @param queue_family_and_counts
 * @param device_extension_names
 * @return
 */
[[deprecated("Use create_device and query_queues_for_queue_family_and_counts instead")]]
std::tuple<types::VulkanDevicePtr, types::MapOfVulkanQueueFamilyIdxToVectorOfQueues>
create_device_and_queues(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> device_extension_names);

/**
 * Create a Vulkan device.
 *
 * @param physical_device
 * @param queue_family_and_counts
 * @param device_extension_names
 * @return
 */
types::VulkanDevicePtr create_device(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> device_extension_names);

/**
 * Get the queues for a device and queue family counts.
 *
 * @param device
 * @param queue_family_and_counts
 * @return
 */
types::MapOfVulkanQueueFamilyIdxToVectorOfQueues query_queues_for_queue_family_and_counts(
	VkDevice device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts);

/**
 * Query the extension names required by SDL for instance creation.
 *
 * @param sdl_window
 * @return
 */
immer::array<types::AvailableInstanceExtensionNameCstr> query_sdl_instance_extension_names(
	types::SDLWindowPtr const & sdl_window);

/**
 * Query all available instance layers.
 *
 * @return
 */
immer::array<VkLayerProperties> query_available_instance_layers();

/**
 * Query all available instance extensions.
 *
 * @return
 */
immer::array<VkExtensionProperties> query_available_instance_extensions();

/**
 * Get the title of an SDL window.
 *
 * @param window
 * @return
 */
char const * query_window_title(types::SDLWindowPtr const & window);

/**
 * Get a list of all physical devices.
 *
 * @param logger
 * @param instance
 * @return
 */
immer::array<VkPhysicalDevice> enumerate_physical_devices(
	LoggerPtr const & logger, types::VulkanInstancePtr const & instance);

/**
 * Enumerate all extension properties for a physical device
 *
 * @param physical_device
 * @return
 */
immer::array<VkExtensionProperties> enumerate_physical_device_extension_properties(
	VkPhysicalDevice physical_device);

/**
 * Enumerate all surface formats for a physical device and surface.
 *
 * @param physical_device
 * @param surface
 * @return
 */
immer::array<VkSurfaceFormatKHR> enumerate_physical_device_surface_formats(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface);

/**
 * Log the requested and available surface formats at debug/trace levels.
 *
 * @param logger
 * @param filtered_surface_formats
 * @param available_surface_formats
 * @param desired_formats
 */
void log_surface_format_selection(
	LoggerPtr const & logger,
	std::span<VkSurfaceFormatKHR const> filtered_surface_formats,
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats);

/**
 * Query surface capabilities for a physical device and surface.
 *
 * @param physical_device
 * @param surface
 * @return
 */
VkSurfaceCapabilitiesKHR query_surface_capabilities(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface);

/**
 * Query present modes for a physical device and surface.
 *
 * @param physical_device
 * @param surface
 * @return
 */
immer::array<VkPresentModeKHR> query_present_modes(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface);

/**
 * Create vulkan surface compatible with SDL window to render to.
 *
 * @param window
 * @param instance
 * @return
 */
types::VulkanSurfacePtr create_surface(
	types::SDLWindowPtr const & window, types::VulkanInstancePtr instance);

/**
 * Create a debug log messenger for use with the VK_EXT_debug_utils extension.
 *
 * @param logger
 * @param instance
 * @return
 */
types::VulkanDebugMessengerPtr create_debug_messenger(
	LoggerPtr logger, types::VulkanInstancePtr instance);

/**
 * Create VkInstance using given window and layers.
 *
 * @param logger
 * @param sdl_window
 * @param layers_to_enable
 * @param extensions_to_enable
 * @return
 */
types::VulkanInstancePtr create_vulkan_instance(
	LoggerPtr const & logger,
	types::SDLWindowPtr const & sdl_window,
	std::span<types::AvailableInstanceLayerNameCstr const> layers_to_enable,
	std::span<types::AvailableInstanceExtensionNameCstr const> extensions_to_enable);

/**
 * Get the drawable size of an SDL window.
 *
 * @param window
 * @return
 */
VkExtent2D window_drawable_size(types::SDLWindowPtr const & window);

/**
 * Create a window
 *
 * @param title The title of the window
 * @param width The width of the window
 * @param height The height of the window
 * @return The window
 */
types::SDLWindowPtr create_window(char const * title, int width, int height);

/**
 * Get the properties of a physical device.
 *
 * @param physical_device
 * @return
 */
VkPhysicalDeviceProperties query_physical_device_properties(VkPhysicalDevice physical_device);

/**
 * Query all available queue family properties for a physical device.
 *
 * @param physical_device
 * @return
 */
immer::array<VkQueueFamilyProperties> query_available_queue_family_properties(
	VkPhysicalDevice physical_device);

/**
 * Get the memory properties of a physical device.
 *
 * @param logger
 * @param physical_device
 * @return
 */
VkPhysicalDeviceMemoryProperties query_physical_device_memory_properties(
	LoggerPtr const & logger, VkPhysicalDevice physical_device);

/**
 * Check if a queue family supports a surface on a physical device.
 *
 * @param physical_device
 * @param surface
 * @param queue_family_idx
 * @return The queue family index if supported, otherwise nullopt.
 */
std::optional<types::VulkanQueueFamilyIdx> maybe_queue_family_idx_if_supported_by_physical_device_and_surface(
	VkPhysicalDevice physical_device,
	types::VulkanSurfacePtr const & surface,
	types::VulkanQueueFamilyIdx queue_family_idx);

/**
 * Create a Vulkan instance.
 *
 * @param logger
 * @param name
 * @param layers_to_enable
 * @param extensions_to_enable
 * @return
 */
types::VulkanInstancePtr create_instance(
	LoggerPtr const & logger,
	std::string name,
	immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
	immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable);

}  // namespace vulkandemo::setup