// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <concepts>
#include <cstdint>
#include <range/v3/view/transform.hpp>
#include <ranges>
#include <set>
#include <span>
#include <spdlog/common.h>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/to_container.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <spdlog/spdlog.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "Logger.hpp"
#include "hof.hpp"
#include "macros.hpp"
#include "monad.hpp"
#include "types.hpp"

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
std::vector<types::VulkanFramebufferPtr> create_per_image_frame_buffers(
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
std::tuple<types::VulkanSwapchainPtr, std::vector<types::VulkanImageViewPtr>>
create_exclusive_double_buffer_swapchain_and_image_views(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanDevicePtr const & device,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR surface_format,
	types::VulkanSwapchainPtr const & previous_swapchain = nullptr);

/**
 * Given a physical device, desired queue types, and desired extensions, get a logical
 * device and corresponding queues.
 *
 * @param physical_device
 * @param queue_family_and_counts
 * @param device_extension_names
 * @return
 */
std::tuple<types::VulkanDevicePtr, types::MapOfVulkanQueueFamilyIdxToVectorOfQueues>
create_device_and_queues(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> device_extension_names);

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
	types::VulkanSurfacePtr const & surface,
	std::span<VkFormat const> desired_formats);

/**
 * Given a list of physical devices, pick the first that has desired capabilities.
 *
 * @param logger
 * @param physical_devices
 * @param required_device_extensions
 * @param required_queue_capabilities
 * @param required_memory_type
 * @param required_surface_support
 * @return
 */
std::tuple<VkPhysicalDevice, types::VulkanQueueFamilyIdx> select_physical_device(
	LoggerPtr const & logger,
	std::vector<VkPhysicalDevice> const & physical_devices,
	std::set<types::DesiredDeviceExtensionNameView> const & required_device_extensions,
	VkQueueFlagBits required_queue_capabilities,
	VkMemoryPropertyFlags required_memory_type = 0,
	types::VulkanSurfacePtr const & required_surface_support = nullptr);

/**
 * Given a device and set of desired device extensions, filter to only those extensions that
 * are supported by the device.
 *
 * @param logger
 * @param physical_device
 * @param desired_device_extension_names
 * @return
 */
std::vector<types::AvailableDeviceExtensionNameView> filter_available_device_extensions(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	std::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names);

/**
 * Filter queue families to find those with desired capabilities
 *
 * @param physical_device Device to check queue families for
 * @param desired_queue_capabilities Required queue capabilities
 * @param desired_surface
 * @return
 */
[[nodiscard]] std::vector<types::VulkanQueueFamilyIdx> filter_available_queue_families(
	VkPhysicalDevice const & physical_device,
	VkQueueFlagBits desired_queue_capabilities,
	types::VulkanSurfacePtr const & desired_surface = nullptr);

/**
 * Given a device and set of desired memory properties, filter to only those memory types that
 * are supported by the device and have the desired properties.
 *
 * @param logger
 * @param physical_device Device to check memory types for.
 * @param memory_flags Required memory properties.
 * @return
 */
[[nodiscard]] std::vector<types::VulkanMemoryTypeIdx> filter_available_memory_types(
	LoggerPtr const & logger, VkPhysicalDevice physical_device, VkMemoryPropertyFlags memory_flags);

/**
 * Get a list of all physical devices.
 *
 * @param logger
 * @param instance
 * @return
 */
std::vector<VkPhysicalDevice> enumerate_physical_devices(
	LoggerPtr const & logger, types::VulkanInstancePtr const & instance);

/**
 * Create vulkan surface compatible with SDL window to render to.
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
 * Query available layers vs. desired layers.

 * @param logger
 * @param desired_layer_names
 * @return
 */
std::vector<types::AvailableInstanceLayerNameCstr> filter_available_layers(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names);

/**
 * Query available generic instance extensions vs. desired..
 *
 * @param logger
 * @param desired_extension_names
 * @return
 */
std::vector<types::AvailableInstanceExtensionNameCstr> filter_available_instance_extensions(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names);

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

void log_layer_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::set<types::AvailableInstanceLayerNameView> const & available_layer_names,
	std::span<VkLayerProperties const> available_layer_descs);

