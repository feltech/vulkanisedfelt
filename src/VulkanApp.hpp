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

#include <strong_type/semiregular.hpp>
#include <strong_type/strong_type.hpp>

#include "Logger.hpp"

namespace vulkandemo
{

#define VK_CHECK(func, msg)                                                                \
	do { /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                   \
		if (const VkResult result = func; result != VK_SUCCESS)                            \
		{                                                                                  \
			throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
		}                                                                                  \
	} while (false)

struct VulkanApp
{
	using SDLWindowPtr = std::shared_ptr<SDL_Window>;
	static SDLWindowPtr make_window_ptr(SDL_Window * window);

	using VulkanInstancePtr = std::shared_ptr<std::remove_pointer_t<VkInstance>>;
	static VulkanInstancePtr make_instance_ptr(VkInstance instance);

	using VulkanSurfacePtr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;
	static VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface);

	using VulkanDevicePtr = std::shared_ptr<std::remove_pointer_t<VkDevice>>;
	static VulkanDevicePtr make_device_ptr(VkDevice device);

	using VulkanDebugMessengerPtr =
		std::shared_ptr<std::remove_pointer_t<VkDebugUtilsMessengerEXT>>;
	static VulkanDebugMessengerPtr make_debug_messenger_ptr(
		VulkanInstancePtr instance,
		gsl::owner<LoggerPtr *> plogger,
		VkDebugUtilsMessengerEXT messenger);

	using VulkanSwapchainPtr = std::shared_ptr<std::remove_pointer_t<VkSwapchainKHR>>;
	static VulkanSwapchainPtr make_swapchain_ptr(VulkanDevicePtr device, VkSwapchainKHR swapchain);

	using VulkanImageViewPtr = std::shared_ptr<std::remove_pointer_t<VkImageView>>;
	static VulkanImageViewPtr make_image_view_ptr(VulkanDevicePtr device, VkImageView image_view);

	using VulkanRenderPassPtr = std::shared_ptr<std::remove_pointer_t<VkRenderPass>>;
	static VulkanRenderPassPtr make_render_pass_ptr(
		VulkanDevicePtr device, VkRenderPass render_pass);

	using VulkanFramebufferPtr = std::shared_ptr<std::remove_pointer_t<VkFramebuffer>>;
	static VulkanFramebufferPtr make_framebuffer_ptr(
		VulkanDevicePtr device, VkFramebuffer framebuffer);

	using VulkanCommandPoolPtr = std::shared_ptr<std::remove_pointer_t<VkCommandPool>>;
	static VulkanCommandPoolPtr make_command_pool_ptr(
		VulkanDevicePtr device, VkCommandPool command_pool);

	/**
	 * RAII class for command buffers, which are allocated and deallocated as a batch, but are
	 * accessed independently.
	 */
	class VulkanCommandBuffers
	{
		VulkanDevicePtr device_;
		VulkanCommandPoolPtr pool_;
		std::vector<VkCommandBuffer> buffers_;

	public:
		VulkanCommandBuffers(
			VulkanDevicePtr device,
			VulkanCommandPoolPtr pool,
			std::vector<VkCommandBuffer> buffers);
		VulkanCommandBuffers(VulkanCommandBuffers const & other) = delete;
		VulkanCommandBuffers(VulkanCommandBuffers && other) noexcept = default;
		VulkanCommandBuffers & operator=(VulkanCommandBuffers const & other) = delete;
		VulkanCommandBuffers & operator=(VulkanCommandBuffers && other) noexcept = default;

		// ReSharper disable once CppNonExplicitConversionOperator
		// NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
		explicit(false) operator std::vector<VkCommandBuffer> const &() const;
		[[nodiscard]] std::vector<VkCommandBuffer> const & as_vector() const;
		~VulkanCommandBuffers();
	};

	static_assert(std::is_convertible_v<VulkanCommandBuffers, std::vector<VkCommandBuffer>>);

