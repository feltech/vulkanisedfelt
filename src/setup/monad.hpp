// SPDX-License-Identifier: MIT
// Copyright 2024-2025 David Feltell
#pragma once
#include <SDL_video.h>
#include <cstdint>
#include <range/v3/range/conversion.hpp>
#include <ranges>
#include <set>
#include <span>

#include <string>
#include <tuple>
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
using vulkandemo::monad::bind;
using vulkandemo::monad::io::IO;
using vulkandemo::monad::stateio::StateIO;

// IO monad lifter for creating a minimal pipeline layout (bind-like)
struct create_minimal_pipeline_layout_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		auto operator()() const
		{
			return setup::create_minimal_pipeline_layout(device);
		}
	};

	static constexpr auto make_io(types::VulkanDevicePtr device)
	{
		return IO{io_action_t{std::move(device)}};
	}
};

// IO monad lifter for creating a semaphore (bind-like)
struct create_semaphore_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		auto operator()() const
		{
			return setup::create_semaphore(device);
		}
	};

	static constexpr auto make_io(types::VulkanDevicePtr device)
	{
		return IO{io_action_t{std::move(device)}};
	}
};

// IO monad lifter for creating primary command buffers (bind-like)
struct create_primary_command_buffers_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		types::VulkanCommandPoolPtr pool;
		types::VulkanCommandBufferCount count;
		auto operator()() const
		{
			return setup::create_primary_command_buffers(device, pool, count);
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device,
		types::VulkanCommandPoolPtr pool,
		types::VulkanCommandBufferCount count)
	{
		return IO{io_action_t{std::move(device), std::move(pool), count}};
	}
};

// IO monad lifter for creating a command pool (bind-like)
struct create_command_pool_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		types::VulkanQueueFamilyIdx queue_family_idx;
		auto operator()() const
		{
			return setup::create_command_pool(device, queue_family_idx);
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device, types::VulkanQueueFamilyIdx queue_family_idx)
	{
		return IO{io_action_t{std::move(device), queue_family_idx}};
	}

	struct with_queue_family_idx_t
	{
		types::VulkanQueueFamilyIdx queue_family_idx;
		auto operator()(types::VulkanDevicePtr device) const
		{
			return make_io(std::move(device), queue_family_idx);
		}
	};
};

struct create_per_image_frame_buffers_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		types::VulkanRenderPassPtr render_pass;
		std::vector<types::VulkanImageViewPtr> image_views;
		VkExtent2D size;
		auto operator()() const
		{
			return setup::create_per_image_frame_buffers(
				device, render_pass, std::span<types::VulkanImageViewPtr const>{image_views}, size);
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device,
		types::VulkanRenderPassPtr render_pass,
		std::vector<types::VulkanImageViewPtr> image_views,
		VkExtent2D size)
	{
		return IO{
			io_action_t{std::move(device), std::move(render_pass), std::move(image_views), size}};
	}

	struct with_size_t
	{
		VkExtent2D size;
		auto operator()(
			types::VulkanDevicePtr device,
			types::VulkanRenderPassPtr render_pass,
			std::vector<types::VulkanImageViewPtr> image_views) const
		{
			return make_io(std::move(device), std::move(render_pass), std::move(image_views), size);
		}
	};
};

// IO monad lifter for creating a single presentation subpass render pass (bind-like)
struct create_single_presentation_subpass_render_pass_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		VkFormat surface_format;
		auto operator()() const
		{
			return setup::create_single_presentation_subpass_render_pass(surface_format, device);
		}
	};

	static constexpr auto make_io(types::VulkanDevicePtr device, VkFormat const surface_format)
	{
		return IO{io_action_t{std::move(device), surface_format}};
	}

	struct with_surface_format_t
	{
		VkFormat surface_format;
		auto operator()(types::VulkanDevicePtr device) const
		{
			return make_io(std::move(device), surface_format);
		}
	};
};

