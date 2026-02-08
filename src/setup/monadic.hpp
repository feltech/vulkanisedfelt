// SPDX-License-Identifier: MIT
// Copyright 2024-2025 David Feltell
#pragma once
#include <cassert>
#include <string>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <immer/set.hpp>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../hof/functions.hpp"
#include "../monad/io.hpp"
#include "../monad/readerio.hpp"
#include "../monad/stateio.hpp"
#include "../setup.hpp"
#include "../types.hpp"
#include "filters.hpp"
#include "io.hpp"

#include "../macros_push.hpp"

namespace vulkandemo::setup::monadic
{
using vulkandemo::monad::io::IO;
using vulkandemo::monad::stateio::StateIO;
namespace io = vulkandemo::monad::io;
namespace readerio = vulkandemo::monad::readerio;
namespace stateio = vulkandemo::monad::stateio;

struct create_minimal_pipeline_layout_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_minimal_pipeline_layout(FW(self).device);
			}
		};

		static constexpr auto operator()(types::VulkanDevicePtr device)
		{
			return IO{action_t{std::move(device)}};
		}
	};
};

struct create_semaphore_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_semaphore(FW(self).device);
			}
		};

		static constexpr auto operator()(types::VulkanDevicePtr device)
		{
			return IO{action_t{std::move(device)}};
		}
	};
};

struct create_primary_command_buffers_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			types::VulkanCommandPoolPtr pool;
			types::VulkanCommandBufferCount count;
			constexpr types::VulkanCommandBuffersPtr operator()(this auto && self)
			{
				return setup::create_primary_command_buffers(
					FW(self).device, FW(self).pool, FW(self).count);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device,
			types::VulkanCommandPoolPtr pool,
			types::VulkanCommandBufferCount count)
		{
			return IO{
				action_t{.device = std::move(device), .pool = std::move(pool), .count = count}};
		}
	};
};

struct create_command_pool_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			types::VulkanQueueFamilyIdx queue_family_idx;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_command_pool(FW(self).device, FW(self).queue_family_idx);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device, types::VulkanQueueFamilyIdx queue_family_idx)
		{
			return IO{action_t{.device = std::move(device), .queue_family_idx = queue_family_idx}};
		}

		struct with_queue_family_idx_t
		{
			types::VulkanQueueFamilyIdx queue_family_idx;
			constexpr auto operator()(this auto && self, types::VulkanDevicePtr device)
			{
				return io_t{}(std::move(device), FW(self).queue_family_idx);
			}
		};
	};

	struct readerio_t
	{
		struct action_t
		{
		};
	};
};

struct create_per_image_frame_buffers_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			types::VulkanRenderPassPtr render_pass;
			immer::array<types::VulkanImageViewPtr> image_views;
			VkExtent2D size;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_per_image_frame_buffers(
					FW(self).device, FW(self).render_pass, FW(self).image_views, FW(self).size);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device,
			types::VulkanRenderPassPtr render_pass,
			immer::array<types::VulkanImageViewPtr> image_views,
			VkExtent2D size)
		{
			return IO{action_t{
				.device = std::move(device),
				.render_pass = std::move(render_pass),
				.image_views = std::move(image_views),
				.size = size}};
		}

		struct with_size_t
		{
			VkExtent2D size;
			constexpr auto operator()(
				this auto && self,
				types::VulkanDevicePtr device,
				types::VulkanRenderPassPtr render_pass,
				immer::array<types::VulkanImageViewPtr> image_views)
			{
				return io_t{}(
					std::move(device),
					std::move(render_pass),
					std::move(image_views),
					FW(self).size);
			}
		};
	};

	struct readerio_t
	{
		struct action_t
		{
			VkExtent2D size;
			constexpr auto operator()(this auto && self, auto && state)
			{
				assert(state->device);
				assert(state->render_pass);
				assert(state->image_views.size() > 0);
				assert(state->image_views[0]);

				return io_t{}(
					state->device, state->render_pass, state->image_views, FW(self).size);
			}
		};

		static constexpr auto operator()(VkExtent2D const size)
		{
			return readerio::ReaderIO{action_t{.size = size}};
		}
	};
};

