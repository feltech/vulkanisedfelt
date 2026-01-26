// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// Conflicts with clang-tidy wrt vulkan handle typedefs:
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppLocalVariableMayBeConst

#include "setup.hpp"
#include "setup/io.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)

#include <doctest/doctest.h>

#include <SDL_video.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include <libfork/core/sync_wait.hpp>
#include <libfork/schedule/lazy_pool.hpp>
#include <libfork/schedule/unit_pool.hpp>

#include <immer/array.hpp>
#include <immer/box.hpp>
#include <immer/set.hpp>
#include <immer/vector.hpp>

#include "./monad/io.hpp"
#include "./monad/readerio.hpp"
#include "./monad/stateio.hpp"
#include "Logger.hpp"
#include "hof.hpp"
#include "setup/filters.hpp"
#include "setup/logging.hpp"
#include "setup/monad.hpp"
#include "types.hpp"

#include "macros_push.hpp"

using namespace std::literals;

namespace vulkandemo::setup
{
immer::array<VkSurfaceFormatKHR> filter_available_surface_formats(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanSurfacePtr const & surface,
	std::span<VkFormat const> const desired_formats)
{
	// Get available surface formats.
	std::vector<VkSurfaceFormatKHR> const available_surface_formats = [&]
	{
		uint32_t count = 0;
		VK_CHECK(
			vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface.get(), &count, nullptr),
			"Failed to get surface format count");

		std::vector<VkSurfaceFormatKHR> out(count);
		VK_CHECK(
			vkGetPhysicalDeviceSurfaceFormatsKHR(
				physical_device, surface.get(), &count, out.data()),
			"Failed to get surface formats");
		return out;
	}();

	immer::array<VkSurfaceFormatKHR> const filtered_surface_formats =
		available_surface_formats |
		ranges::views::filter(
			[&](auto const & available_surface_format)
			{ return std::ranges::contains(desired_formats, available_surface_format.format); }) |
		ranges::to<immer::array>();

	// Log surface formats at debug level.
	if (logger->should_log(spdlog::level::debug))
	{
		// Log availability of desired formats.
		for (VkFormat const desired_format : desired_formats)
		{
			if (std::ranges::contains(
					filtered_surface_formats | std::views::transform(&VkSurfaceFormatKHR::format),
					desired_format))
				logger->debug(
					"Requested surface format: {} (available)", string_VkFormat(desired_format));
			else
				logger->debug(
					"Requested surface format: {} (unavailable)", string_VkFormat(desired_format));
		}

		for (auto const & [format, color_space] : available_surface_formats)
			logger->debug(
				"\tAvailable surface format: {} {}",
				string_VkFormat(format),
				string_VkColorSpaceKHR(color_space));
	}

	return filtered_surface_formats;
}

std::tuple<VkPhysicalDevice, types::VulkanQueueFamilyIdx> select_physical_device(
	LoggerPtr const & logger,
	immer::array<VkPhysicalDevice> const & physical_devices,
	immer::set<types::DesiredDeviceExtensionNameView> const & required_device_extensions,
	VkQueueFlagBits const required_queue_capabilities,
	VkMemoryPropertyFlags required_memory_type,
	types::VulkanSurfacePtr const & required_surface_support)
{
	using Score = std::size_t;
	std::vector<std::tuple<Score, VkPhysicalDevice, types::VulkanQueueFamilyIdx>> candidates;

	for (VkPhysicalDevice physical_device : physical_devices)
	{
		if (logger->should_log(spdlog::level::debug))
		{
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(physical_device, &device_properties);
			// Log name of device.
			logger->debug("Considering device {}", device_properties.deviceName);
		}
		auto const & filtered_device_extensions =
			filter_available_device_extensions(logger, physical_device, required_device_extensions);
		if (filtered_device_extensions.size() < required_device_extensions.size())
			continue;
		auto const & filtered_queue_families = filter_available_queue_families(
			physical_device, required_queue_capabilities, required_surface_support);
		if (filtered_queue_families.empty())
			continue;
		auto const & filtered_memory_types =
			filter_available_memory_types(logger, physical_device, required_memory_type);
		if (filtered_memory_types.empty())
			continue;

		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(physical_device, &device_properties);

		auto const score = static_cast<Score const>(
			device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

		candidates.emplace_back(score, physical_device, filtered_queue_families.front());
	}

	if (candidates.empty())
		throw std::runtime_error("Failed to find device with desired capabilities");

	// Sort by score.
	std::ranges::sort(
		candidates,
		[](auto const & lhs, auto const & rhs)
		{ return std::get<Score>(lhs) < std::get<Score>(rhs); });

	// Select last device, i.e. highest score.
	auto const selected_device_and_queue_family = std::apply(
		[]([[maybe_unused]] auto const score, auto const... tail) { return std::tuple{tail...}; },
		candidates.back());

	if (logger->should_log(spdlog::level::info))
	{
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(
			std::get<VkPhysicalDevice>(selected_device_and_queue_family), &device_properties);
		// Log name of device.
		logger->debug("Selected device {}", device_properties.deviceName);
	}

	return selected_device_and_queue_family;
}

std::vector<types::AvailableDeviceExtensionNameView> filter_available_device_extensions(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	immer::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names)
{
	if (desired_device_extension_names.empty())
		return {};

	// Get available device extension names.
	immer::array<VkExtensionProperties> const available_device_extensions =
		enumerate_physical_device_extension_properties(physical_device);

	std::set<types::AvailableDeviceExtensionNameView> const available_device_extension_names =
		available_device_extensions |
		std::views::transform(
			[](VkExtensionProperties const & extension)
			{ return types::AvailableDeviceExtensionNameView{extension.extensionName}; }) |
		ranges::to<std::set>();

	std::vector<types::AvailableDeviceExtensionNameView> const extensions_to_enable =
		ranges::views::set_intersection(
			available_device_extension_names | hof::views::value_of(),
			desired_device_extension_names | hof::views::value_of()) |
		ranges::to<std::vector<types::AvailableDeviceExtensionNameView>>();

	if (logger->should_log(spdlog::level::debug))
	{
		// Log requested extensions and whether they are available.
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(physical_device, &device_properties);
		logger->debug("Requested device extensions for device {}:", device_properties.deviceName);

		for (auto const & extension_name : desired_device_extension_names)
		{
			if (available_device_extension_names.contains(
					static_cast<types::AvailableDeviceExtensionNameView>(extension_name)))
				logger->debug("\t{} (available)", extension_name);
			else
				logger->debug("\t{} (unavailable)", extension_name);
		}

		// Log all available extensions.
		logger->trace("Available device extensions:");
		for (auto const & extension_name : available_device_extension_names)
			logger->trace("\t{}", extension_name);
	}

	return extensions_to_enable;
}

std::vector<types::VulkanQueueFamilyIdx> filter_available_queue_families(
	VkPhysicalDevice const & physical_device,
	VkQueueFlagBits const desired_queue_capabilities,
	types::VulkanSurfacePtr const & desired_surface)
{
	std::vector<VkQueueFamilyProperties> const queue_family_properties = [&]
	{
		std::vector<VkQueueFamilyProperties> out;
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
		out.resize(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, out.data());
		return out;
	}();

	return std::views::iota(0U, queue_family_properties.size()) |
		std::views::filter(
			   [&](auto const queue_family_idx)
			   {
				   return (queue_family_properties[queue_family_idx].queueFlags &
						   desired_queue_capabilities) == desired_queue_capabilities;
			   }) |
		std::views::filter(
			   [&](auto const queue_family_idx)
			   {
				   if (!desired_surface)
					   return true;
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
		ranges::to<std::vector<types::VulkanQueueFamilyIdx>>();
}

std::vector<types::VulkanMemoryTypeIdx> filter_available_memory_types(
	LoggerPtr const & logger, VkPhysicalDevice physical_device, VkMemoryPropertyFlags memory_flags)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
	std::span const memory_types =
		std::span{memory_properties.memoryTypes}.subspan(0, memory_properties.memoryTypeCount);

	if (logger->should_log(spdlog::level::debug))
	{
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(physical_device, &device_properties);
		logger->debug(
			"Requested memory type {} for device {}:",
			string_VkMemoryPropertyFlags(memory_flags),
			device_properties.deviceName);
		for (auto const & [idx, memory_type] : std::views::enumerate(memory_types))
			logger->debug(
				"\tType {}: {}", idx, string_VkMemoryPropertyFlags(memory_type.propertyFlags));
	}

	return std::views::iota(0U, memory_types.size()) |
		std::views::filter(
			   [&](uint32_t const idx)
			   { return (memory_types[idx].propertyFlags & memory_flags) == memory_flags; }) |
		ranges::to<std::vector<types::VulkanMemoryTypeIdx>>();
}

immer::array<types::AvailableInstanceLayerNameCstr> filter_available_layers(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceLayerNameView> const & desired_layer_names)
{
	// Query available layers.
	std::vector<VkLayerProperties> available_layer_descs = []
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
	}();

	// Extract available layer names.
	auto const available_layer_names = available_layer_descs |
		std::views::transform(&VkLayerProperties::layerName) |
		ranges::to<std::set<types::AvailableInstanceLayerNameView>>;

	log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

	auto out = immer::array<types::AvailableInstanceLayerNameCstr>{}.transient();
	// Get intersection of desired layers and available layers, converted to C strings.
	for (auto const * cstr : ranges::views::set_intersection(
								 desired_layer_names | hof::views::value_of(),
								 available_layer_names | hof::views::value_of()) |
			 ranges::views::transform(&std::string_view::data))
	{
		// TODO(DF): ranges::to<immer::array> doesn't work here, for some reason.
		out.push_back(types::AvailableInstanceLayerNameCstr{cstr});
	}
	return std::move(out).persistent();
}

immer::array<types::AvailableInstanceExtensionNameCstr> filter_available_instance_extensions(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names)
{
	// Get available extensions.
	std::vector<VkExtensionProperties> const available_extensions = []
	{
		std::vector<VkExtensionProperties> out;
		uint32_t available_extensions_count = 0;
		VK_CHECK(
			vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, nullptr),
			"Failed to enumerate instance extensions");
		out.resize(available_extensions_count);
		VK_CHECK(
			vkEnumerateInstanceExtensionProperties(
				nullptr, &available_extensions_count, out.data()),
			"Failed to enumerate instance extensions");

		return out;
	}();

	// Extract extension names.
	std::set const available_extension_names = available_extensions |
		std::views::transform(&VkExtensionProperties::extensionName) |
		ranges::to<std::set<types::AvailableInstanceExtensionNameView>>;

	// Intersection of available extensions and desired extensions to return.
	auto result = immer::array<types::AvailableInstanceExtensionNameCstr>{}.transient();
	for (char const * name : ranges::views::set_intersection(
								 desired_extension_names | hof::views::value_of(),
								 available_extension_names | hof::views::value_of()) |
			 std::views::transform(&std::string_view::data))
	{
		result.push_back(types::AvailableInstanceExtensionNameCstr{name});
	}

	// log_instance_extensions_info(
	// logger, desired_extension_names, available_extension_names, available_extensions);

	return std::move(result).persistent();
}