struct create_exclusive_double_buffer_swapchain_and_image_views_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		VkPhysicalDevice physical_device{};
		types::VulkanDevicePtr device;
		types::VulkanSurfacePtr surface;
		VkSurfaceFormatKHR surface_format{};
		types::VulkanSwapchainPtr previous_swapchain;
		auto operator()() const
		{
			return setup::create_exclusive_double_buffer_swapchain_and_image_views(
				logger, physical_device, device, surface, surface_format, previous_swapchain);
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger,
		VkPhysicalDevice physical_device,
		types::VulkanDevicePtr device,
		types::VulkanSurfacePtr surface,
		VkSurfaceFormatKHR surface_format,
		types::VulkanSwapchainPtr previous_swapchain = nullptr)
	{
		return IO{io_action_t{
			std::move(logger),
			physical_device,
			std::move(device),
			std::move(surface),
			surface_format,
			std::move(previous_swapchain)}};
	}

	struct with_logger_and_surface_format_t
	{
		LoggerPtr logger;
		VkSurfaceFormatKHR surface_format{};
		auto operator()(
			VkPhysicalDevice physical_device,
			types::VulkanDevicePtr device,
			types::VulkanSurfacePtr surface,
			types::VulkanSwapchainPtr previous_swapchain = nullptr) const
		{
			return make_io(
				LoggerPtr{logger},
				physical_device,
				std::move(device),
				std::move(surface),
				surface_format,
				std::move(previous_swapchain));
		}
	};
};

// IO monad lifter for creating device and queues (bind-like)
struct create_device_and_queues_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts;
		std::vector<types::AvailableDeviceExtensionNameView> device_extension_names;
		auto operator()() const
		{
			return setup::create_device_and_queues(
				physical_device,
				std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>{
					queue_family_and_counts},
				std::span<types::AvailableDeviceExtensionNameView const>{device_extension_names});
		}
	};

	static constexpr auto make_io(
		VkPhysicalDevice physical_device,
		std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
			queue_family_and_counts,
		std::span<types::AvailableDeviceExtensionNameView const> device_extension_names)
	{
		return IO{io_action_t{
			physical_device,
			std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>{
				queue_family_and_counts.begin(), queue_family_and_counts.end()},
			std::vector<types::AvailableDeviceExtensionNameView>{
				device_extension_names.begin(), device_extension_names.end()}}};
	}
};

// IO monad lifter for creating a Vulkan device (bind-like)
struct create_device_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts;
		std::vector<types::AvailableDeviceExtensionNameView> device_extension_names;
		auto operator()() const
		{
			return setup::create_device(
				physical_device, queue_family_and_counts, device_extension_names);
		}
	};

	static constexpr auto make_io(
		VkPhysicalDevice physical_device,
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts,
		std::vector<types::AvailableDeviceExtensionNameView> device_extension_names)
	{
		return IO{io_action_t{
			physical_device,
			std::move(queue_family_and_counts),
			std::move(device_extension_names)}};
	}
	static constexpr auto make_stateio(
		VkPhysicalDevice physical_device,
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts,
		std::vector<types::AvailableDeviceExtensionNameView> device_extension_names)
	{
		return StateIO{stateio_action_t{
			.physical_device = physical_device,
			.queue_family_and_counts = std::move(queue_family_and_counts),
			.device_extension_names = std::move(device_extension_names)}};
	}

	struct stateio_action_t
	{
		VkPhysicalDevice physical_device;
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts;
		std::vector<types::AvailableDeviceExtensionNameView> device_extension_names;

		constexpr auto operator()(auto && state) const
		{
			return make_io(physical_device, queue_family_and_counts, device_extension_names)
				.fmap(add_device_to_state_t{FW(state)});
		}
	};

	template <class State>
	struct add_device_to_state_t
	{
		State state;
		constexpr auto operator()(types::VulkanDevicePtr const & device) const
		{
			struct S : std::decay_t<decltype(state)>
			{
				types::VulkanDevicePtr device;
			};
			return std::pair{FW(device), S{state, device}};
		}
	};
};