struct create_single_presentation_subpass_render_pass_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			VkSurfaceFormatKHR surface_format;
			constexpr auto operator()(this auto && self)
			{
				assert(self.device);
				return setup::create_single_presentation_subpass_render_pass(
					FW(self).surface_format.format, FW(self).device);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device, VkSurfaceFormatKHR const surface_format)
		{
			return IO{action_t{.device = std::move(device), .surface_format = surface_format}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				assert(state->device);
				assert(state->surface_format.format != VK_FORMAT_UNDEFINED);
				return io_t{}(state->device, state->surface_format);
			}
		};

		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};

	struct stateio_t
	{
		struct modify_t
		{
			static constexpr auto operator()(types::VulkanRenderPassPtr render_pass, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.render_pass = std::move(render_pass);
						return obj;
					});
			}
		};

		static constexpr auto operator()()
		{
			return stateio::lift_readerio(readerio_t{}()).store(modify_t{});
		}
	};
};

struct create_exclusive_double_buffer_swapchain_and_image_views_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			VkPhysicalDevice physical_device{};
			types::VulkanDevicePtr device;
			types::VulkanSurfacePtr surface;
			VkSurfaceFormatKHR surface_format{};
			types::VulkanSwapchainPtr previous_swapchain;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_exclusive_double_buffer_swapchain_and_image_views(
					FW(self).logger,
					FW(self).physical_device,
					FW(self).device,
					FW(self).surface,
					FW(self).surface_format,
					FW(self).previous_swapchain);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			VkPhysicalDevice physical_device,
			types::VulkanDevicePtr device,
			types::VulkanSurfacePtr surface,
			VkSurfaceFormatKHR surface_format,
			types::VulkanSwapchainPtr previous_swapchain = nullptr)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.physical_device = physical_device,
				.device = std::move(device),
				.surface = std::move(surface),
				.surface_format = surface_format,
				.previous_swapchain = std::move(previous_swapchain)}};
		}

		struct with_logger_and_surface_format_t
		{
			LoggerPtr logger;
			VkSurfaceFormatKHR surface_format{};

			constexpr auto operator()(
				this auto && self,
				VkPhysicalDevice physical_device,
				types::VulkanDevicePtr device,
				types::VulkanSurfacePtr surface,
				types::VulkanSwapchainPtr previous_swapchain = nullptr)
			{
				return io_t{}(
					FW(self).logger,
					physical_device,
					std::move(device),
					std::move(surface),
					FW(self).surface_format,
					std::move(previous_swapchain));
			}
		};
	};
};

// IO monad lifter for creating device and queues (bind-like)
struct create_device_and_queues_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device{};
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts;
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_device_and_queues(
					FW(self).physical_device,
					FW(self).queue_family_and_counts,
					FW(self).device_extension_names);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device,
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts,
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names)
		{
			return IO{action_t{
				.physical_device = physical_device,
				.queue_family_and_counts = std::move(queue_family_and_counts),
				.device_extension_names = std::move(device_extension_names)}};
		}
	};
};

struct create_device_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts;
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names;

			constexpr auto operator()(this auto && self)
			{
				return setup::create_device(
					self.physical_device,
					FW(self).queue_family_and_counts,
					FW(self).device_extension_names);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device,
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts,
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names)
		{
			return IO{action_t{
				.physical_device = physical_device,
				.queue_family_and_counts = std::move(queue_family_and_counts),
				.device_extension_names = std::move(device_extension_names)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names;
			constexpr auto operator()(this auto && self, auto && state)
			{
				return io_t{}(
					state->physical_device,
					state->queue_family_and_counts,
					FW(self).device_extension_names);
			}
		};

		static constexpr auto operator()(
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names)
		{
			return readerio::ReaderIO{
				action_t{.device_extension_names = std::move(device_extension_names)}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			constexpr auto operator()(types::VulkanDevicePtr device, auto && state) const
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.device = std::move(device);
						return obj;
					});
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device,
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts,
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names)
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(
					   io_t{}(
						   physical_device,
						   std::move(queue_family_and_counts),
						   std::move(device_extension_names)))
				.store(modify_state_t{});
		}

		static constexpr auto operator()(
			immer::array<types::AvailableDeviceExtensionNameView> device_extension_names)
		{
			return stateio::lift_readerio(readerio_t{}(std::move(device_extension_names)))
				.store(modify_state_t{});
		}
	};
};

