// SPDX-License-Identifier: MIT
// Copyright 2024-2025 David Feltell
#pragma once
#include <cstdint>
#include <range/v3/range/conversion.hpp>
#include <ranges>
#include <set>
#include <span>

#include <type_traits>
#include <utility>
#include <vector>

#include <spdlog/common.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../macros.hpp"
#include "../monad.hpp"
#include "../setup.hpp"
#include "../types.hpp"
#include "filters.hpp"
#include "io.hpp"

namespace vulkandemo::setup::monad
{
namespace io
{
using vulkandemo::monad::bind;
using vulkandemo::monad::io::IO;

// kleisli
constexpr auto create_minimal_pipeline_layout(AUTO(types::VulkanDevicePtr) device)
{
	return IO{[device = FW(device)] { return create_minimal_pipeline_layout(device); }};
}

// kleisli
constexpr auto create_semaphore(AUTO(types::VulkanDevicePtr) device)
{
	return IO{[device = FW(device)] { return create_semaphore(device); }};
}

constexpr auto create_primary_command_buffers(types::VulkanCommandBufferCount const count)
{
	// kleisli
	return [count](AUTO(types::VulkanDevicePtr) device, AUTO(types::VulkanCommandPoolPtr) pool)
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
				return create_exclusive_double_buffer_swapchain_and_image_views(
					logger, physical_device, device, surface, surface_format, previous_swapchain);
			}};
	};
}