// NOLINTBEGIN(readability-function-cognitive-complexity,*-using-namespace)

namespace
{

using vulkandemo::monad::io::IO;
using vulkandemo::monad::stateio::StateIO;

namespace test
{
using vulkandemo::monad::stateio::get_state_t;
using vulkandemo::monad::stateio::lift_io;
namespace readerio = vulkandemo::monad::readerio;
namespace create_a_window
{
constexpr int kExpectedWidth = 800;
constexpr int kExpectedHeight = 600;
constexpr auto kExpectedName = "Hello Vulkan";
struct check_window_t
{
	struct io_factory_t
	{
		struct action_t
		{
			types::SDLWindowPtr window;

			bool operator()() const
			{
				REQUIRE(window);
				int width = 0;
				int height = 0;
				SDL_GetWindowSize(window.get(), &width, &height);
				CHECK(width == kExpectedWidth);
				CHECK(height == kExpectedHeight);
				CHECK(SDL_GetWindowTitle(window.get()) == std::string_view{kExpectedName});
				return true;
			}
		};

		constexpr auto operator()(types::SDLWindowPtr window) const
		{
			return IO{action_t{std::move(window)}};
		}
	};
};

}  // namespace create_a_window
struct query_desired_instance_extensions_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(LoggerPtr logger)
		{
			return monad::query_available_instance_extensions_t::io_factory_t{}().fmap(
				transform_to_instance_extension_name_filtered_by_instance_extension_name_t{
					.logger = std::move(logger),
					.desired_extension_names = immer::set{{types::DesiredInstanceExtensionNameView{
						VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}}});
		}
	};
};

namespace create_a_vulkan_instance
{

struct query_sdl_and_desired_instance_extensions_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(LoggerPtr logger, types::SDLWindowPtr window)
		{
			return sequence(
					   // Get SDL window vulkan extension names.
					   monad::query_sdl_instance_extension_names_t::io_factory_t{}(
						   std::move(window)),
					   // Fetch and filter additional extension names.
					   query_desired_instance_extensions_t::io_factory_t{}(std::move(logger)))
				.fmap(
					// Concatenate SDL and optional extensions.
					hof::transform_concat_t{});
		}
	};
};

struct query_desired_instance_layer_names_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger) const
		{
			return monad::query_available_instance_layers_t::io_factory_t{}().fmap(
				transform_to_instance_layer_name_filtered_by_instance_layer_name_t{
					.logger = std::move(logger),
					.desired_layer_names = immer::set{
						{types::DesiredInstanceLayerNameView{"some_unavailable_layer"},
						 types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}}}});
		}
	};
};

struct query_layers_and_extensions_and_create_instance_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			LoggerPtr const & logger, types::SDLWindowPtr const & window) const
		{
			using monad::create_instance_t;
			using monad::query_window_title_t;

			return sequence(
					   // Get title of window to use as app/engine name in vulkan.
					   query_window_title_t::io_factory_t{}(window),
					   // Fetch and filter layer names.
					   query_desired_instance_layer_names_t::io_factory_t{}(logger),
					   // Fetch and filter extension names.
					   query_sdl_and_desired_instance_extensions_t::io_factory_t{}(logger, window))
				.bind(create_instance_t::io_factory_t::with_logger_t{logger});
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(types::SDLWindowPtr const & window) const
			{
				return io_factory_t{}(logger, window);
			}
		};
	};
};

struct check_instance_t
{
	struct io_factory_t
	{
		struct action_t
		{
			types::VulkanInstancePtr instance;
			constexpr bool operator()() const
			{
				CHECK(instance);
				return true;
			}
		};

		constexpr auto operator()(types::VulkanInstancePtr instance) const
		{
			return IO{action_t{std::move(instance)}};  // NOLINT(performance-move-const-arg)
		}
	};
};
}  // namespace create_a_vulkan_instance

namespace create_a_vulkan_debug_utils_messenger
{
struct create_instance_with_extensions_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr logger,
			immer::array<types::AvailableInstanceExtensionNameCstr> available_extensions)
		{
			return monad::create_instance_t::io_factory_t{}(
				std::move(logger), "test", {}, std::move(available_extensions));
		}

		struct with_logger_t
		{
			LoggerPtr logger;

			constexpr auto operator()(
				this auto && self,
				immer::array<types::AvailableInstanceExtensionNameCstr> available_extensions)
			{
				return io_factory_t{}(FW(self).logger, std::move(available_extensions));
			}
		};
	};
};