struct query_queues_for_queue_family_and_counts_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_queues_for_queue_family_and_counts(
					FW(self).device.get(), FW(self).queue_family_and_counts);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device,
			immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
				queue_family_and_counts)
		{
			return IO{action_t{
				.device = std::move(device),
				.queue_family_and_counts = std::move(queue_family_and_counts)}};
		}
	};
};

struct query_swapchain_images_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			types::VulkanSwapchainPtr swapchain;
			constexpr immer::array<VkImage> operator()(this auto && self)
			{
				return setup::query_swapchain_images(FW(self).device, FW(self).swapchain);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device, types::VulkanSwapchainPtr swapchain)
		{
			return IO{action_t{.device = std::move(device), .swapchain = std::move(swapchain)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto const & state)
			{
				return io_t{}(state->device, state->swapchain);
			}
		};
		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};
};

struct create_colour_aspect_single_mip_single_layer_image_views_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			VkSurfaceFormatKHR surface_format{};
			immer::array<VkImage> images;
			constexpr immer::array<types::VulkanImageViewPtr> operator()(this auto && self)
			{
				return setup::create_colour_aspect_single_mip_single_layer_image_views(
					FW(self).device, FW(self).surface_format, FW(self).images);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device,
			VkSurfaceFormatKHR surface_format,
			immer::array<VkImage> images)
		{
			return IO{action_t{
				.device = std::move(device),
				.surface_format = surface_format,
				.images = std::move(images)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			immer::array<VkImage> images;
			constexpr auto operator()(this auto && self, auto && state)
			{
				return io_t{}(state->device, state->surface_format, FW(self).images);
			}
		};

		static constexpr auto operator()(immer::array<VkImage> images)
		{
			return readerio::ReaderIO{action_t{.images = std::move(images)}};
		}

		struct with_images_t
		{
			immer::array<VkImage> images;
			constexpr auto operator()(this auto && self)
			{
				return readerio_t{}(FW(self).images);
			}
		};
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			static constexpr auto operator()(
				immer::array<types::VulkanImageViewPtr> image_views, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.image_views = std::move(image_views);
						return obj;
					});
			}
		};

		static constexpr auto operator()(immer::array<VkImage> images)
		{
			return stateio::lift_readerio(readerio_t::with_images_t{std::move(images)}())
				.store(modify_state_t{});
		}
	};
};

// IO monad lifter for filtering available surface formats (bind-like)
struct filter_available_surface_formats_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			VkPhysicalDevice physical_device{};
			types::VulkanSurfacePtr surface;
			immer::array<VkFormat> desired_formats;
			constexpr auto operator()(this auto && self)
			{
				return setup::filter_available_surface_formats(
					FW(self).logger,
					FW(self).physical_device,
					FW(self).surface,
					FW(self).desired_formats);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			VkPhysicalDevice physical_device,
			types::VulkanSurfacePtr surface,
			immer::array<VkFormat> desired_formats)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.physical_device = physical_device,
				.surface = std::move(surface),
				.desired_formats = std::move(desired_formats)}};
		}
	};
};