	using VulkanSemaphorePtr = std::shared_ptr<std::remove_pointer_t<VkSemaphore>>;
	static VulkanSemaphorePtr make_semaphore_ptr(VulkanDevicePtr device, VkSemaphore semaphore);

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
	static bool submit_present_image_cmd(
		VkQueue queue,
		VulkanSwapchainPtr const & swapchain,
		uint32_t image_idx,
		VulkanSemaphorePtr const & wait_semaphore);

	/**
	 * Submit a single command buffer to a queue, with a single wait/signal semaphore pair.
	 *
	 * @param queue
	 * @param command_buffer
	 * @param wait_semaphore
	 * @param signal_semaphore
	 */
	static void submit_command_buffer(
		VkQueue queue,
		VkCommandBuffer command_buffer,
		VulkanSemaphorePtr const & wait_semaphore,
		VulkanSemaphorePtr const & signal_semaphore);

	/**
	 * Populate a command buffer with a render pass that simply clears the frame buffer.
	 *
	 * @param command_buffer
	 * @param render_pass
	 * @param frame_buffer
	 * @param extent
	 * @param clear_colour
	 */
	static void populate_cmd_render_pass(
		VkCommandBuffer command_buffer,
		VulkanRenderPassPtr const & render_pass,
		VulkanFramebufferPtr const & frame_buffer,
		VkExtent2D extent,
		std::array<float, 4> const & clear_colour);

	/**
	 * Create a semaphore.
	 *
	 * @tparam count
	 * @param device
	 * @return
	 */
	static VulkanSemaphorePtr create_semaphore(VulkanDevicePtr const & device);

	/**
	 * Acquire next swapchain image, returning empty optional if the swapchain is out of date and
	 * needs re-creating.
	 *
	 * @param device
	 * @param swapchain
	 * @param semaphore
	 * @return
	 */
	static std::optional<uint32_t> acquire_next_swapchain_image(
		VulkanDevicePtr const & device,
		VulkanSwapchainPtr const & swapchain,
		VulkanSemaphorePtr const & semaphore);

	/**
	 * Create a command buffer of primary level from a given pool.
	 *
	 * @param device
	 * @param pool
	 * @param count
	 * @return
	 */
	static VulkanCommandBuffers create_primary_command_buffers(
		VulkanDevicePtr device, VulkanCommandPoolPtr pool, uint32_t count);

	/**
	 * Create a command pool serving resettable command buffers for a given device queue family.
	 *
	 * @param device
	 * @param queue_family_idx
	 * @return
	 */
	static VulkanCommandPoolPtr create_command_pool(
		VulkanDevicePtr device, uint32_t queue_family_idx);