constexpr auto make_instance_layer_filter(
	AUTO(LoggerPtr) logger, AUTO(std::set<types::DesiredInstanceLayerNameView>) desired_layer_names)
{
	return [logger = FW(logger),
			desired_layer_names = FW(desired_layer_names)](auto && available_layer_descs)
	{
		auto const available_layer_names = available_layer_descs |
			std::views::transform(&VkLayerProperties::layerName) |
			ranges::to<std::set<types::AvailableInstanceLayerNameView>>;

		log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

		// Get intersection of desired layers and available layers, converted to C strings.
		return ranges::views::set_intersection(
				   desired_layer_names | hof::views::value_of(),
				   available_layer_names | hof::views::value_of()) |
			ranges::views::transform(&std::string_view::data) |
			ranges::to<std::vector<types::AvailableInstanceLayerNameCstr>>;
	};
}

/**
 * Log desired instance extensions vs. available.
 *
 * @param logger
 * @param desired_extension_names
 * @param available_extension_names
 * @param available_extension_properties
 */
void log_instance_extensions_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	std::set<types::AvailableInstanceExtensionNameView> const & available_extension_names,
	std::span<VkExtensionProperties const> available_extension_properties);

constexpr auto make_instance_extension_filter(
	AUTO(LoggerPtr) logger,
	AUTO(std::set<types::DesiredInstanceExtensionNameView>) desired_extension_names)
{
	return [logger = FW(logger), desired_extension_names = FW(desired_extension_names)](
			   AUTO(std::vector<VkExtensionProperties>) available_extensions)
	{
		std::set const available_extension_names = available_extensions |
			std::views::transform(&VkExtensionProperties::extensionName) |
			ranges::to<std::set<types::AvailableInstanceExtensionNameView>>;

		log_instance_extensions_info(
			logger, desired_extension_names, available_extension_names, FW(available_extensions));

		// Intersection of available extensions and desired extensions to return.
		return ranges::views::set_intersection(
				   desired_extension_names | hof::views::value_of(),
				   available_extension_names | hof::views::value_of()) |
			std::views::transform(&std::string_view::data) |
			ranges::to<std::vector<types::AvailableInstanceExtensionNameCstr>>;
	};
}