struct select_physical_device_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			immer::array<VkPhysicalDevice> physical_devices;
			immer::set<types::DesiredDeviceExtensionNameView> required_device_extensions;
			VkQueueFlagBits required_queue_capabilities{};
			VkMemoryPropertyFlags required_memory_type{};
			types::VulkanSurfacePtr required_surface_support;
			constexpr auto operator()(this auto && self)
			{
				return setup::select_physical_device(
					FW(self).logger,
					FW(self).physical_devices,
					FW(self).required_device_extensions,
					FW(self).required_queue_capabilities,
					FW(self).required_memory_type,
					FW(self).required_surface_support);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			immer::array<VkPhysicalDevice> physical_devices,
			immer::set<types::DesiredDeviceExtensionNameView> required_device_extensions,
			VkQueueFlagBits const required_queue_capabilities,
			VkMemoryPropertyFlags const required_memory_type = 0,
			types::VulkanSurfacePtr required_surface_support = nullptr)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.physical_devices = std::move(physical_devices),
				.required_device_extensions = std::move(required_device_extensions),
				.required_queue_capabilities = required_queue_capabilities,
				.required_memory_type = required_memory_type,
				.required_surface_support = std::move(required_surface_support)}};
		}
	};
};

// IO monad lifter for querying available device extensions (bind-like)
struct query_available_device_extensions_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			constexpr auto operator()(this auto && self)
			{
				return setup::enumerate_physical_device_extension_properties(
					FW(self).physical_device);
			}
		};

		static constexpr auto operator()(VkPhysicalDevice physical_device)
		{
			return IO{action_t{physical_device}};
		}
	};
};

struct maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			types::VulkanSurfacePtr surface;
			types::VulkanQueueFamilyIdx queue_family_idx;
			constexpr auto operator()(this auto && self)
			{
				return setup::maybe_queue_family_idx_if_supported_by_physical_device_and_surface(
					FW(self).physical_device, FW(self).surface, FW(self).queue_family_idx);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device,
			types::VulkanSurfacePtr surface,
			types::VulkanQueueFamilyIdx const queue_family_idx)
		{
			return IO{action_t{
				.physical_device = physical_device,
				.surface = std::move(surface),
				.queue_family_idx = queue_family_idx}};
		}

		struct with_physical_device_and_surface_t
		{
			VkPhysicalDevice physical_device;
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(
				this auto && self, types::VulkanQueueFamilyIdx const queue_family_idx)
			{
				return io_t{}(self.physical_device, FW(self).surface, queue_family_idx);
			}
		};
	};
};

// IO monad lifter for querying physical device properties (bind-like)
struct query_physical_device_properties_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device{};
			constexpr auto operator()(this auto && self)
			{
				return setup::query_physical_device_properties(FW(self).physical_device);
			}
		};

		static constexpr auto operator()(VkPhysicalDevice physical_device)
		{
			return IO{action_t{physical_device}};
		}
	};
};

struct query_available_queue_family_properties_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_available_queue_family_properties(FW(self).physical_device);
			}
		};

		static constexpr auto operator()(VkPhysicalDevice physical_device)
		{
			return IO{action_t{physical_device}};
		}
	};
};

struct filter_available_queue_families_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			VkQueueFlagBits desired_queue_capabilities;
			types::VulkanSurfacePtr desired_surface;
			constexpr auto operator()(this auto && self)
			{
				return setup::filter_available_queue_families(
					FW(self).physical_device,
					FW(self).desired_queue_capabilities,
					FW(self).desired_surface);
			}
		};
		static constexpr auto operator()(
			VkPhysicalDevice physical_device,
			VkQueueFlagBits const desired_queue_capabilities,
			types::VulkanSurfacePtr desired_surface)
		{
			return IO{
				action_t{physical_device, desired_queue_capabilities, std::move(desired_surface)}};
		}
	};
};

struct create_swapchain_t
{
	struct io_t
	{
		struct action_t
		{
			types::VulkanDevicePtr device;
			VkSwapchainCreateInfoKHR create_info;
			constexpr auto operator()(this auto && self)
			{
				return setup::create_swapchain(FW(self).device, FW(self).create_info);
			}
		};