struct create_debug_messenger_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance) const
		{
			return monad::create_debug_messenger_t::io_factory_t{}(
				std::move(logger), std::move(instance));
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(types::VulkanInstancePtr instance) const
			{
				return io_factory_t{}(logger, std::move(instance));
			}
		};
	};
};

struct check_messenger_t
{
	struct io_factory_t
	{
		struct action_t
		{
			types::VulkanDebugMessengerPtr messenger;
			bool operator()() const
			{
				CHECK(messenger);
				return true;
			}
		};

		constexpr auto operator()(types::VulkanDebugMessengerPtr messenger) const
		{
			return IO{action_t{std::move(messenger)}};	// NOLINT(performance-move-const-arg)
		}
	};
};

}  // namespace create_a_vulkan_debug_utils_messenger

struct query_validation_layer_names_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger) const
		{
			return monad::query_available_instance_layers_t::io_factory_t{}().fmap(
				transform_to_instance_layer_name_filtered_by_instance_layer_name_t{
					.logger = std::move(logger),
					.desired_layer_names = immer::set{
						{types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}}}});
		}
	};
};

struct query_sdl_and_desired_instance_extensions_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, types::SDLWindowPtr window) const
		{
			return sequence(
					   // Get SDL window required vulkan extension names.
					   monad::query_sdl_instance_extension_names_t::io_factory_t{}(
						   std::move(window)),
					   // Fetch and filter additional extension names.
					   monad::query_available_instance_extensions_t::io_factory_t{}().fmap(
						   transform_to_instance_extension_name_filtered_by_instance_extension_name_t{
							   .logger = std::move(logger),
							   .desired_extension_names =
								   immer::set{{types::DesiredInstanceExtensionNameView{
									   VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}}}))
				.fmap(
					// Concatenate SDL and optional extensions.
					hof::transform_concat_t{});
		}
	};
};

struct query_instance_args_from_window_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, types::SDLWindowPtr window) const
		{
			// Explicit copies to then move out of, to avoid thinking about std::move precedence
			// below.
			auto logger_copy = logger;
			auto window_copy = window;

			return sequence(
				// Get title of window to use as app/engine name in vulkan.
				monad::query_window_title_t::io_factory_t{}(std::move(window)),
				// Fetch and filter layer names.
				query_validation_layer_names_t::io_factory_t{}(std::move(logger)),
				// Fetch and filter extension names (SDL + desired).
				query_sdl_and_desired_instance_extensions_t::io_factory_t{}(
					std::move(logger_copy), std::move(window_copy)));
		}
	};

	struct readerio_factory_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto const & state)
			{
				return io_factory_t{}(state->logger, state->window);
			}
		};

		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};
};

struct create_default_instance_t
{
	struct stateio_factory_t
	{
		constexpr auto operator()() const
		{
			using monad::create_debug_messenger_t;
			using monad::create_instance_t;
			using vulkandemo::monad::stateio::get_state;
			using vulkandemo::monad::stateio::lift_readerio;
			// Create application window.
			return monad::create_window_t::stateio_factory_t{}("", 0, 0)
				// Gather arguments for constructing a vulkan instance.
				.then(lift_readerio(query_instance_args_from_window_t::readerio_factory_t{}()))
				// Create/store Vulkan instance
				.bind(create_instance_t::stateio_factory_t::using_state_t{})
				// Create/store debug messenger callback closure.
				.then(create_debug_messenger_t::stateio_factory_t::using_state_t{}())
				// Replace arg with state, in case useful for subsequent bind()/fmap() calls.
				.then(get_state());
		}
	};
};

namespace create_a_vulkan_surface
{

struct create_surface_t
{
	struct readerio_factory_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto const & state)
			{
				return monad::create_surface_t::io_factory_t{}(state->window, state->instance);
			}
		};
		static constexpr auto operator()()
		{
			return readerio::ReaderIO{action_t{}};
		}
	};
};

struct check_surface_t
{
	static constexpr bool operator()(types::VulkanSurfacePtr const & surface)
	{
		CHECK(surface);
		return true;
	}
};

}  // namespace create_a_vulkan_surface
namespace enumerate_devices
{

using vulkandemo::monad::io::IO;
using vulkandemo::monad::stateio::StateIO;

struct query_filtered_device_extensions_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device) const
		{
			return monad::query_available_device_extensions_t::io_factory_t{}(physical_device)
				.fmap(
					extension_properties_filter_by_and_transform_to_device_extension_name_t::
						with_logger_and_physical_device_and_desired_device_extension_names_t{
							std::move(logger),
							physical_device,
							immer::set{{types::DesiredDeviceExtensionNameView{
								VK_KHR_SWAPCHAIN_EXTENSION_NAME}}}});
		}
	};
};

struct query_host_visible_memory_type_idxs_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device) const
		{
			return monad::query_physical_device_memory_properties_t::io_factory_t{}(
					   std::move(logger), physical_device)
				.fmap(
					memory_properties_filter_by_and_transform_to_memory_type_idx_t::
						with_memory_property_flags_t{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT});
		}
	};
};

struct query_supported_graphics_queue_families_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
		{
			return monad::query_available_queue_family_properties_t::io_factory_t{}(physical_device)
				.fmap(
					transform_queue_family_properties_to_queue_family_idxs_filtered_by_capability_t::
						with_desired_queue_capabilities_t{VK_QUEUE_GRAPHICS_BIT})
				.filter(
					monad::maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t::
						io_factory_t::with_physical_device_and_surface_t{
							.physical_device = physical_device, .surface = std::move(surface)});
		}
	};
};

struct check_availability_t
{
	constexpr auto operator()(
		immer::array<types::AvailableDeviceExtensionNameView> const & available_device_extensions,
		immer::array<types::VulkanMemoryTypeIdx> const & available_memory_types,
		immer::array<types::VulkanQueueFamilyIdx> const & available_queue_families) const
	{
		CHECK(!available_memory_types.empty());
		CHECK(available_device_extensions.size() == 1);
		CHECK(!available_queue_families.empty());
		return true;
	}
};

struct choose_physical_device_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance) const
		{
			return monad::enumerate_physical_devices_t::io_factory_t{}(
					   std::move(logger), std::move(instance))
				.fmap(hof::transform_range_to_front_elem_t{});
		}
	};
};

struct create_surface_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			types::SDLWindowPtr window, types::VulkanInstancePtr instance) const
		{
			return monad::create_surface_t::io_factory_t{}(std::move(window), std::move(instance));
		}
	};
};

struct choose_physical_device_and_create_surface_t
{
	struct stateio_factory_t
	{
		static constexpr auto operator()(auto const & state)
		{
			auto instance_for_surface = state->instance;
			return lift_io(sequence(
				choose_physical_device_t::io_factory_t{}(state->logger, state->instance),
				create_surface_t::io_factory_t{}(state->window, std::move(instance_for_surface))));
		}
	};
};

struct check_supported_extensions_and_memory_types_and_queue_families_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			LoggerPtr logger, VkPhysicalDevice chosen_device, types::VulkanSurfacePtr surface) const
		{
			auto logger_for_extension_filter = logger;
			auto logger_for_memory_type_query = std::move(logger);

			return vulkandemo::monad::io::sequence(
					   query_filtered_device_extensions_t::io_factory_t{}(
						   std::move(logger_for_extension_filter), chosen_device),
					   query_host_visible_memory_type_idxs_t::io_factory_t{}(
						   std::move(logger_for_memory_type_query), chosen_device),
					   query_supported_graphics_queue_families_t::io_factory_t{}(
						   chosen_device, std::move(surface)))
				.fmap(check_availability_t{});
		}
	};

	struct readerio_factory_t
	{
		struct action_t
		{
			VkPhysicalDevice chosen_device;
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(this auto && self, auto const & state)
			{
				return io_factory_t{}(state->logger, FW(self).chosen_device, FW(self).surface);
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice chosen_device, types::VulkanSurfacePtr surface)
		{
			return readerio::ReaderIO{
				action_t{.chosen_device = chosen_device, .surface = std::move(surface)}};
		}
	};
};

}  // namespace enumerate_devices