namespace io
{
using monad::io::IO;

constexpr auto create_minimal_pipeline_layout()
{
	// kleisli
	return [](auto && device)
	{ return IO{[device = FW(device)] { return setup::create_minimal_pipeline_layout(device); }}; };
}

// kleisli
constexpr auto create_semaphore(auto && device)
{
	return IO{[device = FW(device)] { return setup::create_semaphore(device); }};
}

constexpr auto create_primary_command_buffers(types::VulkanCommandBufferCount const count)
{
	// kleisli
	return [count](auto && device, auto && pool)
	{
		return IO{[device = FW(device), pool = FW(pool), count]
				  { return setup::create_primary_command_buffers(device, pool, count); }};
	};
}

constexpr auto create_command_pool(types::VulkanQueueFamilyIdx const queue_family_idx)
{
	// kleisli
	return [queue_family_idx](auto && device)
	{
		return IO{[device = FW(device), queue_family_idx]
				  { return setup::create_command_pool(device, queue_family_idx); }};
	};
}

constexpr auto create_per_image_frame_buffers(VkExtent2D const size)
{
	// kleisli
	return [size](auto && device, auto && render_pass, auto && image_views)
	{
		return IO{[device = FW(device),
				   render_pass = FW(render_pass),
				   image_views = FW(image_views),
				   size]
				  {
					  return setup::create_per_image_frame_buffers(
						  device, render_pass, image_views, size);
				  }};
	};
}

constexpr auto create_single_presentation_subpass_render_pass(VkFormat const surface_format)
{
	// kleisli
	return [surface_format](auto && device)
	{
		return IO{[device = FW(device), surface_format]
				  {
					  return setup::create_single_presentation_subpass_render_pass(
						  surface_format, device);
				  }};
	};
}

constexpr auto create_exclusive_double_buffer_swapchain_and_image_views(
	auto && logger, VkFormat const surface_format)
{
	// kleisli
	return [logger = FW(logger), surface_format](
			   auto && physical_device, auto && device, auto && surface, auto && previous_swapchain)
	{
		return IO{
			[logger = FW(logger),
			 surface_format,
			 physical_device = FW(physical_device),
			 device = FW(device),
			 surface = FW(surface),
			 previous_swapchain = FW(previous_swapchain)]
			{
				return setup::create_exclusive_double_buffer_swapchain_and_image_views(
					logger, physical_device, device, surface, surface_format, previous_swapchain);
			}};
	};
}

constexpr auto create_device_and_queues(
	auto && physical_device, auto && queue_family_and_counts, auto && device_extension_names)
{
	// kleisli
	return IO{[queue_family_and_counts = FW(queue_family_and_counts),
			   device_extension_names = FW(device_extension_names),
			   physical_device = FW(physical_device)]
			  {
				  return setup::create_device_and_queues(
					  physical_device, queue_family_and_counts, device_extension_names);
			  }};
}

constexpr auto filter_available_surface_formats(
	auto && logger, auto && physical_device, auto && surface, auto && desired_formats)
{
	return IO{[logger = FW(logger),
			   desired_formats = FW(desired_formats),
			   physical_device = FW(physical_device),
			   surface = FW(surface)]
			  {
				  return setup::filter_available_surface_formats(
					  logger, physical_device, surface, desired_formats);
			  }};
}

constexpr auto select_physical_device(
	auto && logger,
	auto && physical_devices,
	auto && required_device_extensions,
	VkQueueFlagBits required_queue_capabilities,
	VkMemoryPropertyFlags required_memory_type = 0,
	types::VulkanSurfacePtr required_surface_support = nullptr)
{
	return IO{[logger = FW(logger),
			   physical_devices = FW(physical_devices),
			   required_device_extensions = FW(required_device_extensions),
			   required_queue_capabilities,
			   required_memory_type,
			   required_surface_support = std::move(required_surface_support)]
			  {
				  return setup::select_physical_device(
					  logger,
					  physical_devices,
					  required_device_extensions,
					  required_queue_capabilities,
					  required_memory_type,
					  required_surface_support);
			  }};
}

constexpr auto filter_available_device_extensions(
	auto && logger, auto && physical_device, auto && desired_device_extension_names)
{
	return IO{[logger = FW(logger),
			   desired_device_extension_names = FW(desired_device_extension_names),
			   physical_device = FW(physical_device)]
			  {
				  return setup::filter_available_device_extensions(
					  logger, physical_device, desired_device_extension_names);
			  }};
}

constexpr auto filter_available_queue_families(
	auto && physical_device,
	VkQueueFlagBits const desired_queue_capabilities,
	auto && desired_surface)
{
	return IO{[desired_queue_capabilities,
			   physical_device = FW(physical_device),
			   desired_surface = FW(desired_surface)]
			  {
				  return setup::filter_available_queue_families(
					  physical_device, desired_queue_capabilities, desired_surface);
			  }};
}
constexpr auto filter_available_memory_types(
	auto && logger, auto && physical_device, VkMemoryPropertyFlags memory_flags)
{
	return IO{
		[memory_flags, logger = FW(logger), physical_device = FW(physical_device)]
		{ return setup::filter_available_memory_types(logger, physical_device, memory_flags); }};
}

constexpr auto enumerate_physical_devices(auto && logger, auto && instance)
{
	return IO{[logger = FW(logger), instance = FW(instance)]
			  { return setup::enumerate_physical_devices(logger, instance); }};
}

constexpr auto create_surface(auto && window, auto && instance)
{
	return IO{[window = FW(window), instance = FW(instance)]
			  { return setup::create_surface(window, instance); }};
}
constexpr auto create_debug_messenger(auto && logger, auto && instance)
{
	return IO{[logger = FW(logger), instance = FW(instance)]
			  { return setup::create_debug_messenger(logger, instance); }};
}

constexpr auto create_vulkan_instance(
	auto && logger, auto && window, auto && layers_to_enable, auto && extensions_to_enable)
{
	return IO{[logger = FW(logger),
			   layers_to_enable = FW(layers_to_enable),
			   extensions_to_enable = FW(extensions_to_enable),
			   window = FW(window)]
			  {
				  return setup::create_vulkan_instance(
					  logger, window, layers_to_enable, extensions_to_enable);
			  }};
}

constexpr auto query_available_instance_layers()
{
	return IO{[]
			  {
				  std::vector<VkLayerProperties> out;
				  uint32_t available_layers_count = 0;
				  VK_CHECK(
					  vkEnumerateInstanceLayerProperties(&available_layers_count, nullptr),
					  "Failed to enumerate instance layers");
				  out.resize(available_layers_count);
				  VK_CHECK(
					  vkEnumerateInstanceLayerProperties(&available_layers_count, out.data()),
					  "Failed to enumerate instance layers");

				  return out;
			  }};
}

constexpr auto filter_available_instance_layers(
	AUTO(LoggerPtr) logger, AUTO(std::set<types::DesiredInstanceLayerNameView>) desired_layer_names)
{
	return query_available_instance_layers().fmap(
		make_instance_layer_filter(FW(logger), FW(desired_layer_names)));
}

template <class T, typename R>
concept IOTo = requires(T io)
{
	{
		io()
	} -> std::convertible_to<R>;
};

template <typename F, typename A>
concept MappingFrom = requires(F func, A value)
{
	{func(value)};
};

constexpr auto query_available_instance_extensions()
{
	return IO{[]
			  {
				  std::vector<VkExtensionProperties> out;
				  uint32_t available_extensions_count = 0;
				  VK_CHECK(
					  vkEnumerateInstanceExtensionProperties(
						  nullptr, &available_extensions_count, nullptr),
					  "Failed to enumerate instance extensions");
				  out.resize(available_extensions_count);
				  VK_CHECK(
					  vkEnumerateInstanceExtensionProperties(
						  nullptr, &available_extensions_count, out.data()),
					  "Failed to enumerate instance extensions");

				  return out;
			  }};
}

constexpr auto filter_available_instance_extensions(auto && logger, auto && desired_extension_names)
{
	return IO{
		[logger = FW(logger), desired_extension_names = FW(desired_extension_names)]
		{ return setup::filter_available_instance_extensions(logger, desired_extension_names); }};
}

constexpr auto window_drawable_size()
{
	return [](auto && window)
	{ return IO{[window = FW(window)] { return setup::window_drawable_size(window); }}; };
}

constexpr auto create_window(char const * title, int width, int height)
{
	return IO{[title, width, height] { return setup::create_window(title, width, height); }};
}

}  // namespace io