		static constexpr auto operator()(
			types::VulkanDevicePtr device, VkSwapchainCreateInfoKHR const & create_info)
		{
			return IO{action_t{.device = std::move(device), .create_info = create_info}};
		}
	};

	struct stateio_t
	{
		struct with_swapchain_create_info_t
		{
			VkSwapchainCreateInfoKHR create_info;
			constexpr auto operator()(this auto && self, auto const & state)
			{
				using vulkandemo::monad::stateio::lift_io;
				return lift_io(io_t{}(state->device, FW(self).create_info));
			}
		};

		struct modify_state_t
		{
			static constexpr auto operator()(types::VulkanSwapchainPtr swapchain, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.swapchain = std::move(swapchain);
						return obj;
					});
			}
		};

		static constexpr auto operator()(VkSwapchainCreateInfoKHR const create_info)
		{
			using vulkandemo::monad::stateio::get_state_t;
			return get_state_t::stateio_t{}()
				.bind(with_swapchain_create_info_t{create_info})
				.store(modify_state_t{});
		}
	};
};

struct filter_available_memory_types_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			VkPhysicalDevice physical_device;
			VkMemoryPropertyFlags memory_flags;

			constexpr auto operator()(this auto && self)
			{
				return setup::filter_available_memory_types(
					FW(self).logger, FW(self).physical_device, FW(self).memory_flags);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger, VkPhysicalDevice physical_device, VkMemoryPropertyFlags memory_flags)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.physical_device = physical_device,
				.memory_flags = memory_flags}};
		}
	};
};

struct query_physical_device_memory_properties_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			VkPhysicalDevice physical_device;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_physical_device_memory_properties(
					FW(self).logger, FW(self).physical_device);
			}
		};

		static constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device)
		{
			return IO{action_t{.logger = std::move(logger), .physical_device = physical_device}};
		}
	};
};

// IO monad lifter for querying surface capabilities (bind-like)
struct query_surface_capabilities_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_surface_capabilities(
					FW(self).physical_device, FW(self).surface);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
		{
			return IO{action_t{.physical_device = physical_device, .surface = std::move(surface)}};
		}
	};
};

// IO monad lifter for querying present modes (bind-like)
struct query_present_modes_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_present_modes(FW(self).physical_device, FW(self).surface);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
		{
			return IO{action_t{.physical_device = physical_device, .surface = std::move(surface)}};
		}
	};
};

struct query_available_surface_formats_t
{
	struct io_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device{};
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(this auto && self)
			{
				return setup::enumerate_physical_device_surface_formats(
					FW(self).physical_device, FW(self).surface);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
		{
			return IO{action_t{.physical_device = physical_device, .surface = std::move(surface)}};
		}
	};
};

struct select_surface_format_t
{
	struct io_t
	{
		static constexpr auto operator()(
			LoggerPtr logger,
			VkPhysicalDevice physical_device,
			types::VulkanSurfacePtr surface,
			immer::array<VkFormat> desired_formats)
		{
			return query_available_surface_formats_t::io_t{}(
					   physical_device, std::move(surface))
				.fmap(
					filter_surface_formats_t::with_logger_and_desired_formats_t{
						.logger = std::move(logger), .desired_formats = std::move(desired_formats)})
				.fmap(hof::transform_range_to_front_elem_t{});
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			immer::array<VkFormat> desired_formats;
			constexpr auto operator()(this auto && self, auto && state)
			{
				assert(state->logger);
				assert(state->physical_device);
				assert(state->surface);
				return io_t{}(
					FW(state)->logger,
					FW(state)->physical_device,
					FW(state)->surface,
					FW(self).desired_formats);
			}
		};

		static constexpr auto operator()(immer::array<VkFormat> desired_formats)
		{
			return readerio::ReaderIO{action_t{std::move(desired_formats)}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			static constexpr auto operator()(VkSurfaceFormatKHR const surface_format, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.surface_format = surface_format;
						return obj;
					});
			}
		};