struct query_queues_for_queue_family_and_counts_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts;
		auto operator()() const
		{
			return setup::query_queues_for_queue_family_and_counts(
				device.get(),
				std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>{
					queue_family_and_counts});
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device,
		std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
			queue_family_and_counts)
	{
		return IO{io_action_t{
			std::move(device),
			std::vector<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>{
				queue_family_and_counts.begin(), queue_family_and_counts.end()}}};
	}
};

struct query_swapchain_images_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		types::VulkanSwapchainPtr swapchain;
		auto operator()() const
		{
			return setup::query_swapchain_images(device, swapchain);
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device, types::VulkanSwapchainPtr swapchain)
	{
		return IO{io_action_t{std::move(device), std::move(swapchain)}};
	}
};

// IO monad lifter for creating image views (bind-like)
struct create_colour_aspect_single_mip_single_layer_image_views_t
{
	static constexpr auto make_stateio(
		VkSurfaceFormatKHR surface_format, std::vector<VkImage> images)
	{
		return StateIO{stateio_action_t{surface_format, std::move(images)}};
	}

	struct io_action_t
	{
		types::VulkanDevicePtr device;
		VkSurfaceFormatKHR surface_format{};
		std::vector<VkImage> images;
		auto operator()() const
		{
			return setup::create_colour_aspect_single_mip_single_layer_image_views(
				device, surface_format, std::span<VkImage const>{images});
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device,
		VkSurfaceFormatKHR surface_format,
		std::span<VkImage const> images)
	{
		return IO{io_action_t{
			std::move(device), surface_format, std::vector<VkImage>{images.begin(), images.end()}}};
	}

	struct stateio_action_t
	{
		VkSurfaceFormatKHR surface_format;
		std::vector<VkImage> images;
		constexpr auto operator()(auto const & state) const
		{
			return make_io(state.device, surface_format, images)
				.fmap(add_image_views_to_state_t{state});
		}
	};

	template <class State>
	struct add_image_views_to_state_t
	{
		State state;
		constexpr auto operator()(std::vector<types::VulkanImageViewPtr> const & image_views) const
		{
			struct S : std::decay_t<decltype(state)>
			{
				std::vector<types::VulkanImageViewPtr> image_views;
			};
			return std::pair{image_views, S{state, image_views}};
		}
	};
};

// IO monad lifter for filtering available surface formats (bind-like)
struct filter_available_surface_formats_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		VkPhysicalDevice physical_device{};
		types::VulkanSurfacePtr surface;
		std::vector<VkFormat> desired_formats;
		auto operator()() const
		{
			return setup::filter_available_surface_formats(
				logger, physical_device, surface, std::span<VkFormat const>{desired_formats});
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger,
		VkPhysicalDevice physical_device,
		types::VulkanSurfacePtr surface,
		std::span<VkFormat const> desired_formats)
	{
		return IO{io_action_t{
			std::move(logger),
			physical_device,
			std::move(surface),
			std::vector<VkFormat>{desired_formats.begin(), desired_formats.end()}}};
	}
};

// IO monad lifter for selecting a physical device (bind-like)
struct select_physical_device_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		std::vector<VkPhysicalDevice> physical_devices;
		std::set<types::DesiredDeviceExtensionNameView> required_device_extensions;
		VkQueueFlagBits required_queue_capabilities{};
		VkMemoryPropertyFlags required_memory_type{};
		types::VulkanSurfacePtr required_surface_support;
		auto operator()() const
		{
			return setup::select_physical_device(
				logger,
				physical_devices,
				required_device_extensions,
				required_queue_capabilities,
				required_memory_type,
				required_surface_support);
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger,
		std::vector<VkPhysicalDevice> physical_devices,
		std::set<types::DesiredDeviceExtensionNameView> required_device_extensions,
		VkQueueFlagBits required_queue_capabilities,
		VkMemoryPropertyFlags required_memory_type = 0,
		types::VulkanSurfacePtr required_surface_support = nullptr)
	{
		return IO{io_action_t{
			std::move(logger),
			std::move(physical_devices),
			std::move(required_device_extensions),
			required_queue_capabilities,
			required_memory_type,
			std::move(required_surface_support)}};
	}
};