constexpr auto create_device_and_queues(
	auto && physical_device, auto && queue_family_and_counts, auto && device_extension_names)
{
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

constexpr auto query_available_device_extensions(VkPhysicalDevice physical_device)
{
	return IO{[physical_device]
			  { return setup::enumerate_physical_device_extension_properties(physical_device); }};
}

[[deprecated]] constexpr auto filter_available_device_extensions(
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

[[deprecated]] constexpr auto filter_queue_family_idxs_by_surface_support(
	VkPhysicalDevice physical_device,
	AUTO(std::vector<types::VulkanQueueFamilyIdx>) queue_family_idxs,
	AUTO(types::VulkanSurfacePtr) desired_surface)
{
	return IO{[physical_device,
			   desired_surface = FW(desired_surface),
			   queue_family_idxs = FW(queue_family_idxs)]
			  {
				  return queue_family_idxs |
					  std::views::filter(
							 [&](auto const queue_family_idx)
							 {
								 VkBool32 surface_supported = VK_FALSE;
								 VK_CHECK(
									 vkGetPhysicalDeviceSurfaceSupportKHR(
										 physical_device,
										 queue_family_idx,
										 desired_surface.get(),
										 &surface_supported),
									 "Failed to check surface support");
								 return surface_supported == VK_TRUE;
							 }) |
					  ranges::to<std::vector>;
			  }};
}

constexpr auto make_query_is_queue_family_supported_by_physical_device_and_surface(
	VkPhysicalDevice physical_device, AUTO(types::VulkanSurfacePtr) desired_surface)
{
	return [physical_device, desired_surface = FW(desired_surface)](
			   types::VulkanQueueFamilyIdx const queue_family_idx)
	{
		return IO{[physical_device, desired_surface, queue_family_idx]
				  {
					  VkBool32 surface_supported = VK_FALSE;
					  VK_CHECK(
						  vkGetPhysicalDeviceSurfaceSupportKHR(
							  physical_device,
							  queue_family_idx,
							  desired_surface.get(),
							  &surface_supported),
						  "Failed to check surface support");
					  return surface_supported == VK_TRUE;
				  }};
	};
}

constexpr auto query_physical_device_properties(VkPhysicalDevice physical_device)
{
	return IO{[physical_device]
			  { return setup::query_physical_device_properties(physical_device); }};
}

constexpr auto query_available_queue_family_properties(VkPhysicalDevice physical_device)
{
	return IO{[physical_device]
			  {
				  std::vector<VkQueueFamilyProperties> out;
				  uint32_t queue_family_count = 0;
				  vkGetPhysicalDeviceQueueFamilyProperties(
					  physical_device, &queue_family_count, nullptr);
				  out.resize(queue_family_count);
				  vkGetPhysicalDeviceQueueFamilyProperties(
					  physical_device, &queue_family_count, out.data());
				  return out;
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

constexpr auto query_surface_capabilities(
	VkPhysicalDevice physical_device, AUTO(types::VulkanSurfacePtr) surface)
{
	return IO{[physical_device, surface = FW(surface)]
			  { return setup::query_surface_capabilities(physical_device, surface); }};
}

constexpr auto query_present_modes(
	VkPhysicalDevice physical_device, AUTO(types::VulkanSurfacePtr) surface)
{
	return IO{[physical_device, surface = FW(surface)]
			  { return setup::query_present_modes(physical_device, surface); }};
}

constexpr auto create_swapchain(
	AUTO(types::VulkanDevicePtr) device, VkSwapchainCreateInfoKHR const & create_info)
{
	return IO{[device = FW(device), create_info]
			  { return setup::create_swapchain(device, create_info); }};
}

[[deprecated]] constexpr auto filter_available_memory_types(
	auto && logger, auto && physical_device, VkMemoryPropertyFlags memory_flags)
{
	return IO{
		[memory_flags, logger = FW(logger), physical_device = FW(physical_device)]
		{ return setup::filter_available_memory_types(logger, physical_device, memory_flags); }};
}

constexpr auto query_physical_device_memory_properties(
	AUTO(LoggerPtr) logger, VkPhysicalDevice physical_device)
{
	return IO{[logger = FW(logger), physical_device]
			  {
				  VkPhysicalDeviceMemoryProperties memory_properties;
				  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

				  if (logger->should_log(spdlog::level::debug))
				  {
					  VkPhysicalDeviceProperties device_properties;
					  vkGetPhysicalDeviceProperties(physical_device, &device_properties);

					  logger->debug(
						  "Memory types for {}:\n\t{}",
						  device_properties.deviceName,
						  fmt::join(
							  std::views::enumerate(
								  std::span{memory_properties.memoryTypes}.subspan(
									  0, memory_properties.memoryTypeCount)) |
								  std::views::transform(
									  [](auto const & idx_and_memory_type)
									  {
										  return fmt::format(
											  "{}: {}",
											  std::get<0>(idx_and_memory_type),
											  string_VkMemoryPropertyFlags(
												  std::get<1>(idx_and_memory_type).propertyFlags));
									  }),
							  "\n\t"));
				  }

				  return memory_properties;
			  }};
}

constexpr auto enumerate_physical_devices(
	AUTO(LoggerPtr) logger, AUTO(types::VulkanInstancePtr) instance)
{
	return IO{[logger = FW(logger), instance = FW(instance)]
			  { return setup::enumerate_physical_devices(logger, instance); }};
}

constexpr auto create_surface(
	AUTO(types::SDLWindowPtr) window, AUTO(types::VulkanInstancePtr) instance)
{
	return IO{[window = FW(window), instance = FW(instance)]
			  { return setup::create_surface(window, instance); }};
}

constexpr auto create_debug_messenger(
	AUTO(LoggerPtr) logger, AUTO(types::VulkanInstancePtr) instance)
{
	return IO{[logger = FW(logger), instance = FW(instance)]
			  { return setup::create_debug_messenger(logger, instance); }};
}

constexpr auto create_instance(
	AUTO(LoggerPtr) logger,
	char const * name,
	types::ContiguousContainerOf<types::AvailableInstanceLayerNameCstr> auto && layers_to_enable,
	types::ContiguousContainerOf<types::AvailableInstanceExtensionNameCstr> auto &&
		extensions_to_enable)
{
	return IO{
		[logger = FW(logger),
		 name,
		 layers_to_enable = FW(layers_to_enable),
		 extensions_to_enable = FW(extensions_to_enable)]
		{
			auto const layers_to_enable_cstr =
				layers_to_enable | hof::views::value_of() | ranges::to<std::vector<char const *>>;
			auto const extensions_to_enable_cstr = extensions_to_enable | hof::views::value_of() |
				ranges::to<std::vector<char const *>>;

			logger->debug(
				"Enabling instance extensions: {}", fmt::join(layers_to_enable_cstr, ", "));
			logger->debug("Enabling layers: {}", fmt::join(extensions_to_enable_cstr, ", "));

			// Application metadata.
			VkApplicationInfo const app_info = {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = name,
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = name,
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_API_VERSION_1_3,
			};

			VkInstanceCreateInfo const create_info = {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &app_info,
				.enabledLayerCount = static_cast<uint32_t>(layers_to_enable_cstr.size()),
				.ppEnabledLayerNames = layers_to_enable_cstr.data(),
				.enabledExtensionCount = static_cast<uint32_t>(extensions_to_enable_cstr.size()),
				.ppEnabledExtensionNames = extensions_to_enable_cstr.data(),
			};

			VkInstance out = nullptr;
			VK_CHECK(
				vkCreateInstance(&create_info, nullptr, &out), "Failed to create Vulkan instance");

			return types::make_instance_ptr(out);
		}};
}

constexpr auto query_sdl_instance_extension_names(AUTO(types::SDLWindowPtr) sdl_window)
{
	return IO{[sdl_window = FW(sdl_window)]
			  {
				  std::vector<char const *> out;
				  uint32_t extension_count = 0;
				  SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, nullptr);
				  out.resize(extension_count);
				  SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, out.data());
				  return hof::views::cast<types::AvailableInstanceExtensionNameCstr>(out) |
					  ranges::to<std::vector>;
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

constexpr auto window_title(AUTO(types::SDLWindowPtr) window)
{
	return IO{[window = FW(window)] { return SDL_GetWindowTitle(window.get()); }};
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

constexpr auto query_available_surface_formats(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface)
{
	return IO{
		[physical_device, surface = FW(surface)]
		{ return setup::enumerate_physical_device_surface_formats(physical_device, surface); }};
}

constexpr auto log_surface_format_selection(
	LoggerPtr const & logger,
	auto && filtered_surface_formats,
	auto && available_surface_formats,
	auto && desired_formats)
{
	return IO{[logger = FW(logger),
			   filtered_surface_formats = FW(filtered_surface_formats),
			   available_surface_formats = FW(available_surface_formats),
			   desired_formats = FW(desired_formats)]
			  {
				  setup::log_surface_format_selection(
					  logger, filtered_surface_formats, available_surface_formats, desired_formats);
			  }};
}

}  // namespace io

namespace stateio
{
using vulkandemo::monad::stateio::StateIO;

constexpr auto create_surface()
{
	return StateIO{[](auto && state)
				   {
					   return io::create_surface(state.window, state.instance)
						   .fmap(
							   [state = FW(state)](auto && surface)
							   {
								   struct S : std::decay_t<decltype(state)>
								   {
									   types::VulkanSurfacePtr surface;
								   };
								   return std::pair{FW(surface), S{state, surface}};
							   });
				   }};
}

constexpr auto create_instance(
	char const * app_name,
	types::ContiguousContainerOf<types::AvailableInstanceLayerNameCstr> auto && layers_to_enable,
	types::ContiguousContainerOf<types::AvailableInstanceExtensionNameCstr> auto &&
		extensions_to_enable)
{
	return StateIO{[app_name,
					layers_to_enable = FW(layers_to_enable),
					extensions_to_enable = FW(extensions_to_enable)](auto && state)
				   {
					   return io::create_instance(
								  state.logger, app_name, layers_to_enable, extensions_to_enable)
						   .fmap(
							   [state = FW(state)](auto && instance)
							   {
								   struct S : std::decay_t<decltype(state)>
								   {
									   types::VulkanInstancePtr instance;
								   };
								   return std::pair{FW(instance), S{state, instance}};
							   });
				   }};
}

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
								   return std::pair{window, S{state, FW(window)}};
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
								   return std::pair{messenger, S{state, FW(messenger)}};
							   });
				   }};
}
}  // namespace stateio

}  // namespace vulkandemo::setup::monad