		static constexpr auto operator()(immer::array<VkFormat> desired_formats)
		{
			return stateio::lift_readerio(readerio_t{}(std::move(desired_formats)))
				.store(modify_state_t{});
		}
	};
};

struct log_surface_format_selection_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			immer::array<VkSurfaceFormatKHR> filtered_surface_formats;
			immer::array<VkSurfaceFormatKHR> available_surface_formats;
			immer::array<VkFormat> desired_formats;
			constexpr auto operator()(this auto && self)
			{
				setup::log_surface_format_selection(
					FW(self).logger,
					FW(self).filtered_surface_formats,
					FW(self).available_surface_formats,
					FW(self).desired_formats);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			immer::array<VkSurfaceFormatKHR> filtered_surface_formats,
			immer::array<VkSurfaceFormatKHR> available_surface_formats,
			immer::array<VkFormat> desired_formats)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.filtered_surface_formats = std::move(filtered_surface_formats),
				.available_surface_formats = std::move(available_surface_formats),
				.desired_formats = std::move(desired_formats)}};
		}
	};
};

struct enumerate_physical_devices_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			types::VulkanInstancePtr instance;
			constexpr immer::array<VkPhysicalDevice> operator()(this auto && self)
			{
				return setup::enumerate_physical_devices(FW(self).logger, FW(self).instance);
			}
		};

		static constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance)
		{
			return IO{action_t{.logger = std::move(logger), .instance = std::move(instance)}};
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(this auto && self, types::VulkanInstancePtr instance)
			{
				return io_t{}(FW(self).logger, std::move(instance));
			}
		};
	};
};

struct create_surface_t
{
	struct io_t
	{
		struct action_t
		{
			types::SDLWindowPtr window;
			types::VulkanInstancePtr instance;
			constexpr auto operator()(this auto && self)
			{
				// NOLINTNEXTLINE(bugprone-use-after-move)
				return setup::create_surface(FW(self).window, FW(self).instance);
			}
		};

		static constexpr auto operator()(
			types::SDLWindowPtr window, types::VulkanInstancePtr instance)
		{
			return IO{action_t{.window = std::move(window), .instance = std::move(instance)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				return io_t{}(state->window, state->instance);
			}
		};

		static constexpr auto operator()()
		{
			namespace readerio = vulkandemo::monad::readerio;
			return readerio::ReaderIO{action_t{}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			static constexpr auto operator()(types::VulkanSurfacePtr surface, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.surface = std::move(surface);
						return obj;
					});
			}
		};

		static constexpr auto operator()()
		{
			namespace stateio = vulkandemo::monad::stateio;
			return stateio::lift_readerio(readerio_t{}()).store(modify_state_t{});
		}
	};
};

struct create_debug_messenger_t
{
	struct io_t
	{
		struct action_t
		{
			LoggerPtr logger;
			types::VulkanInstancePtr instance;
			constexpr auto operator()(this auto && self)
			{
				// NOLINTNEXTLINE(bugprone-use-after-move)
				return setup::create_debug_messenger(FW(self).logger, FW(self).instance);
			}
		};

		static constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance)
		{
			return IO{action_t{.logger = std::move(logger), .instance = std::move(instance)}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			static constexpr auto operator()(
				types::VulkanDebugMessengerPtr messenger, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.messenger = std::move(messenger);
						return obj;
					});
			}
		};

		static constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance)
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(io_t{}(std::move(logger), std::move(instance)))
				.store(modify_state_t{});
		}

		struct from_state_t
		{
			static constexpr auto operator()(auto const & state)
			{
				// NOLINTNEXTLINE(bugprone-use-after-move)
				return stateio_t{}(state->logger, state->instance);
			}
		};

		struct using_state_t
		{
			static constexpr auto operator()()
			{
				using vulkandemo::monad::stateio::get_state;
				return get_state().bind(from_state_t{});
			}
		};
	};
};

struct create_instance_t
{
	struct io_t
	{
		// IO monad lifter for creating a Vulkan instance
		struct action_t
		{
			LoggerPtr logger;
			std::string name;
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable;
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable;