// IO monad lifter for querying available device extensions (bind-like)
struct query_available_device_extensions_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		auto operator()() const
		{
			return setup::enumerate_physical_device_extension_properties(physical_device);
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device)
	{
		return IO{io_action_t{physical_device}};
	}
};

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

// Deprecated: use make_query_is_queue_family_supported_by_physical_device_and_surface_cont_t
// and bind/filter as needed.
[[deprecated]] constexpr auto filter_queue_family_idxs_by_surface_support(
	VkPhysicalDevice physical_device,
	std::vector<types::VulkanQueueFamilyIdx> queue_family_idxs,
	types::VulkanSurfacePtr desired_surface)
{
	return IO{[physical_device,
			   desired_surface = std::move(desired_surface),
			   queue_family_idxs = std::move(queue_family_idxs)]
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

// IO monad lifter for checking if a queue family supports a surface (bind-like)
struct query_is_queue_family_supported_by_physical_device_and_surface_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		types::VulkanSurfacePtr surface;
		types::VulkanQueueFamilyIdx queue_family_idx{};
		auto operator()() const
		{
			VkBool32 surface_supported = VK_FALSE;
			VK_CHECK(
				vkGetPhysicalDeviceSurfaceSupportKHR(
					physical_device, queue_family_idx, surface.get(), &surface_supported),
				"Failed to check surface support");
			return surface_supported == VK_TRUE;
		}
	};

	static constexpr auto make_io(
		VkPhysicalDevice physical_device,
		types::VulkanSurfacePtr surface,
		types::VulkanQueueFamilyIdx queue_family_idx)
	{
		return IO{io_action_t{physical_device, std::move(surface), queue_family_idx}};
	}

	struct with_physical_device_and_surface_t
	{
		VkPhysicalDevice physical_device;
		types::VulkanSurfacePtr surface;
		auto operator()(types::VulkanQueueFamilyIdx const queue_family_idx) const
		{
			return make_io(physical_device, types::VulkanSurfacePtr{surface}, queue_family_idx);
		}
	};
};

// IO monad lifter for querying physical device properties (bind-like)
struct query_physical_device_properties_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		auto operator()() const
		{
			return setup::query_physical_device_properties(physical_device);
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device)
	{
		return IO{io_action_t{physical_device}};
	}
};

struct query_available_queue_family_properties_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		auto operator()() const
		{
			std::vector<VkQueueFamilyProperties> out;
			uint32_t queue_family_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
			out.resize(queue_family_count);
			vkGetPhysicalDeviceQueueFamilyProperties(
				physical_device, &queue_family_count, out.data());
			return out;
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device)
	{
		return IO{io_action_t{physical_device}};
	}
};

struct filter_available_queue_families_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device;
		VkQueueFlagBits desired_queue_capabilities;
		types::VulkanSurfacePtr desired_surface;
		auto operator()() const
		{
			return setup::filter_available_queue_families(
				physical_device, desired_queue_capabilities, desired_surface);
		}
	};
	static constexpr auto make_io(
		VkPhysicalDevice physical_device,
		VkQueueFlagBits const desired_queue_capabilities,
		types::VulkanSurfacePtr desired_surface)
	{
		return IO{
			io_action_t{physical_device, desired_queue_capabilities, std::move(desired_surface)}};
	}
};

struct create_swapchain_t
{
	struct io_action_t
	{
		types::VulkanDevicePtr device;
		VkSwapchainCreateInfoKHR create_info{};
		auto operator()() const
		{
			return setup::create_swapchain(device, create_info);
		}
	};

