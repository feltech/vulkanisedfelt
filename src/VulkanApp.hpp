// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan_core.h>

#include <gsl/pointers>

#include <strong_type/ordered.hpp>
#include <strong_type/semiregular.hpp>
#include <strong_type/strong_type.hpp>

#include "Logger.hpp"

namespace vulkandemo::setup
{

#define VK_CHECK(func, msg)                                                                \
	do { /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                   \
		if (const VkResult result = func; result != VK_SUCCESS)                            \
		{                                                                                  \
			throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
		}                                                                                  \
	} while (false)

using SDLWindowPtr = std::shared_ptr<SDL_Window>;
SDLWindowPtr make_window_ptr(SDL_Window * window);

using VulkanInstancePtr = std::shared_ptr<std::remove_pointer_t<VkInstance>>;
VulkanInstancePtr make_instance_ptr(VkInstance instance);

using VulkanSurfacePtr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;
VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface);

using VulkanDevicePtr = std::shared_ptr<std::remove_pointer_t<VkDevice>>;
VulkanDevicePtr make_device_ptr(VkDevice device);

using VulkanDebugMessengerPtr = std::shared_ptr<std::remove_pointer_t<VkDebugUtilsMessengerEXT>>;
VulkanDebugMessengerPtr make_debug_messenger_ptr(
	VulkanInstancePtr instance,
	gsl::owner<LoggerPtr *> plogger,
	VkDebugUtilsMessengerEXT messenger);

using VulkanSwapchainPtr = std::shared_ptr<std::remove_pointer_t<VkSwapchainKHR>>;
VulkanSwapchainPtr make_swapchain_ptr(VulkanDevicePtr device, VkSwapchainKHR swapchain);

using VulkanImageViewPtr = std::shared_ptr<std::remove_pointer_t<VkImageView>>;
VulkanImageViewPtr make_image_view_ptr(VulkanDevicePtr device, VkImageView image_view);

using VulkanRenderPassPtr = std::shared_ptr<std::remove_pointer_t<VkRenderPass>>;
VulkanRenderPassPtr make_render_pass_ptr(VulkanDevicePtr device, VkRenderPass render_pass);

using VulkanFramebufferPtr = std::shared_ptr<std::remove_pointer_t<VkFramebuffer>>;
VulkanFramebufferPtr make_framebuffer_ptr(VulkanDevicePtr device, VkFramebuffer framebuffer);

using VulkanCommandPoolPtr = std::shared_ptr<std::remove_pointer_t<VkCommandPool>>;
VulkanCommandPoolPtr make_command_pool_ptr(
	VulkanDevicePtr device, VkCommandPool command_pool);

using VulkanCommandBuffersPtr = std::shared_ptr<std::vector<VkCommandBuffer>>;
VulkanCommandBuffersPtr make_command_buffers_ptr(
	VulkanDevicePtr device,
	VulkanCommandPoolPtr pool,
	std::vector<VkCommandBuffer> command_buffers);

using VulkanSemaphorePtr = std::shared_ptr<std::remove_pointer_t<VkSemaphore>>;
VulkanSemaphorePtr make_semaphore_ptr(VulkanDevicePtr device, VkSemaphore semaphore);

using VulkanImageIdx = strong::type<
	uint32_t,
	struct TagForVulkanImageIdx,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t, std::size_t>,
	strong::equality,
	strong::equality_with<uint32_t, std::size_t>>;

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
	VulkanSwapchainPtr const & swapchain,
	VulkanImageIdx image_idx,
	VulkanSemaphorePtr const & wait_semaphore);

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
	VulkanSemaphorePtr const & wait_semaphore,
	VulkanSemaphorePtr const & signal_semaphore);

using VulkanClearColour = strong::type<
	std::array<float, 4>,
	struct TagForRgbaColourFractions,
	strong::regular,
	strong::indexed<>,
	strong::range>;

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
	VulkanRenderPassPtr const & render_pass,
	VulkanFramebufferPtr const & frame_buffer,
	VkExtent2D extent,
	VulkanClearColour const & clear_colour);

/**
 * Create a semaphore.
 *
 * @param device
 * @return
 */
VulkanSemaphorePtr create_semaphore(VulkanDevicePtr const & device);

/**
 * Acquire next swapchain image, returning empty optional if the swapchain is out of date and
 * needs re-creating.
 *
 * @param device
 * @param swapchain
 * @param semaphore
 * @return
 */
std::optional<VulkanImageIdx> acquire_next_swapchain_image(
	VulkanDevicePtr const & device,
	VulkanSwapchainPtr const & swapchain,
	VulkanSemaphorePtr const & semaphore);