			constexpr auto operator()(this auto && self)
			{
				return setup::create_instance(
					FW(self).logger,
					FW(self).name,
					FW(self).layers_to_enable,
					FW(self).extensions_to_enable);
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			std::string name,
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
		{
			return IO{action_t{
				.logger = std::move(logger),
				.name = std::move(name),
				.layers_to_enable = std::move(layers_to_enable),
				.extensions_to_enable = std::move(extensions_to_enable)}};
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(
				this auto && self,
				std::string name,
				immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
				immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
			{
				return io_t{}(
					FW(self).logger,
					std::move(name),
					std::move(layers_to_enable),
					std::move(extensions_to_enable));
			}
		};
	};

	struct readerio_t
	{
		struct action_t
		{
			std::string name;
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable;
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable;

			constexpr auto operator()(this auto && self, auto && state)
			{
				assert(state->logger);
				return io_t{}(
					FW(state)->logger,
					FW(self).name,
					FW(self).layers_to_enable,
					FW(self).extensions_to_enable);
			}
		};

		static constexpr auto operator()(
			std::string name,
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
		{
			return readerio::ReaderIO{action_t{
				.name = std::move(name),
				.layers_to_enable = std::move(layers_to_enable),
				.extensions_to_enable = std::move(extensions_to_enable)}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			constexpr auto operator()(types::VulkanInstancePtr instance, auto && state) const
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.instance = std::move(instance);
						return obj;
					});
			}
		};

		static constexpr auto operator()(
			LoggerPtr logger,
			std::string name,
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(
					   io_t{}(
						   std::move(logger),
						   std::move(name),
						   std::move(layers_to_enable),
						   std::move(extensions_to_enable)))
				.store(modify_state_t{});
		}

		static constexpr auto operator()(
			std::string name,
			immer::array<types::AvailableInstanceLayerNameCstr> layers_to_enable,
			immer::array<types::AvailableInstanceExtensionNameCstr> extensions_to_enable)
		{
			return stateio::lift_readerio(
					   readerio_t{}(
						   std::move(name),
						   std::move(layers_to_enable),
						   std::move(extensions_to_enable)))
				.store(modify_state_t{});
		}
	};
};

struct query_sdl_instance_extension_names_t
{
	struct io_t
	{
		struct action_t
		{
			types::SDLWindowPtr window;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_sdl_instance_extension_names(FW(self).window);
			}
		};

		static constexpr auto operator()(types::SDLWindowPtr sdl_window)
		{
			return IO{action_t{std::move(sdl_window)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				return io_t{}(FW(state)->window);
			}
		};

		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};
};

struct enumerate_instance_layer_properties_t
{
	struct io_t
	{
		struct action_t
		{
			constexpr immer::array<VkLayerProperties> operator()() const
			{
				return setup::enumerate_instance_layer_properties();
			}
		};

		static constexpr auto operator()()
		{
			return IO{action_t{}};
		}
	};
};

struct layer_properties_filter_by_and_transform_to_instance_layer_name_t
{
	struct readerio_t
	{
		struct action_t
		{
			immer::set<types::DesiredInstanceLayerNameView> desired_layer_names;
			immer::array<VkLayerProperties> available_layer_descs;
			constexpr auto operator()(this auto && self, auto && state)
			{
				return io::pure(layer_properties_filter_by_and_transform_to_instance_layer_name(
					state->logger, FW(self).desired_layer_names, FW(self).available_layer_descs));
			}
		};

		static constexpr auto operator()(
			immer::set<types::DesiredInstanceLayerNameView> desired_layer_names,
			immer::array<VkLayerProperties> available_layer_descs)
		{
			return readerio::ReaderIO{action_t{
				.desired_layer_names = std::move(desired_layer_names),
				.available_layer_descs = std::move(available_layer_descs)}};
		}