	static constexpr auto make_io(
		types::VulkanDevicePtr device, VkSwapchainCreateInfoKHR const & create_info)
	{
		return IO{io_action_t{std::move(device), create_info}};
	}

	static constexpr auto make_stateio(VkSwapchainCreateInfoKHR create_info)
	{
		return StateIO{stateio_action_t{std::move(create_info)}};
	}

	struct stateio_action_t
	{
		VkSwapchainCreateInfoKHR create_info;
		constexpr auto operator()(auto && state) const
		{
			return make_io(state.device, create_info).fmap(add_swapchain_to_state_t{state});
		}
	};

	template <class State>
	struct add_swapchain_to_state_t
	{
		State state;
		auto operator()(types::VulkanSwapchainPtr swapchain) const
		{
			struct S : std::decay_t<decltype(state)>
			{
				types::VulkanSwapchainPtr swapchain;
			};
			return std::pair{FW(swapchain), S{state, swapchain}};
		}
	};
};

struct filter_available_memory_types_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		VkPhysicalDevice physical_device;
		VkMemoryPropertyFlags memory_flags;

		auto operator()() const
		{
			return setup::filter_available_memory_types(logger, physical_device, memory_flags);
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger, VkPhysicalDevice physical_device, VkMemoryPropertyFlags memory_flags)
	{
		return IO{io_action_t{std::move(logger), physical_device, memory_flags}};
	}
};

struct query_physical_device_memory_properties_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		VkPhysicalDevice physical_device{};
		auto operator()() const
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
		}
	};

	static constexpr auto make_io(LoggerPtr logger, VkPhysicalDevice physical_device)
	{
		return IO{io_action_t{std::move(logger), physical_device}};
	}
};

// IO monad lifter for querying surface capabilities (bind-like)
struct query_surface_capabilities_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device;
		types::VulkanSurfacePtr surface;
		auto operator()() const
		{
			return setup::query_surface_capabilities(physical_device, surface);
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
	{
		return IO{io_action_t{physical_device, std::move(surface)}};
	}
};

// IO monad lifter for querying present modes (bind-like)
struct query_present_modes_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device;
		types::VulkanSurfacePtr surface;
		auto operator()() const
		{
			return setup::query_present_modes(physical_device, surface);
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
	{
		return IO{io_action_t{physical_device, std::move(surface)}};
	}
};

// IO monad lifter for querying available surface formats (bind-like)
struct query_available_surface_formats_t
{
	struct io_action_t
	{
		VkPhysicalDevice physical_device{};
		types::VulkanSurfacePtr surface;
		auto operator()() const
		{
			return setup::enumerate_physical_device_surface_formats(physical_device, surface);
		}
	};

	static constexpr auto make_io(VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
	{
		return IO{io_action_t{physical_device, std::move(surface)}};
	}
};

struct log_surface_format_selection_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		std::vector<VkSurfaceFormatKHR> filtered_surface_formats;
		std::vector<VkSurfaceFormatKHR> available_surface_formats;
		std::vector<VkFormat> desired_formats;
		auto operator()() const
		{
			setup::log_surface_format_selection(
				logger,
				std::span<VkSurfaceFormatKHR const>{filtered_surface_formats},
				std::span<VkSurfaceFormatKHR const>{available_surface_formats},
				std::span<VkFormat const>{desired_formats});
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger,
		std::span<VkSurfaceFormatKHR const> filtered_surface_formats,
		std::span<VkSurfaceFormatKHR const> available_surface_formats,
		std::span<VkFormat const> desired_formats)
	{
		return IO{io_action_t{
			std::move(logger),
			std::vector<VkSurfaceFormatKHR>{
				filtered_surface_formats.begin(), filtered_surface_formats.end()},
			std::vector<VkSurfaceFormatKHR>{
				available_surface_formats.begin(), available_surface_formats.end()},
			std::vector<VkFormat>{desired_formats.begin(), desired_formats.end()}}};
	}
};