using VulkanCommandBufferCount = strong::type<
	uint32_t,
	struct TagForVulkanCommandBufferCount,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t, std::size_t>,
	strong::equality,
	strong::equality_with<uint32_t, std::size_t>>;

/**
 * Create command buffers of primary level from a given pool.
 *
 * @param device
 * @param pool
 * @param count
 * @return
 */
VulkanCommandBuffersPtr create_primary_command_buffers(
	VulkanDevicePtr device, VulkanCommandPoolPtr pool, VulkanCommandBufferCount count);

using VulkanQueueFamilyIdx = strong::type<
	uint32_t,
	struct TagForVulkanQueueFamilyIdx,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t>,
	strong::equality,
	strong::equality_with<uint32_t>,
	strong::bicrementable,
	strong::strongly_ordered>;

/**
 * Create a command pool serving resettable command buffers for a given device queue family.
 *
 * @param device
 * @param queue_family_idx
 * @return
 */
VulkanCommandPoolPtr create_command_pool(
	VulkanDevicePtr device, VulkanQueueFamilyIdx queue_family_idx);

/**
 * Create a list of frame buffers, one-one mapped to a list of image views.
 *
 * @param device
 * @param render_pass
 * @param image_views
 * @param size
 * @return
 */
std::vector<VulkanFramebufferPtr> create_per_image_frame_buffers(
	VulkanDevicePtr const & device,
	VulkanRenderPassPtr const & render_pass,
	std::vector<VulkanImageViewPtr> const & image_views,
	VkExtent2D size);

/**
 * Create a simple render pass of a single subpass that hosts a single color attachment whose
 * final layout is appropriate for presentation on a surface.
 *
 * @param surface_format
 * @param device
 * @return
 */
VulkanRenderPassPtr create_single_presentation_subpass_render_pass(
	VkFormat surface_format, VulkanDevicePtr const & device);

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
std::tuple<VulkanSwapchainPtr, std::vector<VulkanImageViewPtr>>
create_double_buffer_swapchain(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	VulkanDevicePtr const & device,
	VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR surface_format,
	VulkanSwapchainPtr const & previous_swapchain = nullptr);

using VulkanQueueCount = strong::type<
	uint32_t,
	struct TagForVulkanQueueCount,
	strong::implicitly_convertible_to<uint32_t, std::size_t>,
	strong::equality,
	strong::equality_with<uint32_t, std::size_t>,
	strong::strongly_ordered,
	strong::bicrementable>;

using MapOfVulkanQueueFamilyIdxToVectorOfQueues =
	std::map<VulkanQueueFamilyIdx, std::vector<VkQueue>>;

using VectorOfAvailableDeviceExtensionNameViews = strong::type<
	std::vector<std::string_view>,
	struct TagForVectorOfAvailableDeviceExtensionNameViews,
	strong::regular,
	strong::range,
	strong::indexed<>>;

/**
 * Given a physical device, desired queue types, and desired extensions, get a logical
 * device and corresponding queues.
 *
 * @param physical_device
 * @param queue_family_and_counts
 * @param device_extension_names
 * @return
 */
std::tuple<VulkanDevicePtr, MapOfVulkanQueueFamilyIdxToVectorOfQueues>
create_device_and_queues(
	VkPhysicalDevice physical_device,
	std::vector<std::pair<VulkanQueueFamilyIdx, VulkanQueueCount>> const & queue_family_and_counts,
	VectorOfAvailableDeviceExtensionNameViews const & device_extension_names);

/**
 * Given some desired image/surface formats (e.g. VK_FORMAT_B8G8R8_UNORM), filter to only those
 * suppored by the device and surface.
 *
 * Will preserve ordering, so that @p desired_formats can be in priority order.
 *
 * @param logger
 * @param physical_device
 * @param surface
 * @param desired_formats
 * @return
 */
std::vector<VkSurfaceFormatKHR> filter_available_surface_formats(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	VulkanSurfacePtr const & surface,
	std::vector<VkFormat> desired_formats);

using SetOfDesiredDeviceExtensionNameViews = strong::type<
	std::set<std::string_view>,
	struct TagForSetOfDesiredDeviceExtensionNameViews,
	strong::regular,
	strong::range>;

/**
 * Given a list of physical devices, pick the first that has desired capabilities.
 *
 * @param logger
 * @param physical_devices
 * @param required_device_extensions
 * @param required_queue_capabilities
 * @param surface
 * @return
 */