		struct with_desired_layer_names_t
		{
			immer::set<types::DesiredInstanceLayerNameView> desired_layer_names;
			constexpr auto operator()(
				this auto && self, immer::array<VkLayerProperties> available_layer_descs)
			{
				return readerio_t{}(FW(self).desired_layer_names, std::move(available_layer_descs));
			}
		};
	};
};

struct query_available_instance_extensions_t
{
	struct io_t
	{
		struct action_t
		{
			constexpr immer::array<VkExtensionProperties> operator()() const
			{
				return setup::query_available_instance_extensions();
			}
		};

		static constexpr auto operator()()
		{
			return IO{action_t{}};
		}
	};
};

struct extension_properties_filter_by_and_transform_to_instance_extension_name_t
{
	struct readerio_t
	{
		struct action_t
		{
			immer::set<types::DesiredInstanceExtensionNameView> desired_extension_names;
			immer::array<VkExtensionProperties> available_extensions;
			constexpr auto operator()(this auto && self, auto && state)
			{
				return io::pure(
					extension_properties_filter_by_and_transform_to_instance_extension_name(
						state->logger,
						FW(self).desired_extension_names,
						FW(self).available_extensions));
			}
		};

		static constexpr auto operator()(
			immer::set<types::DesiredInstanceExtensionNameView> desired_extension_names,
			immer::array<VkExtensionProperties> available_extensions)
		{
			return readerio::ReaderIO{action_t{
				.desired_extension_names = std::move(desired_extension_names),
				.available_extensions = std::move(available_extensions)}};
		}

		struct with_desired_extension_names_t
		{
			immer::set<types::DesiredInstanceExtensionNameView> desired_extension_names;
			constexpr auto operator()(
				this auto && self, immer::array<VkExtensionProperties> available_extensions)
			{
				return readerio_t{}(
					FW(self).desired_extension_names, std::move(available_extensions));
			}
		};
	};
};

struct query_window_title_t
{
	struct io_t
	{
		struct action_t
		{
			types::SDLWindowPtr window;
			constexpr auto operator()(this auto && self)
			{
				return setup::query_window_title(FW(self).window);
			}
		};

		static constexpr auto operator()(types::SDLWindowPtr window)
		{
			return IO{action_t{std::move(window)}};
		}
	};

	struct readerio_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				return io_t{}(state->window);
			}
		};

		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};
};

// IO monad lifter for window drawable size (bind-like)
struct window_drawable_size_t
{
	struct io_t
	{
		struct action_t
		{
			types::SDLWindowPtr window;
			constexpr auto operator()(this auto && self)
			{
				assert(self.window);
				return setup::window_drawable_size(FW(self).window);
			}
		};

		static constexpr auto operator()(types::SDLWindowPtr window)
		{
			return IO{action_t{std::move(window)}};
		}
	};

	struct readerio_factory
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				return io_t{}(state->window);
			}
		};

		static constexpr auto operator()()
		{
			namespace readerio = vulkandemo::monad::readerio;
			return readerio::ReaderIO{action_t{}};
		}
	};
};

struct create_window_t
{
	struct io_t
	{
		struct action_t
		{
			std::string title;
			int width;
			int height;

			constexpr types::SDLWindowPtr operator()(this auto && self)
			{
				return setup::create_window(
					FW(self).title.c_str(), FW(self).width, FW(self).height);
			}
		};
		static constexpr auto operator()(std::string title, int const width, int const height)
		{
			return IO{action_t{.title = std::move(title), .width = width, .height = height}};
		}
	};

	struct stateio_t
	{
		struct modify_state_t
		{
			static constexpr auto operator()(types::SDLWindowPtr window, auto && state)
			{
				return FW(state).update(
					[&](auto obj)
					{
						obj.window = std::move(window);
						return obj;
					});
			}
		};
		static constexpr auto operator()(std::string title, int width, int height)
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(io_t{}(std::move(title), width, height)).store(modify_state_t{});
		}
	};
};

}  // namespace vulkandemo::setup::monad

#include "../macros_pop.hpp"