namespace select_physical_device
{
using vulkandemo::monad::io::IO;

// Reuse helpers from enumerate_devices where possible
using test::enumerate_devices::query_filtered_device_extensions_t;
using test::enumerate_devices::query_host_visible_memory_type_idxs_t;

struct has_host_visible_memory_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device) const
		{
			return monad::query_physical_device_memory_properties_t::io_factory_t{}(
					   std::move(logger), physical_device)
				.fmap(
					memory_properties_filter_by_and_transform_to_memory_type_idx_t::
						with_memory_property_flags_t{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT})
				.fmap(hof::transform_range_to_check_non_empty_t{})
				.fmap(hof::transform_bool_to_optional_t{physical_device});
		}

		struct with_logger_t
		{
			LoggerPtr logger;

			constexpr auto operator()(VkPhysicalDevice physical_device) const
			{
				return io_factory_t{}(logger, physical_device);
			}
		};
	};
};

struct has_required_device_extensions_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device) const
		{
			return monad::query_available_device_extensions_t::io_factory_t{}(physical_device)
				.fmap(
					extension_properties_filter_by_and_transform_to_device_extension_name_t::
						with_logger_and_physical_device_and_desired_device_extension_names_t{
							.logger = std::move(logger),
							.physical_device = physical_device,
							.desired_device_extension_names =
								immer::set{{types::DesiredDeviceExtensionNameView{
									VK_KHR_SWAPCHAIN_EXTENSION_NAME}}}})
				.fmap(hof::transform_range_to_check_non_empty_t{})
				.fmap(hof::transform_bool_to_optional_t{physical_device});
		}
	};

	struct with_logger_t
	{
		LoggerPtr logger;
		constexpr auto operator()(VkPhysicalDevice physical_device) const
		{
			return io_factory_t{}(logger, physical_device);
		}
	};
};

struct maybe_score_physical_device_and_queue_family_t
{
	static constexpr auto operator()(
		VkPhysicalDevice physical_device,
		VkPhysicalDeviceProperties const & physical_device_properties,
		immer::array<types::VulkanQueueFamilyIdx> const & filtered_queue_family_idxs)
	{
		return maybe_score_physical_device_and_queue_family(
			physical_device, physical_device_properties, filtered_queue_family_idxs);
	}

	struct with_physical_device_t
	{
		VkPhysicalDevice physical_device;
		constexpr auto operator()(
			VkPhysicalDeviceProperties const & physical_device_properties,
			immer::array<types::VulkanQueueFamilyIdx> const & filtered_queue_family_idxs) const
		{
			return maybe_score_physical_device_and_queue_family_t{}(
				physical_device, physical_device_properties, filtered_queue_family_idxs);
		}
	};
};

struct compute_device_score_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			types::VulkanSurfacePtr surface, VkPhysicalDevice physical_device) const
		{
			using monad::maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t;
			using monad::query_available_queue_family_properties_t;
			using monad::query_physical_device_properties_t;
			using vulkandemo::monad::io::filter_t;

			return sequence(
					   query_physical_device_properties_t::io_factory_t{}(physical_device),
					   query_available_queue_family_properties_t::io_factory_t{}(physical_device)
						   .fmap(
							   transform_queue_family_properties_to_queue_family_idxs_filtered_by_capability_t::
								   with_desired_queue_capabilities_t{VK_QUEUE_GRAPHICS_BIT})
						   .bind(
							   filter_t::io_factory_t::with_kleisli_t{
								   maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t::
									   io_factory_t::with_physical_device_and_surface_t{
										   .physical_device = physical_device,
										   .surface = std::move(surface)}}))
				.fmap(
					maybe_score_physical_device_and_queue_family_t::with_physical_device_t{
						physical_device});
		}

		struct with_surface_t
		{
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(this auto && self, VkPhysicalDevice physical_device)
			{
				return io_factory_t{}(FW(self).surface, physical_device);
			}
		};
	};
};

struct check_selected_physical_device_and_queue_family_t
{
	auto operator()(std::optional<std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx>> const &
						maybe_selected_physical_device_and_queue_family_idx) const
	{
		REQUIRE(maybe_selected_physical_device_and_queue_family_idx.has_value());
		auto const & [selected_device, queue_family_idx] =
			*maybe_selected_physical_device_and_queue_family_idx;
		CHECK(selected_device != nullptr);
		CHECK(queue_family_idx >= 0);
		return true;
	}
};

struct score_devices_and_select_best_and_check_valid_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			types::VulkanSurfacePtr surface, immer::array<VkPhysicalDevice> physical_devices) const
		{
			namespace io = vulkandemo::monad::io;
			auto score_ios =
				physical_devices |
				ranges::views::transform(
					compute_device_score_t::io_factory_t::with_surface_t{std::move(surface)}) |
				ranges::to<immer::array>;

			return io::sequence(std::move(score_ios))
				.fmap(hof::transform_maybes_to_values_t{})
				.fmap(maybe_select_best_scoring_physical_device_and_queue_family_idx)
				.fmap(check_selected_physical_device_and_queue_family_t{});
		}
	};
};

struct select_physical_device_and_queue_family_and_check_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr const & logger,
			types::SDLWindowPtr window,
			types::VulkanInstancePtr const & instance)
		{
			auto filtered_physical_devices_io =
				monad::enumerate_physical_devices_t::io_factory_t{}(logger, instance)
					.filter(has_host_visible_memory_t::io_factory_t::with_logger_t{logger})
					.filter(has_required_device_extensions_t::with_logger_t(logger));
			return sequence(
					   monad::create_surface_t::io_factory_t{}(std::move(window), instance),
					   std::move(filtered_physical_devices_io))
				.bind(score_devices_and_select_best_and_check_valid_t::io_factory_t{});
		}
	};

	struct stateio_factory_t
	{
		static constexpr auto operator()(auto const & state)
		{
			return lift_io(io_factory_t{}(state->logger, state->window, state->instance));
		}
	};
};

}  // namespace select_physical_device

struct check_has_host_visible_mem_for_physical_device_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device)
		{
			return monad::query_physical_device_memory_properties_t::io_factory_t{}(
					   std::move(logger), physical_device)
				.fmap(
					memory_properties_filter_by_and_transform_to_memory_type_idx_t::
						with_memory_property_flags_t(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
				.fmap(hof::transform_range_to_check_non_empty_t{})
				.fmap(hof::transform_bool_to_optional_t{physical_device});
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(this auto && self, VkPhysicalDevice physical_device)
			{
				return io_factory_t{}(FW(self).logger, physical_device);
			}
		};
	};
};

struct check_has_swapchain_extension_for_physical_device_t
{
	struct io_factory_t
	{
		constexpr auto operator()(LoggerPtr logger, VkPhysicalDevice physical_device) const
		{
			return monad::query_available_device_extensions_t::io_factory_t{}(physical_device)
				.fmap(
					extension_properties_filter_by_and_transform_to_device_extension_name_t::
						with_logger_and_physical_device_and_desired_device_extension_names_t{
							.logger = std::move(logger),
							.physical_device = physical_device,
							.desired_device_extension_names =
								immer::set{{types::DesiredDeviceExtensionNameView{
									VK_KHR_SWAPCHAIN_EXTENSION_NAME}}}})
				.fmap(hof::transform_range_to_check_non_empty_t{})
				.fmap(hof::transform_bool_to_optional_t{physical_device});
		}