std::tuple<VkPhysicalDevice, VulkanQueueFamilyIdx> select_physical_device(
	LoggerPtr const & logger,
	std::vector<VkPhysicalDevice> const & physical_devices,
	SetOfDesiredDeviceExtensionNameViews const & required_device_extensions,
	VkQueueFlagBits required_queue_capabilities,
	VkSurfaceKHR surface = nullptr);

/**
 * Given a device and set of desired device extensions, filter to only those extensions that
 * are supported by the device.
 *
 * @param logger
 * @param physical_device
 * @param desired_device_extension_names
 * @return
 */
VectorOfAvailableDeviceExtensionNameViews filter_available_device_extensions(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	SetOfDesiredDeviceExtensionNameViews const & desired_device_extension_names);

/**
 * Filter queue families to find those with desired capabilities
 *
 * @param physical_device Device to check queue families for
 * @param desired_queue_capabilities Required queue capabilities
 * @return
 */
std::vector<VulkanQueueFamilyIdx> filter_available_queue_families(
	VkPhysicalDevice const & physical_device, VkQueueFlagBits desired_queue_capabilities);

/**
 * Filter a list of physical devices to only those that support presentation on a given surface.
 *
 * No-op if surface is null.
 *
 * @param physical_devices
 * @param surface
 * @return
 */
std::vector<VkPhysicalDevice> filter_physical_devices_for_surface_support(
	std::vector<VkPhysicalDevice> const & physical_devices, VkSurfaceKHR surface);

/**
 * Get a list of all physical devices.
 *
 * @param logger
 * @param instance
 * @return
 */
std::vector<VkPhysicalDevice> enumerate_physical_devices(
	LoggerPtr const & logger, VulkanInstancePtr const & instance);

/**
 * Create vulkan surface compatible with SDL window to render to.
 * @param window
 * @param instance
 * @return
 */
VulkanSurfacePtr create_surface(SDLWindowPtr const & window, VulkanInstancePtr instance);

/**
 * Create a debug log messenger for use with the VK_EXT_debug_utils extension.
 *
 * @param logger
 * @param instance
 * @return
 */
VulkanDebugMessengerPtr create_debug_messenger(LoggerPtr logger, VulkanInstancePtr instance);

using VectorOfAvailableInstanceLayerNameCstrs = strong::type<
	std::vector<char const *>,
	struct TagForVectorOfAvailableInstanceLayerNameCstrs,
	strong::regular,
	strong::range>;

using VectorOfAvailableInstanceExtensionNameCstrs = strong::type<
	std::vector<char const *>,
	struct TagForVectorOfAvailableInstanceExtensionNameCstrs,
	strong::semiregular,
	strong::range>;

/**
 * Create VkInstance using given window and layers.
 *
 * @param logger
 * @param sdl_window
 * @param layers_to_enable
 * @param extensions_to_enable
 * @return
 */
VulkanInstancePtr create_vulkan_instance(
	LoggerPtr const & logger,
	SDLWindowPtr const & sdl_window,
	VectorOfAvailableInstanceLayerNameCstrs const & layers_to_enable,
	VectorOfAvailableInstanceExtensionNameCstrs const & extensions_to_enable);

using SetOfDesiredInstanceLayerNameViews = strong::type<
	std::set<std::string_view>,
	struct TagForSetOfDesiredInstanceLayerNameViews,
	strong::regular,
	strong::range>;

/**
 * Query available layers vs. desired layers.

 * @param logger
 * @param desired_layer_names
 * @return
 */
VectorOfAvailableInstanceLayerNameCstrs filter_available_layers(
	LoggerPtr const & logger, SetOfDesiredInstanceLayerNameViews const & desired_layer_names);

using SetOfDesiredInstanceExtensionNameViews = strong::type<
	std::set<std::string_view>,
	struct TagForSetOfDesiredInstanceExtensionNameViews,
	strong::semiregular,
	strong::range>;

/**
 * Query available generic instance extensions vs. desired..
 *
 * @param logger
 * @param desired_extension_names
 * @return
 */
VectorOfAvailableInstanceExtensionNameCstrs filter_available_instance_extensions(
	LoggerPtr const & logger,
	SetOfDesiredInstanceExtensionNameViews const & desired_extension_names);

/**
 * Get the drawable size of an SDL window.
 *
 * @param window
 * @return
 */
VkExtent2D window_drawable_size(SDLWindowPtr const & window);

/**
 * Create a window
 *
 * @param title The title of the window
 * @param width The width of the window
 * @param height The height of the window
 * @return The window
 */
SDLWindowPtr create_window(char const * title, int width, int height);
}  // namespace vulkandemo::setup