struct enumerate_physical_devices_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		types::VulkanInstancePtr instance;
		auto operator()() const
		{
			return setup::enumerate_physical_devices(logger, instance);
		}
	};

	static constexpr auto make_io(LoggerPtr logger, types::VulkanInstancePtr instance)
	{
		return IO{io_action_t{std::move(logger), std::move(instance)}};
	}
};

struct create_surface_t
{
	struct io_action_t
	{
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
		auto operator()() const
		{
			return setup::create_surface(window, instance);
		}
	};

	static constexpr auto make_io(types::SDLWindowPtr window, types::VulkanInstancePtr instance)
	{
		return IO{io_action_t{.window = std::move(window), .instance = std::move(instance)}};
	}

	static constexpr auto make_stateio()
	{
		return StateIO{stateio_action_t{}};
	}

	struct stateio_action_t
	{
		constexpr auto operator()(auto && state) const
		{
			return make_io(state.window, state.instance).fmap(add_surface_to_state_t{state});
		}
	};

	template <class State>
	struct add_surface_to_state_t
	{
		State state;
		auto operator()(types::VulkanSurfacePtr surface) const
		{
			struct S : std::decay_t<decltype(state)>
			{
				types::VulkanSurfacePtr surface;
			};
			return std::pair{surface, S{state, surface}};
		}
	};
};

struct create_debug_messenger_t
{
	struct io_action_t
	{
		LoggerPtr logger;
		types::VulkanInstancePtr instance;
		auto operator()() const
		{
			return setup::create_debug_messenger(logger, instance);
		}
	};

	static constexpr auto make_io(LoggerPtr logger, types::VulkanInstancePtr instance)
	{
		return IO{io_action_t{std::move(logger), std::move(instance)}};
	}

	static constexpr auto make_stateio(types::VulkanInstancePtr instance)
	{
		return StateIO{stateio_action_t{std::move(instance)}};
	}

	template <class State>
	struct add_debug_messenger_to_state_t
	{
		State state;

		auto operator()(types::VulkanDebugMessengerPtr messenger) const
		{
			struct S : State
			{
				types::VulkanDebugMessengerPtr messenger;
			};
			return std::pair{messenger, S{state, messenger}};
		}
	};

	struct stateio_action_t
	{
		types::VulkanInstancePtr instance;

		auto operator()(auto const & state) const
		{
			return make_io(state.logger, instance).fmap(add_debug_messenger_to_state_t{state});
		}
	};
};

struct create_instance_t
{
	// IO monad lifter for creating a Vulkan instance
	struct io_action_t
	{
		LoggerPtr logger;
		std::string name;
		std::vector<types::AvailableInstanceLayerNameCstr> layers_to_enable;
		std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable;