		struct with_logger_t
		{
			LoggerPtr logger;
			constexpr auto operator()(this auto && self, VkPhysicalDevice physical_device)
			{
				return io_factory_t{}(FW(self).logger, physical_device);
			}
		};
	};
};

struct maybe_score_for_physical_device_properties_and_queue_family_t
{
	constexpr auto operator()(
		VkPhysicalDevice physical_device,
		VkPhysicalDeviceProperties const & physical_device_properties,
		immer::array<types::VulkanQueueFamilyIdx> const & queue_family_idxs) const
	{
		return maybe_score_physical_device_and_queue_family(
			physical_device, physical_device_properties, queue_family_idxs);
	}

	struct with_physical_device_t
	{
		VkPhysicalDevice physical_device;
		constexpr auto operator()(
			VkPhysicalDeviceProperties const & physical_device_properties,
			immer::array<types::VulkanQueueFamilyIdx> const & queue_family_idxs) const
		{
			return maybe_score_for_physical_device_properties_and_queue_family_t{}(
				physical_device, physical_device_properties, queue_family_idxs);
		}
	};
};

struct compute_score_for_physical_device_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			types::VulkanSurfacePtr surface, VkPhysicalDevice physical_device) const
		{
			using monad::maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t;
			using monad::query_available_queue_family_properties_t;
			using monad::query_physical_device_properties_t;
			return sequence(
					   query_physical_device_properties_t::io_factory_t{}(physical_device),
					   query_available_queue_family_properties_t::io_factory_t{}(physical_device)
						   .fmap(
							   transform_queue_family_properties_to_queue_family_idxs_filtered_by_capability_t::
								   with_desired_queue_capabilities_t{VK_QUEUE_GRAPHICS_BIT})
						   .filter(
							   maybe_queue_family_idx_if_supported_by_physical_device_and_surface_t::
								   io_factory_t::with_physical_device_and_surface_t{
									   .physical_device = physical_device,
									   .surface = std::move(surface)}))
				.fmap(
					maybe_score_for_physical_device_properties_and_queue_family_t::
						with_physical_device_t{physical_device});
		}

		struct with_surface_t
		{
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(VkPhysicalDevice physical_device) const
			{
				return io_factory_t{}(surface, physical_device);
			}
		};
	};
};

struct compute_score_for_physical_devices_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			types::VulkanSurfacePtr surface, immer::array<VkPhysicalDevice> physical_devices)
		{
			using monad::query_available_queue_family_properties_t;
			using monad::query_physical_device_properties_t;
			using vulkandemo::monad::io::sequence;

			auto const score_ios =
				physical_devices |
				ranges::views::transform(
					compute_score_for_physical_device_t::io_factory_t::with_surface_t{
						std::move(surface)}) |
				ranges::to<immer::array>;

			return sequence(score_ios);
		}

		struct with_surface_t
		{
			types::VulkanSurfacePtr surface;
			constexpr auto operator()(
				this auto && self, immer::array<VkPhysicalDevice> physical_devices)
			{
				return io_factory_t{}(FW(self).surface, std::move(physical_devices));
			}
		};
	};
};

struct select_physical_device_and_queue_family_for_surface_t
{
	constexpr auto operator()(
		LoggerPtr const & logger,
		types::VulkanInstancePtr instance,
		types::VulkanSurfacePtr surface) const
	{
		return monad::enumerate_physical_devices_t::io_factory_t{}(logger, std::move(instance))
			.filter(
				check_has_host_visible_mem_for_physical_device_t::io_factory_t::with_logger_t{
					logger})
			.filter(
				check_has_swapchain_extension_for_physical_device_t::io_factory_t::with_logger_t{
					logger})
			.bind(
				compute_score_for_physical_devices_t::io_factory_t::with_surface_t{
					std::move(surface)})
			.fmap(hof::transform_maybes_to_values_t{})
			.fmap(maybe_select_best_scoring_physical_device_and_queue_family_idx);
	}

	struct with_logger_and_instance_t
	{
		LoggerPtr logger;
		types::VulkanInstancePtr instance;
		constexpr auto operator()(types::VulkanSurfacePtr surface) const
		{
			return select_physical_device_and_queue_family_for_surface_t{}(
				logger, instance, std::move(surface));
		}
	};
};

struct create_surface_and_select_physical_device_and_queue_family_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr logger, types::SDLWindowPtr window, types::VulkanInstancePtr instance)
		{
			auto instance_for_surface_creation = instance;
			auto instance_for_physical_device_selection = std::move(instance);
			return monad::create_surface_t::io_factory_t{}(
					   std::move(window), std::move(instance_for_surface_creation))
				.bind(
					select_physical_device_and_queue_family_for_surface_t::
						with_logger_and_instance_t{
							.logger = std::move(logger),
							.instance = std::move(instance_for_physical_device_selection)});
		}
	};

	struct stateio_factory_t
	{
		static constexpr auto operator()(auto const & state)
		{
			using vulkandemo::monad::stateio::lift_io;

			return lift_io(io_factory_t{}(state->logger, state->window, state->instance));
		}
	};
};

struct create_default_instance_and_physical_device_and_queue_family_t
{
	struct stateio_factory_t
	{
		static constexpr auto operator()()
		{
			using vulkandemo::monad::stateio::get_state_t;
			return create_default_instance_t::stateio_factory_t{}()
				.then(get_state_t::stateio_factory_t{}())
				.bind(
					create_surface_and_select_physical_device_and_queue_family_t::
						stateio_factory_t{});
		}
	};
};
namespace select_device_with_capability
{
using vulkandemo::monad::io::IO;

struct select_basic_physical_device_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr logger, immer::array<VkPhysicalDevice> physical_devices)
		{
			return monad::select_physical_device_t::io_factory_t{}(
				std::move(logger),
				std::move(physical_devices),
				immer::set{
					{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}},
				VK_QUEUE_GRAPHICS_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		}
	};
	struct with_logger_t
	{
		LoggerPtr logger;
		constexpr auto operator()(
			this auto && self, immer::array<VkPhysicalDevice> physical_devices)
		{
			return io_factory_t{}(FW(self).logger, std::move(physical_devices));
		}
	};
};

struct select_physical_device_with_requirements_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(LoggerPtr logger, types::VulkanInstancePtr instance)
		{
			auto logger_for_device_enumeration = logger;
			auto logger_for_physical_device_selection = std::move(logger);
			return monad::enumerate_physical_devices_t::io_factory_t{}(
					   std::move(logger_for_device_enumeration), std::move(instance))
				.bind(
					select_basic_physical_device_t::with_logger_t{
						std::move(logger_for_physical_device_selection)});
		}
	};

	struct stateio_factory_t
	{
		static constexpr auto operator()(auto const & state)
		{
			return lift_io(io_factory_t{}(state->logger, state->instance));
		}
	};
};

struct check_selected_device_t
{
	struct io_factory_t
	{
		struct action_t
		{
			VkPhysicalDevice device;
			types::VulkanQueueFamilyIdx queue_family_idx;
			constexpr bool operator()() const
			{
				CHECK(device);
				CHECK(queue_family_idx >= types::VulkanQueueFamilyIdx{0});
				VkPhysicalDeviceProperties device_properties;
				vkGetPhysicalDeviceProperties(device, &device_properties);
				WARN(device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);
				return true;
			}
		};

		constexpr auto operator()(
			VkPhysicalDevice physical_device,
			types::VulkanQueueFamilyIdx const queue_family_idx) const
		{
			return IO{action_t{.device = physical_device, .queue_family_idx = queue_family_idx}};
		}
	};

	struct stateio_factory_t
	{
		constexpr auto operator()(
			std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx> device_and_queue_family) const
		{
			auto const [device, queue_family_idx] = device_and_queue_family;
			using vulkandemo::monad::stateio::get_state_t;

			return lift_io(io_factory_t{}(device, queue_family_idx));
		}
	};
};
}  // namespace select_device_with_capability