	/**
	 * Create a list of frame buffers, one-one mapped to a list of image views.
	 *
	 * @param device
	 * @param render_pass
	 * @param image_views
	 * @param size
	 * @return
	 */
	static std::vector<VulkanFramebufferPtr> create_per_image_frame_buffers(
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
	static VulkanRenderPassPtr create_single_presentation_subpass_render_pass(
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
	static std::tuple<VulkanSwapchainPtr, std::vector<VulkanImageViewPtr>>
	create_double_buffer_swapchain(
		LoggerPtr const & logger,
		VkPhysicalDevice physical_device,
		VulkanDevicePtr const & device,
		VulkanSurfacePtr const & surface,
		VkSurfaceFormatKHR surface_format,
		VulkanSwapchainPtr const & previous_swapchain = nullptr);

	/**
	 * Given a physical device, desired queue types, and desired extensions, get a logical
	 * device and corresponding queues.
	 *
	 * @param physical_device
	 * @param queue_family_and_counts
	 * @param device_extension_names
	 * @return
	 */
	static std::tuple<VulkanDevicePtr, std::map<uint32_t, std::vector<VkQueue>>>
	create_device_and_queues(
		VkPhysicalDevice physical_device,
		std::vector<std::pair<uint32_t, uint32_t>> const & queue_family_and_counts,
		std::vector<std::string_view> const & device_extension_names);

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
	static std::vector<VkSurfaceFormatKHR> filter_available_surface_formats(
		LoggerPtr const & logger,
		VkPhysicalDevice physical_device,
		VulkanSurfacePtr const & surface,
		std::vector<VkFormat> desired_formats);

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
	static std::tuple<VkPhysicalDevice, uint32_t> select_physical_device(
		LoggerPtr const & logger,
		std::vector<VkPhysicalDevice> const & physical_devices,
		std::set<std::string_view> const & required_device_extensions,
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
	static std::vector<std::string_view> filter_available_device_extensions(
		LoggerPtr const & logger,
		VkPhysicalDevice physical_device,
		std::set<std::string_view> const & desired_device_extension_names);

	/**
	 * Filter queue families to find those with desired capabilities
	 *
	 * @param physical_device Device to check queue families for
	 * @param desired_queue_capabilities Required queue capabilities
	 * @return
	 */
	static std::vector<uint32_t> filter_available_queue_families(
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
	static std::vector<VkPhysicalDevice> filter_physical_devices_for_surface_support(
		std::vector<VkPhysicalDevice> const & physical_devices, VkSurfaceKHR surface);

	/**
	 * Get a list of all physical devices.
	 *
	 * @param logger
	 * @param instance
	 * @return
	 */
	static std::vector<VkPhysicalDevice> enumerate_physical_devices(
		LoggerPtr const & logger, VulkanInstancePtr const & instance);

	/**
	 * Create vulkan surface compatible with SDL window to render to.
	 * @param window
	 * @param instance
	 * @return
	 */
	static VulkanSurfacePtr create_surface(SDLWindowPtr const & window, VulkanInstancePtr instance);

	/**
	 * Create a debug log messenger for use with the VK_EXT_debug_utils extension.
	 *
	 * @param logger
	 * @param instance
	 * @return
	 */
	static VulkanDebugMessengerPtr create_debug_messenger(
		LoggerPtr logger, VulkanInstancePtr instance);

	using InstanceLayerNameCstrList = strong::
		type<std::vector<char const *>, struct InstanceLayerNameCstrList_, strong::semiregular>;
	using InstanceExtensionNameCstrList = strong::
		type<std::vector<char const *>, struct InstanceExtensionNameCstrList_, strong::semiregular>;

	/**
	 * Create VkInstance using given window and layers.
	 *
	 * @param logger
	 * @param sdl_window
	 * @param layers_to_enable
	 * @param extensions_to_enable
	 * @return
	 */
	static VulkanInstancePtr create_vulkan_instance(
		LoggerPtr const & logger,
		SDLWindowPtr const & sdl_window,
		InstanceLayerNameCstrList const & layers_to_enable,
		InstanceExtensionNameCstrList const & extensions_to_enable);

	/**
	 * Query available layers vs. desired layers.

	 * @param logger
	 * @param desired_layer_names
	 * @return
	 */
	static std::vector<char const *> filter_available_layers(
		LoggerPtr const & logger, std::set<std::string_view> const & desired_layer_names);

	/**
	 * Query available generic instance extensions vs. desired..
	 *
	 * @param logger
	 * @param desired_extension_names
	 * @return
	 */
	static std::vector<char const *> filter_available_instance_extensions(
		LoggerPtr const & logger, std::set<std::string_view> const & desired_extension_names);

	/**
	 * Get the drawable size of an SDL window.
	 *
	 * @param window
	 * @return
	 */
	static VkExtent2D window_drawable_size(SDLWindowPtr const & window);

	/**
	 * Create a window
	 *
	 * @param title The title of the window
	 * @param width The width of the window
	 * @param height The height of the window
	 * @return The window
	 */
	static SDLWindowPtr create_window(char const * title, int width, int height);
};
}  // namespace vulkandemo