		auto operator()() const
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
				.pApplicationName = name.c_str(),
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = name.c_str(),
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
		}
	};

	static constexpr auto make_io(
		LoggerPtr logger,
		std::string name,
		std::vector<types::AvailableInstanceLayerNameCstr> layers_to_enable,
		std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
	{
		return IO{io_action_t{
			.logger = std::move(logger),
			.name = std::move(name),
			.layers_to_enable = std::move(layers_to_enable),
			.extensions_to_enable = std::move(extensions_to_enable)}};
	}

	struct with_logger_t
	{
		LoggerPtr logger;
		constexpr auto operator()(
			std::string name,
			std::vector<types::AvailableInstanceLayerNameCstr> layers_to_enable,
			std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable) const
		{
			return IO{io_action_t{
				.logger = logger,
				.name = std::move(name),
				.layers_to_enable = std::move(layers_to_enable),
				.extensions_to_enable = std::move(extensions_to_enable)}};
		}
	};

	template <class State>
	struct add_instance_to_state_t
	{
		State state;

		constexpr auto operator()(types::VulkanInstancePtr instance) const
		{
			struct S : std::decay_t<decltype(state)>
			{
				types::VulkanInstancePtr instance;
			};
			return std::pair{instance, S{state, instance}};
		}
	};

	struct stateio_action_t
	{
		std::string name;
		std::vector<types::AvailableInstanceLayerNameCstr> layers_to_enable;
		std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable;

		constexpr auto operator()(auto const & state) const
		{
			return make_io(state.logger, name, layers_to_enable, extensions_to_enable)
				.fmap(add_instance_to_state_t{state});
		}
	};

	static constexpr auto make_stateio(
		std::string name,
		std::vector<types::AvailableInstanceLayerNameCstr> layers_to_enable,
		std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
	{
		return StateIO{stateio_action_t{
			.name = std::move(name),
			.layers_to_enable = std::move(layers_to_enable),
			.extensions_to_enable = std::move(extensions_to_enable)}};
	}
};

struct query_sdl_instance_extension_names_t
{
	struct io_action_t
	{
		types::SDLWindowPtr sdl_window;
		std::vector<types::AvailableInstanceExtensionNameCstr> operator()() const
		{
			std::vector<char const *> out;
			uint32_t extension_count = 0;
			SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, nullptr);
			out.resize(extension_count);
			SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, out.data());
			return hof::views::cast<types::AvailableInstanceExtensionNameCstr>(out) |
				ranges::to<std::vector>;
		}
	};

	static constexpr auto make_io(types::SDLWindowPtr sdl_window)
	{
		return IO{io_action_t{std::move(sdl_window)}};
	}
};

struct query_available_instance_layers_t
{
	struct io_action_t
	{
		std::vector<VkLayerProperties> operator()() const
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
		}
	};

	static constexpr auto make_io()
	{
		return IO{io_action_t{}};
	}
};

struct query_available_instance_extensions_t
{
	struct io_action_t
	{
		std::vector<VkExtensionProperties> operator()() const
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
		}
	};

	static constexpr auto make_io()
	{
		return IO{io_action_t{}};
	}
};

struct query_window_title_t
{
	struct io_action_t
	{
		types::SDLWindowPtr window;
		auto operator()() const
		{
			return SDL_GetWindowTitle(window.get());
		}
	};

	static constexpr auto make_io(types::SDLWindowPtr window)
	{
		return IO{io_action_t{std::move(window)}};
	}
};

// IO monad lifter for window drawable size (bind-like)
struct window_drawable_size_t
{
	struct io_action_t
	{
		types::SDLWindowPtr window;
		auto operator()() const
		{
			return setup::window_drawable_size(window);
		}
	};

	static constexpr auto make_io(types::SDLWindowPtr window)
	{
		return IO{io_action_t{std::move(window)}};
	}
};

struct create_window_t
{
	static constexpr auto make_stateio(std::string title, int width, int height)
	{
		using vulkandemo::monad::stateio::lift;

		return StateIO{
			stateio_action_t{.title = std::move(title), .width = width, .height = height}};
	}

	static constexpr auto make_io(std::string title, int const width, int const height)
	{
		return IO{io_action_t{.title = std::move(title), .width = width, .height = height}};
	}

	struct io_action_t
	{
		std::string title;
		int width;
		int height;

		types::SDLWindowPtr operator()() const
		{
			return setup::create_window(title.c_str(), width, height);
		}
	};

	struct stateio_action_t
	{
		std::string title;
		int width;
		int height;

		constexpr auto operator()(auto state) const
		{
			return make_io(title, width, height).fmap(add_window_to_state_t{std::move(state)});
		}
	};

	template <class State>
	struct add_window_to_state_t
	{
		State state;

		constexpr auto operator()(types::SDLWindowPtr window) const
		{
			struct S : State
			{
				types::SDLWindowPtr window;
			};
			return std::pair{window, S{state, window}};
		}
	};
};

}  // namespace vulkandemo::setup::monad