namespace create_logical_device_with_queues
{
constexpr types::VulkanQueueCount kExpectedQueueCount{2};

using QueueFamilyIdxAndCounts =
	immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>;

struct query_queues_and_check_t
{
	struct check_validity_of_queues_t
	{
		static constexpr auto operator()(
			types::VulkanDevicePtr const & device,
			types::VulkanQueueFamilyIdx const queue_family_idx,
			types::MapOfVulkanQueueFamilyIdxToVectorOfQueues const & queues)
		{
			CHECK(device);
			CHECK(queues.size() == 1);
			CHECK(queues.at(queue_family_idx).size() == kExpectedQueueCount);
			CHECK(queues.at(queue_family_idx)[0]);
			CHECK(queues.at(queue_family_idx)[1]);
			return true;
		}

		struct with_device_and_queue_family_idx_t
		{
			types::VulkanDevicePtr device;
			types::VulkanQueueFamilyIdx queue_family_idx;

			constexpr auto operator()(
				types::MapOfVulkanQueueFamilyIdxToVectorOfQueues const & queues) const
			{
				return check_validity_of_queues_t{}(device, queue_family_idx, queues);
			}
		};
	};

	struct io_factory_t
	{
		static constexpr auto operator()(
			types::VulkanDevicePtr device,
			types::VulkanQueueFamilyIdx queue_family_idx,
			QueueFamilyIdxAndCounts queue_family_and_counts)
		{
			auto device_for_queue_query = device;
			auto device_for_queue_check = std::move(device);
			return monad::query_queues_for_queue_family_and_counts_t::io_factory_t{}(
					   std::move(device_for_queue_query), std::move(queue_family_and_counts))
				.fmap(
					check_validity_of_queues_t::with_device_and_queue_family_idx_t{
						.device = std::move(device_for_queue_check),
						.queue_family_idx = queue_family_idx});
		}
	};

	struct stateio_factory_t
	{
		constexpr auto operator()(
			types::VulkanQueueFamilyIdx queue_family_idx,
			QueueFamilyIdxAndCounts queue_family_and_counts,
			auto const & state) const
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(
				io_factory_t{}(state->device, queue_family_idx, queue_family_and_counts));
		}

		struct with_queue_family_idx_and_queue_counts_t
		{
			types::VulkanQueueFamilyIdx queue_family_idx;
			QueueFamilyIdxAndCounts queue_family_and_counts;
			constexpr auto operator()(this auto && self, auto const & state)
			{
				return stateio_factory_t{}(
					self.queue_family_idx, FW(self).queue_family_and_counts, state);
			}
		};
	};
};

struct create_device_and_queues_and_check_t
{
	struct stateio_factory_t
	{
		static constexpr auto operator()(
			std::optional<std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
				physical_device_and_queue_family)
		{
			using vulkandemo::monad::stateio::get_state_t;

			REQUIRE(physical_device_and_queue_family.has_value());
			auto const [physical_device, queue_family_idx] = *FW(physical_device_and_queue_family);
			QueueFamilyIdxAndCounts const queue_family_and_counts{
				std::pair{queue_family_idx, kExpectedQueueCount}};

			return monad::create_device_t::stateio_factory_t{}(
					   physical_device,
					   queue_family_and_counts,
					   {types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}})
				.then(get_state_t::stateio_factory_t{}())
				.bind(
					query_queues_and_check_t::stateio_factory_t::
						with_queue_family_idx_and_queue_counts_t{
							.queue_family_idx = queue_family_idx,
							.queue_family_and_counts = queue_family_and_counts});
		}
	};
};
}  // namespace create_logical_device_with_queues

namespace create_swapchain
{

struct query_and_filter_surface_formats_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr logger, VkPhysicalDevice physical_device, types::VulkanSurfacePtr surface)
		{
			return monad::query_available_surface_formats_t::io_factory_t{}(
					   physical_device, std::move(surface))
				.fmap(
					filter_surface_formats_t::with_logger_and_desired_formats_t{
						.logger = std::move(logger),
						.desired_formats =
							immer::array{{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}}})
				.fmap(hof::transform_range_to_front_elem_t{});
		}
	};

	struct stateio_factory_t
	{
		static constexpr auto operator()(
			LoggerPtr logger, types::VulkanSurfacePtr surface, VkPhysicalDevice physical_device)
		{
			using vulkandemo::monad::stateio::lift_io;
			return lift_io(io_factory_t{}(std::move(logger), physical_device, std::move(surface)));
		}

		struct from_state_t
		{
			VkPhysicalDevice physical_device;

			constexpr auto operator()(auto const & state) const
			{
				return stateio_factory_t{}(state->logger, state->surface, physical_device);
			}
		};

		struct with_physical_device_t
		{
			VkPhysicalDevice physical_device;

			constexpr auto operator()() const
			{
				using vulkandemo::monad::stateio::get_state;
				return get_state().bind(from_state_t{physical_device});
			}
		};
	};
};

struct exclusive_double_buffer_swapchain_create_info_t
{
	struct with_logger_surface_and_format_t
	{
		LoggerPtr logger;
		types::VulkanSurfacePtr surface;
		VkSurfaceFormatKHR surface_format;

		constexpr auto operator()(
			immer::array<VkPresentModeKHR> surface_present_modes,
			VkSurfaceCapabilitiesKHR const surface_capabilities) const
		{
			return exclusive_double_buffer_swapchain_create_info(
				logger,
				surface,
				surface_format,
				surface_capabilities,
				std::move(surface_present_modes));
		}
	};
};

struct swapchain_create_info_t
{
	struct io_factory_t
	{
		constexpr auto operator()(
			LoggerPtr logger,
			VkPhysicalDevice physical_device,
			types::VulkanSurfacePtr surface,
			VkSurfaceFormatKHR surface_format) const
		{
			auto surface_for_present_mode_query = surface;
			auto surface_for_capabilities_query = surface;
			auto surface_for_swapchain_create_info = std::move(surface);

			return sequence(
					   monad::query_present_modes_t::io_factory_t{}(
						   physical_device, std::move(surface_for_present_mode_query)),
					   monad::query_surface_capabilities_t::io_factory_t{}(
						   physical_device, std::move(surface_for_capabilities_query)))
				.fmap(
					exclusive_double_buffer_swapchain_create_info_t::
						with_logger_surface_and_format_t{
							.logger = std::move(logger),
							.surface = std::move(surface_for_swapchain_create_info),
							.surface_format = surface_format});
		}
	};

	struct stateio_factory_t
	{
		struct with_physical_device_and_surface_format_t
		{
			VkPhysicalDevice physical_device;
			VkSurfaceFormatKHR surface_format;
			constexpr auto operator()(auto const & state) const
			{
				return lift_io(
					io_factory_t{}(state->logger, physical_device, state->surface, surface_format));
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device, VkSurfaceFormatKHR surface_format)
		{
			using vulkandemo::monad::stateio::get_state_t;
			return get_state_t::stateio_factory_t{}().bind(
				with_physical_device_and_surface_format_t{
					.physical_device = physical_device, .surface_format = surface_format});
		}
	};
};

struct create_swapchain_and_image_views_t
{
	struct stateio_factory_t
	{
		struct action_t
		{
			VkPhysicalDevice physical_device;
			VkSurfaceFormatKHR surface_format;

			constexpr auto operator()(auto && state) const
			{
				return swapchain_create_info_t::stateio_factory_t{}(physical_device, surface_format)
					.bind(monad::create_swapchain_t::stateio_factory_t{})
					.then(get_state_t::stateio_factory_t{}())
					.bind(monad::query_swapchain_images_t::stateio_factory_t{})
					.bind(
						monad::create_colour_aspect_single_mip_single_layer_image_views_t::
							stateio_factory_t::with_surface_format_t{surface_format})(FW(state));
			}
		};