namespace stateio
{
using monad::stateio::StateIO;

constexpr auto create_window(char const * title, int width, int height)
{
	return StateIO{[title, width, height](auto && state)
				   {
					   return io::create_window(title, width, height)
						   .fmap(
							   [state = FW(state)](auto && window)
							   {
								   struct S : std::decay_t<decltype(state)>
								   {
									   types::SDLWindowPtr window;
								   };
								   return std::pair{window, S{FW(state), FW(window)}};
							   });
				   }};
}

constexpr auto create_vulkan_instance(auto && layers_to_enable, auto && extensions_to_enable)
{
	return StateIO{
		[layers_to_enable = FW(layers_to_enable),
		 extensions_to_enable = FW(extensions_to_enable)](auto && state)
		{
			return io::create_vulkan_instance(
					   state.logger, state.window, layers_to_enable, extensions_to_enable)
				.fmap(
					[state = FW(state)](auto && instance)
					{
						struct S : std::decay_t<decltype(state)>
						{
							types::VulkanInstancePtr instance;
						};
						return std::pair{FW(instance), S{FW(state), instance}};
					});
		}};
}

constexpr auto create_debug_messenger(auto && instance)
{
	return StateIO{[instance = FW(instance)](auto && state)
				   {
					   return io::create_debug_messenger(state.logger, instance)
						   .fmap(
							   [state = FW(state)](auto && messenger)
							   {
								   struct S : std::decay_t<decltype(state)>
								   {
									   types::VulkanDebugMessengerPtr messenger;
								   };
								   return std::pair{messenger, S{FW(state), FW(messenger)}};
							   });
				   }};
}
}  // namespace stateio

}  // namespace vulkandemo::setup