		static constexpr auto operator()(
			VkPhysicalDevice physical_device, VkSurfaceFormatKHR surface_format)
		{
			return StateIO{
				action_t{.physical_device = physical_device, .surface_format = surface_format}};
		}

		struct with_physical_device_t
		{
			VkPhysicalDevice physical_device;
			constexpr auto operator()(VkSurfaceFormatKHR surface_format) const
			{
				return stateio_factory_t{}(physical_device, surface_format);
			}
		};
	};
};

struct query_surface_formats_and_create_swapchain_and_image_views_t
{
	struct stateio_factory_t
	{
		static constexpr auto operator()(VkPhysicalDevice physical_device)
		{
			return query_and_filter_surface_formats_t::stateio_factory_t::with_physical_device_t{
				physical_device}()
				.bind(
					create_swapchain_and_image_views_t::stateio_factory_t::with_physical_device_t{
						physical_device});
		}
	};
};

struct check_swapchain_t
{
	struct io_factory_t
	{
		struct action_t
		{
			types::VulkanSwapchainPtr swapchain;
			immer::array<types::VulkanImageViewPtr> image_views;
			constexpr auto operator()() const
			{
				CHECK(swapchain);
				CHECK(!image_views.empty());
				WARN(image_views.size() == 2);
				return true;
			}
		};

		static constexpr auto operator()(
			types::VulkanSwapchainPtr swapchain,
			immer::array<types::VulkanImageViewPtr> image_views)
		{
			return IO{
				action_t{.swapchain = std::move(swapchain), .image_views = std::move(image_views)}};
		}
	};

	struct stateio_factory_t
	{
		struct with_image_views_t
		{
			immer::array<types::VulkanImageViewPtr> image_views;
			constexpr auto operator()(this auto && self, auto const & state)
			{
				return lift_io(io_factory_t{}(state->swapchain, FW(self).image_views));
			}
		};

		static constexpr auto operator()(immer::array<types::VulkanImageViewPtr> image_views)
		{
			using vulkandemo::monad::stateio::get_state_t;
			return get_state_t::stateio_factory_t{}().bind(
				with_image_views_t{std::move(image_views)});
		}
	};
};

struct create_and_check_swapchain_for_physical_device_and_queue_family_t
{
	struct stateio_factory_t
	{
		static constexpr auto operator()(
			std::optional<std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
				physical_device_and_queue_family)
		{
			REQUIRE(physical_device_and_queue_family.has_value());
			auto const [physical_device, queue_family_idx] = *physical_device_and_queue_family;
			immer::array queue_family_and_counts{
				std::pair{queue_family_idx, types::VulkanQueueCount{1}}};

			return monad::create_surface_t::stateio_factory_t{}()
				.then(
					monad::create_device_t::stateio_factory_t{}(
						physical_device,
						std::move(queue_family_and_counts),
						{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}))
				.then(
					query_surface_formats_and_create_swapchain_and_image_views_t::
						stateio_factory_t{}(physical_device))
				.bind(check_swapchain_t::stateio_factory_t{});
		}
	};
};
}  // namespace create_swapchain

namespace monad_utilities
{
struct evens_filter_t
{
	struct io_factory_t
	{
		struct action_t
		{
			int val;
			constexpr std::optional<int> operator()() const
			{
				return ((val % 2) != 0) ? std::optional{val} : std::nullopt;
			}
		};

		static constexpr auto operator()(int val)
		{
			return IO{action_t{val}};
		}
	};
};
}  // namespace monad_utilities
}  // namespace test
}  // namespace

TEST_CASE("Monad utilites")
{
	using namespace test::monad_utilities;
	SUBCASE("sequence - array of sync IO")
	{
		namespace io = vulkandemo::monad::io;

		auto const program = io::sequence(immer::vector{{io::pure(1), io::pure(2), io::pure(3)}});
		// .fmap([](auto val) { return val; });

		auto result = program().sync_wait();
		CHECK(result == immer::vector{{1, 2, 3}});
	}

	SUBCASE("traverse - array of sync IO")
	{
		namespace io = vulkandemo::monad::io;

		auto const program = io::traverse_t::io_factory_t::with_kleisli_t{
			[](auto val) { return io::pure(val); }}(immer::vector{{1, 2, 3}});

		auto result = program().sync_wait();
		CHECK(result == immer::vector{{1, 2, 3}});
	}

	SUBCASE("filter by IO")
	{
		namespace io = vulkandemo::monad::io;

		auto const program =
			io::pure(immer::vector{{2, 3, 4}}).filter(evens_filter_t::io_factory_t{});

		auto result = program().sync_wait();
		CHECK(result == immer::vector{{3}});
	}
}

TEST_CASE("Create a window")
{
	using namespace test::create_a_window;
	using monad::create_window_t;
	// Create a window.
	auto const program =
		create_window_t::io_factory_t{}(kExpectedName, kExpectedWidth, kExpectedHeight)
			.bind(check_window_t::io_factory_t{});

	CHECK(program().sync_wait(4));
}

TEST_CASE("Create a Vulkan instance")
{
	// namespace di = boost::di;

	// NOLINTBEGIN(*-avoid-c-arrays)
	// auto const injector = di::make_injector(
	// 	di::bind<vulkandemo::LoggerPtr::element_type>.to(
	// 		vulkandemo::create_logger("Create a Vulkan instance")).in(di::singleton),
	// 	di::bind<vulkandemo::types::SDLWindowPtr::element_type>.to(create_window("", 0, 0)),
	// 	di::bind<types::AvailableInstanceLayerNameCstr[]>.to(
	// 		[](auto const & injector)
	// 		{
	// 			return filter_available_layers(
	// 				injector.template create<vulkandemo::LoggerPtr>(),
	// 				{types::DesiredInstanceLayerNameView{"some_unavailable_layer"},
	// 				 types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}});
	// 		}).in(di::singleton),
	// 	di::bind<types::AvailableInstanceExtensionNameCstr[]>.to(
	// 		[](auto const & injector)
	// 		{
	// 			return filter_available_instance_extensions(
	// 				injector.template create<vulkandemo::LoggerPtr>(),
	// 				{types::DesiredInstanceExtensionNameView{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
	// 				 types::DesiredInstanceExtensionNameView{"some_unavailable_extension"}});
	// 		}).in(di::singleton),
	// 	di::bind<vulkandemo::types::VulkanInstancePtr>().to(
	// 		[](auto const & injector) -> types::VulkanInstancePtr // NOLINT(*-trailing-return)
	// 		{ return create_vulkan_instance(injector.template create<vulkandemo::LoggerPtr>(),
	// 			injector.template create<vulkandemo::types::SDLWindowPtr>(),
	// 			injector.template create<std::vector<types::AvailableInstanceLayerNameCstr>
	// const&>(), 			injector.template
	// create<std::vector<types::AvailableInstanceExtensionNameCstr> const&>());
	// }).in(di::scopes::instance{})); NOLINTEND(*-avoid-c-arrays)

	// auto const & instance = di::create<vulkandemo::types::VulkanInstancePtr>(injector);
	auto const logger = create_logger("Create a Vulkan instance (io)");

	using namespace test::create_a_vulkan_instance;

	auto const program =
		// Create application window.
		monad::create_window_t::io_factory_t{}("", 0, 0)
			.bind(
				query_layers_and_extensions_and_create_instance_t::io_factory_t::with_logger_t{
					logger})
			.bind(check_instance_t::io_factory_t{});

	CHECK(program().sync_wait());
}

TEST_CASE("Create a Vulkan debug utils messenger")
{
	LoggerPtr const logger = create_logger("Create a Vulkan debug utils messenger");

	using namespace test::create_a_vulkan_debug_utils_messenger;

	auto const program =
		test::query_desired_instance_extensions_t::io_factory_t{}(logger)
			.bind(create_instance_with_extensions_t::io_factory_t::with_logger_t{logger})
			.bind(create_debug_messenger_t::io_factory_t::with_logger_t{logger})
			.bind(check_messenger_t::io_factory_t{});

	CHECK(program().sync_wait());
}

TEST_CASE("Create a Vulkan surface")
{
	using namespace test::create_a_vulkan_surface;
	namespace stateio = vulkandemo::monad::stateio;

	auto const program = test::create_default_instance_t::stateio_factory_t{}().then(
		stateio::lift_readerio(create_surface_t::readerio_factory_t{}()).fmap(check_surface_t{}));

	struct state_t
	{
		LoggerPtr logger = create_logger("Create a Vulkan surface");
		types::VulkanDebugMessengerPtr messenger;
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
	} initial_state_v;
	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto const [result, state] = program(std::move(initial_state))().sync_wait(lf::unit_pool{});
	CHECK(result);

	// Checking error reporting

	// monad::IO iom{[] { return false; }};
	// iom.bind([](std::string) { return monad::IO{[] { return true; }}; });

	// monad::StateIO s{[](auto const& state)
	// 						  { return monad::IO{[] { return true;
	// }}.pair_with(state); }};
	//
	// s.bind([](bool) { return false; })(initial_state);

	// To trace unknown memory addresses spat out by ASan/LSan
	// std::filesystem::copy_file(
	// 	std::filesystem::path{"/proc/self/maps"},
	// 	std::filesystem::path{"/tmp/maps"},
	// 	std::filesystem::copy_options::update_existing);
}

TEST_CASE("Enumerate devices")
{
	using namespace test::enumerate_devices;
	namespace stateio = vulkandemo::monad::stateio;
	auto const program =
		test::create_default_instance_t::stateio_factory_t{}()
			.bind(choose_physical_device_and_create_surface_t::stateio_factory_t{})
			.bind(
				stateio::lift_kleisli_t{
					check_supported_extensions_and_memory_types_and_queue_families_t::
						readerio_factory_t{}});

	struct state_t
	{
		LoggerPtr logger = create_logger("Enumerate devices");
		types::VulkanDebugMessengerPtr messenger;
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
	} initial_state_v;
	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto const [result, state] = program(std::move(initial_state))().sync_wait();
	CHECK(result);
}

/*
TEST_CASE("Select physical device")
{
	using namespace test::select_physical_device;
	using vulkandemo::monad::stateio::get_state_t;

	auto const program =
		test::create_default_instance_t::stateio_factory_t{}()
			.then(get_state_t::stateio_factory_t{}())
			.bind(select_physical_device_and_queue_family_and_check_t::stateio_factory_t{});

	struct state_t
	{
		LoggerPtr logger = create_logger("Select physical device");
		types::VulkanDebugMessengerPtr messenger;
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
	} initial_state_v;
	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto [result, _] = program(std::move(initial_state))().sync_wait();
	CHECK(result);
}

TEST_CASE("Select device with capability")
{
	using namespace test::select_device_with_capability;
	auto const program = test::create_default_instance_t::stateio_factory_t{}()
							 .bind(select_physical_device_with_requirements_t::stateio_factory_t{})
							 .bind(check_selected_device_t::stateio_factory_t{});

	struct state_t
	{
		LoggerPtr logger = create_logger("Select device with capability");
		types::VulkanDebugMessengerPtr messenger;
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
	} initial_state_v;
	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto const [result, state] = program(std::move(initial_state))().sync_wait();
	CHECK(result);
}

TEST_CASE("Create logical device with queues")
{
	auto const program =
		test::create_default_instance_and_physical_device_and_queue_family_t::stateio_factory_t{}()
			.bind(
				test::create_logical_device_with_queues::create_device_and_queues_and_check_t::
					stateio_factory_t{});

	struct state_t
	{
		LoggerPtr logger = create_logger("Create logical device with queues");
		types::VulkanDebugMessengerPtr messenger;
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
		types::VulkanDevicePtr device;
	} initial_state_v;
	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto const [result, state] = program(std::move(initial_state))().sync_wait();
	CHECK(result);
}

TEST_CASE("Create swapchain")
{
	SUBCASE("Create swapchain and image views")
	{
		auto const program =
			test::create_default_instance_and_physical_device_and_queue_family_t::
				stateio_factory_t{}()
					.bind(
						test::create_swapchain::
							create_and_check_swapchain_for_physical_device_and_queue_family_t::
								stateio_factory_t{});

		struct state_t
		{
			LoggerPtr logger = create_logger("Create swapchain and image views");
			types::VulkanDebugMessengerPtr messenger;
			types::SDLWindowPtr window;
			types::VulkanInstancePtr instance;
			types::VulkanSurfacePtr surface;
			types::VulkanDevicePtr device;
			types::VulkanSwapchainPtr swapchain;
			immer::array<types::VulkanImageViewPtr> image_views;
		} initial_state_v;
		immer::box<state_t> initial_state{std::move(initial_state_v)};

		auto const [result, state] = program(std::move(initial_state))().sync_wait();
		CHECK(result);
	}

	LoggerPtr const logger = vulkandemo::create_logger("Create swapchain");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(logger, window, {}, {});
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VK_QUEUE_GRAPHICS_BIT,
		0,
		surface);

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	immer::array<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
		logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	REQUIRE(!available_formats.empty());
	VkSurfaceFormatKHR const surface_format = available_formats.front();

	auto [swapchain, image_views] = create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, surface_format);

	CHECK(swapchain);
	CHECK(!image_views.empty());
	WARN(image_views.size() == 2);

	// Reuse swapchain
	std::tie(swapchain, image_views) = create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, surface_format, swapchain);

	CHECK(swapchain);
	CHECK(!image_views.empty());
	WARN(image_views.size() == 2);
}

TEST_CASE("Create render pass")
{
	LoggerPtr const logger = vulkandemo::create_logger("Create render pass");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VK_QUEUE_GRAPHICS_BIT,
		0,
		surface);

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	immer::array<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
		logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	auto render_pass =
		create_single_presentation_subpass_render_pass(available_formats.at(0).format, device);

	CHECK(render_pass);
}

TEST_CASE("Create frame buffers")
{
	LoggerPtr const logger = create_logger("Create frame buffers");
	types::SDLWindowPtr const window = create_window("", 1, 2);
	VkExtent2D const drawable_size = window_drawable_size(window);
	CHECK(drawable_size.width > 0);
	CHECK(drawable_size.height > 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VkQueueFlagBits{},
		0,
		surface);

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	immer::array<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
		logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	auto [swapchain, image_views] = create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto render_pass =
		create_single_presentation_subpass_render_pass(available_formats.at(0).format, device);

	immer::array<types::VulkanFramebufferPtr> const frame_buffers =
		create_per_image_frame_buffers(device, render_pass, image_views, drawable_size);

	CHECK(frame_buffers.size() == image_views.size());
}

TEST_CASE("Create command buffers")
{
	LoggerPtr const logger = create_logger("Create command buffers");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		{},
		0,
		surface);

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	types::VulkanCommandPoolPtr const command_pool = create_command_pool(device, queue_family_idx);

	CHECK(command_pool);

	types::VulkanCommandBuffersPtr const command_buffer =
		create_primary_command_buffers(device, command_pool, types::VulkanCommandBufferCount{2});

	CHECK(command_buffer->size() == 2);
}

TEST_CASE("Create semaphores")
{
	LoggerPtr const logger = create_logger("Create semaphores");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		{},
		0,
		surface);

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	types::VulkanSemaphorePtr semaphore = create_semaphore(device);

	CHECK(semaphore);
}
*/
// NOLINTEND(readability-function-cognitive-complexity,*-using-namespace)
}  // namespace vulkandemo::setup
