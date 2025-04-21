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

#include "Logger.hpp"
#include "hof.hpp"
#include "macros.hpp"
#include "monad.hpp"
#include "setup/filters.hpp"
#include "setup/logging.hpp"
#include "setup/monad.hpp"
#include "types.hpp"

using namespace std::literals;

namespace vulkandemo::setup
{

std::vector<VkSurfaceFormatKHR> filter_available_surface_formats(
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

	std::vector<VkSurfaceFormatKHR> const filtered_surface_formats =
		available_surface_formats |
		std::views::filter(
			[&](auto const & available_surface_format)
			{ return std::ranges::contains(desired_formats, available_surface_format.format); }) |
		ranges::to<std::vector>();

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
	std::vector<VkPhysicalDevice> const & physical_devices,
	std::set<types::DesiredDeviceExtensionNameView> const & required_device_extensions,
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
	std::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names)
{
	if (desired_device_extension_names.empty())
		return {};

	// Get available device extension names.
	std::vector<VkExtensionProperties> const available_device_extensions =
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

std::vector<types::AvailableInstanceLayerNameCstr> filter_available_layers(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names)
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

	// Get intersection of desired layers and available layers, converted to C strings.
	return ranges::views::set_intersection(
			   desired_layer_names | hof::views::value_of(),
			   available_layer_names | hof::views::value_of()) |
		ranges::views::transform(&std::string_view::data) |
		ranges::to<std::vector<types::AvailableInstanceLayerNameCstr>>;
}

std::vector<types::AvailableInstanceExtensionNameCstr> filter_available_instance_extensions(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names)
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
	std::vector const extensions_to_enable =
		ranges::views::set_intersection(
			desired_extension_names | hof::views::value_of(),
			available_extension_names | hof::views::value_of()) |
		std::views::transform(&std::string_view::data) |
		ranges::to<std::vector<types::AvailableInstanceExtensionNameCstr>>;

	log_instance_extensions_info(
		logger, desired_extension_names, available_extension_names, available_extensions);

	return extensions_to_enable;
}

// NOLINTBEGIN(readability-function-cognitive-complexity)

using monad::io::IO;
using monad::stateio::StateIO;

TEST_CASE("Create a window")
{
	static constexpr int kExpectedWidth = 800;
	static constexpr int kExpectedHeight = 600;
	static constexpr auto kExpectedName = "Hello Vulkan";

	// Create a window.
	auto const program = monad::io::create_window(kExpectedName, kExpectedWidth, kExpectedHeight)
							 .bind(
								 [](AUTO(types::SDLWindowPtr) window)
								 {
									 CHECK(window);

									 return IO{[window = FW(window)]
											   {
												   int width = 0;
												   int height = 0;
												   SDL_GetWindowSize(window.get(), &width, &height);
												   CHECK(width == kExpectedWidth);
												   CHECK(height == kExpectedHeight);
												   CHECK(
													   SDL_GetWindowTitle(window.get()) ==
													   std::string_view{kExpectedName});
												   return true;
											   }};
								 });

	CHECK(program());
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

	auto const program =
		// Create application window.
		monad::io::create_window("", 0, 0)
			.bind(
				[logger](AUTO(types::SDLWindowPtr) window)
				{
					return bind(
						// Fetch and filter layer names.
						monad::io::query_available_instance_layers().fmap(
							make_layer_description_filter_by_and_transform_to_instance_layer_name(
								logger,
								std::set{
									types::DesiredInstanceLayerNameView{"some_unavailable_layer"},
									types::DesiredInstanceLayerNameView{
										"VK_LAYER_KHRONOS_validation"}})),

						// Fetch and filter extension names.
						fmap(
							// Get SDL window vulkan extension names.
							monad::io::query_sdl_instance_extension_names(window),
							// Fetch and filter additional extension names.
							monad::io::query_available_instance_extensions().fmap(
								make_extension_properties_filter_by_and_transform_to_instance_extension_name(
									logger,
									std::set{types::DesiredInstanceExtensionNameView{
										VK_EXT_DEBUG_UTILS_EXTENSION_NAME}})),
							// Concatenate SDL and optional extensions.
							hof::make_concat()),

						// Get title of window to use as app/engine name in vulkan.
						monad::io::window_title(window),

						[logger](
							AUTO(std::vector<types::AvailableInstanceLayerNameCstr>) desired_layers,
							AUTO(std::vector<types::AvailableInstanceExtensionNameCstr>)
								desired_extensions,
							char const * app_name)
						{
							return monad::io::create_instance(
								logger, app_name, FW(desired_layers), FW(desired_extensions));
						});
				})
			.bind(
				[](AUTO(types::VulkanInstancePtr) instance)
				{
					return IO{[instance = FW(instance)]
							  {
								  CHECK(instance);
								  return true;
							  }};
				});

	CHECK(program());
}

TEST_CASE("Create a Vulkan debug utils messenger")
{
	LoggerPtr const logger = create_logger("Create a Vulkan debug utils messenger");

	auto const program =
		bind(
			monad::io::create_window("", 0, 0),
			monad::io::query_available_instance_extensions().fmap(
				make_extension_properties_filter_by_and_transform_to_instance_extension_name(
					logger,
					std::set{types::DesiredInstanceExtensionNameView{
						VK_EXT_DEBUG_UTILS_EXTENSION_NAME}})),
			[logger]([[maybe_unused]] auto && window, auto && available_extensions)
			{
				return monad::io::create_instance(
					logger,
					"test",
					std::array<types::AvailableInstanceLayerNameCstr, 0>{},
					FW(available_extensions));
			})
			.bind([logger](auto && instance)
				  { return monad::io::create_debug_messenger(logger, FW(instance)); })
			.bind(
				[](auto && messenger)
				{
					return IO{[messenger = FW(messenger)]
							  {
								  CHECK(messenger);
								  return true;
							  }};
				});

	CHECK(program());
}

namespace
{
auto bind_to_default_instance(auto && io_from_state)
{
	// Create application window.
	return monad::stateio::create_window("", 0, 0)
		// Gather arguments for constructing a vulkan instance.
		.bind(
			[](AUTO(types::SDLWindowPtr) window)
			{
				return StateIO{
					[window = FW(window)](auto && state)
					{
						return zip(
								   // Get title of window to use as app/engine name in vulkan.
								   monad::io::window_title(window),
								   // Fetch and filter layer names.
								   monad::io::query_available_instance_layers().fmap(
									   make_layer_description_filter_by_and_transform_to_instance_layer_name(
										   state.logger,
										   std::set{types::DesiredInstanceLayerNameView{
											   "VK_LAYER_KHRONOS_validation"}})),

								   // Fetch and filter extension names.
								   fmap(
									   // Get SDL window requiredvulkan extension names.
									   monad::io::query_sdl_instance_extension_names(window),
									   // Fetch and filter additional extension names.
									   monad::io::query_available_instance_extensions().fmap(
										   make_extension_properties_filter_by_and_transform_to_instance_extension_name(
											   state.logger,
											   std::set{types::DesiredInstanceExtensionNameView{
												   VK_EXT_DEBUG_UTILS_EXTENSION_NAME}})),
									   // Concatenate SDL and optional extensions.
									   hof::make_concat()))
							.pair_with(FW(state));
					}};
			})
		// Create/store Vulkan instance
		.bind(
			[](auto && instance_args)
			{
				return std::apply(
					[](auto &&... args) { return monad::stateio::create_instance(FW(args)...); },
					FW(instance_args));
			})
		// Create/store debug messenger callback closure.
		.bind([](AUTO(types::VulkanInstancePtr) instance)
			  { return monad::stateio::create_debug_messenger(FW(instance)); })
		// Continue on to provided function
		.bind([io_from_state = FW(io_from_state)]([[maybe_unused]] auto && messenger)
			  { return StateIO{io_from_state}; });
}
}  // namespace

TEST_CASE("Create a Vulkan surface")
{
	auto const program =
		bind_to_default_instance(
			[](auto && state)
			{
				return monad::io::create_surface(state.window, state.instance).pair_with(FW(state));
			})
			.bind(
				[](auto && surface)
				{
					return vulkandemo::monad::stateio::lift(
						IO{[surface = FW(surface)]
						   {
							   CHECK(surface);
							   return true;
						   }});
				});

	const struct
	{
		LoggerPtr logger = create_logger("Create a Vulkan surface");
	} initial_state;

	auto const [result, state] = program(initial_state)();
	CHECK(result);

	// Checking error reporting

	// monad::io::IO iom{[] { return false; }};
	// iom.bind([](std::string) { return monad::io::IO{[] { return true; }}; });

	// monad::io::stateio::StateIO s{[](auto && state)
	// 						  { return monad::io::IO{[] { return true;
	// }}.pair_with(state); }};
	//
	// s.bind([](bool) { return false; })(initial_state);

	// To trace unknown memory addresses spat out by ASan/LSan
	// std::filesystem::copy_file(
	// 	std::filesystem::path{"/proc/self/maps"},
	// 	std::filesystem::path{"/tmp/maps"},
	// 	std::filesystem::copy_options::update_existing);
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
TEST_CASE("Enumerate devices")
{
	auto const program = bind_to_default_instance(
		[](auto && state)
		{
			return monad::io::
				bind(monad::io::enumerate_physical_devices(state.logger, state.instance),
					 monad::io::create_surface(state.window, state.instance),
					 [state](
						 AUTO(std::vector<VkPhysicalDevice>) physical_devices,
						 AUTO(types::VulkanSurfacePtr) surface)
					 {
						 REQUIRE(!physical_devices.empty());

						 VkPhysicalDevice chosen_device = physical_devices.front();

						 return bind(
							 monad::io::query_available_device_extensions(chosen_device)
								 .fmap(
									 make_extension_properties_filter_by_and_transform_to_device_extension_name(
										 state.logger,
										 chosen_device,
										 std::set{types::DesiredDeviceExtensionNameView{
											 VK_KHR_SWAPCHAIN_EXTENSION_NAME}})),
							 monad::io::query_physical_device_memory_properties(
								 state.logger, chosen_device)
								 .fmap(
									 make_memory_properties_filter_by_and_transform_to_memory_type_idx(
										 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)),
							 monad::io::query_available_queue_family_properties(chosen_device)
								 .fmap(
									 make_queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
										 VK_QUEUE_GRAPHICS_BIT))
								 .filter(
									 monad::io::
										 make_query_is_queue_family_supported_by_physical_device_and_surface(
											 chosen_device, surface)),
							 [](auto && available_device_extensions,
								auto && available_memory_types,
								auto && available_queue_families)
							 {
								 return IO{
									 [available_device_extensions = FW(available_device_extensions),
									  available_memory_types = FW(available_memory_types),
									  available_queue_families = FW(available_queue_families)]
									 {
										 CHECK(!available_memory_types.empty());
										 CHECK(available_device_extensions.size() == 1);
										 CHECK(!available_queue_families.empty());
										 return true;
									 }};
							 });
					 })
					.pair_with(FW(state));
		});

	const struct
	{
		LoggerPtr logger = create_logger("Enumerate devices");
	} initial_state;

	auto const [result, state] = program(initial_state)();
	CHECK(result);
}
// NOLINTEND(readability-function-cognitive-complexity)

TEST_CASE("Select physical device")
{
	const struct
	{
		LoggerPtr logger = create_logger("Select physical device");
	} initial_state;

	auto const program = bind_to_default_instance(
		[](auto && state)
		{
			using monad::io::query_available_device_extensions;
			using monad::io::query_available_queue_family_properties;
			using monad::io::query_physical_device_memory_properties;
			using monad::io::make_query_is_queue_family_supported_by_physical_device_and_surface;
			using monad::io::query_physical_device_properties;

			return monad::io::create_surface(state.window, state.instance)
				.bind(
					[state](auto && surface)
					{
						return monad::io::enumerate_physical_devices(state.logger, state.instance)
							.filter(
								[state](auto && physical_device)
								{
									return query_physical_device_memory_properties(
											   state.logger, physical_device)
										.fmap(
											make_memory_properties_filter_by_and_transform_to_memory_type_idx(
												VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
										.as_bool();
								})
							.filter(
								[state](auto && physical_device)
								{
									return query_available_device_extensions(physical_device)
										.fmap(
											make_extension_properties_filter_by_and_transform_to_device_extension_name(
												state.logger,
												physical_device,
												std::set{types::DesiredDeviceExtensionNameView{
													VK_KHR_SWAPCHAIN_EXTENSION_NAME}}))
										.as_bool();
								})
							.filter(
								[state](auto && physical_device)
								{
									return query_physical_device_memory_properties(
											   state.logger, physical_device)
										.fmap(
											make_memory_properties_filter_by_and_transform_to_memory_type_idx(
												VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
										.as_bool();
								})
							.traverse(
								[surface = FW(surface)](auto physical_device)
								{
									return zip(query_physical_device_properties(physical_device),
											   query_available_queue_family_properties(
												   physical_device)
												   .fmap(
													   make_queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
														   VK_QUEUE_GRAPHICS_BIT))
												   .filter(
													   make_query_is_queue_family_supported_by_physical_device_and_surface(
														   physical_device, surface)))
										.fmap(
											[physical_device](auto && args)
											{
												auto && [physical_device_properties, filtered_queue_family_idxs] =
													FW(args);

												return maybe_score_physical_device_and_queue_family(
													physical_device,
													physical_device_properties,
													filtered_queue_family_idxs);
											});
								})
							.compact()
							.fmap(maybe_select_best_scoring_physical_device_and_queue_family_idx)
							.fmap(
								[](auto && maybe_selected_physical_device_and_queue_family_idx)
								{
									REQUIRE(maybe_selected_physical_device_and_queue_family_idx
												.has_value());
									auto const & [selected_device, queue_family_idx] =
										*maybe_selected_physical_device_and_queue_family_idx;
									CHECK(selected_device != nullptr);
									CHECK(queue_family_idx >= 0);
									return true;
								})
							.pair_with(state);
					});
		});

	auto [result, _] = program(initial_state)();
	CHECK(result);
}

namespace
{
auto bind_to_default_instance_and_physical_device_and_queue_family(auto && lifter)
{
	return bind_to_default_instance(
			   [](auto && state)
			   {
				   using monad::io::query_available_device_extensions;
				   using monad::io::query_available_queue_family_properties;
				   using monad::io::query_physical_device_memory_properties;
				   using monad::io::
					   make_query_is_queue_family_supported_by_physical_device_and_surface;
				   using monad::io::query_physical_device_properties;

				   return monad::io::create_surface(state.window, state.instance)
					   .bind(
						   [state](auto && surface)
						   {
							   return monad::io::enumerate_physical_devices(
										  state.logger, state.instance)
								   .filter(
									   [state](auto && physical_device)
									   {
										   return query_physical_device_memory_properties(
													  state.logger, physical_device)
											   .fmap(
												   make_memory_properties_filter_by_and_transform_to_memory_type_idx(
													   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
											   .as_bool();
									   })
								   .filter(
									   [state](auto && physical_device)
									   {
										   return query_available_device_extensions(physical_device)
											   .fmap(
												   make_extension_properties_filter_by_and_transform_to_device_extension_name(
													   state.logger,
													   physical_device,
													   std::set{
														   types::DesiredDeviceExtensionNameView{
															   VK_KHR_SWAPCHAIN_EXTENSION_NAME}}))
											   .as_bool();
									   })
								   .filter(
									   [state](auto && physical_device)
									   {
										   return query_physical_device_memory_properties(
													  state.logger, physical_device)
											   .fmap(
												   make_memory_properties_filter_by_and_transform_to_memory_type_idx(
													   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
											   .as_bool();
									   })
								   .traverse(
									   [surface = FW(surface)](auto physical_device)
									   {
										   return fmap(
											   query_physical_device_properties(physical_device),
											   query_available_queue_family_properties(
												   physical_device)
												   .fmap(
													   make_queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
														   VK_QUEUE_GRAPHICS_BIT))
												   .filter(
													   make_query_is_queue_family_supported_by_physical_device_and_surface(
														   physical_device, surface)),
											   [physical_device](
												   auto && physical_device_properties,
												   auto && filtered_queue_family_idxs)
											   {
												   return maybe_score_physical_device_and_queue_family(
													   physical_device,
													   physical_device_properties,
													   filtered_queue_family_idxs);
											   });
									   })
								   .compact()
								   .fmap(
									   maybe_select_best_scoring_physical_device_and_queue_family_idx)
								   .pair_with(state);
						   });
			   })
		.bind(FW(lifter));
}
}  // namespace

TEST_CASE("Select device with capability [deprecated]")
{
	auto const program = bind_to_default_instance(
		[](auto && state)
		{
			return (
					   monad::io::enumerate_physical_devices(state.logger, state.instance) >>
					   [state](auto && physical_devices)
					   {
						   return monad::io::select_physical_device(
							   state.logger,
							   FW(physical_devices),
							   std::set{types::DesiredDeviceExtensionNameView{
								   VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
							   VK_QUEUE_GRAPHICS_BIT,
							   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
					   } >>
					   [](auto && device_and_queue_family)
					   {
						   return IO{
							   [device_and_queue_family = FW(device_and_queue_family)]
							   {
								   auto const & [device, queue_family_idx] =
									   device_and_queue_family;
								   CHECK(device);
								   CHECK(queue_family_idx >= types::VulkanQueueFamilyIdx{0});
								   // Get device type.
								   VkPhysicalDeviceProperties device_properties;
								   vkGetPhysicalDeviceProperties(device, &device_properties);
								   // Should be sorted in order of GPU-first.
								   WARN(
									   device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);
								   return true;
							   }};
					   })
				.pair_with(FW(state));
		});

	const struct
	{
		LoggerPtr logger = create_logger("Select device with capability");
	} initial_state;

	auto const [result, state] = program(initial_state)();
	CHECK(result);
}

TEST_CASE("Create logical device with queues")
{
	static constexpr types::VulkanQueueCount kExpectedQueueCount{2};

	auto const program = bind_to_default_instance_and_physical_device_and_queue_family(
		[](auto && physical_device_and_queue_family)
		{
			REQUIRE(physical_device_and_queue_family.has_value());
			auto [physical_device, queue_family_idx] = *FW(physical_device_and_queue_family);

			std::array queue_family_and_counts{std::pair{queue_family_idx, kExpectedQueueCount}};

			return monad::stateio::create_device(
					   physical_device,
					   queue_family_and_counts,
					   std::array{types::AvailableDeviceExtensionNameView{
						   VK_KHR_SWAPCHAIN_EXTENSION_NAME}})
				.bind(
					[queue_family_idx, queue_family_and_counts]([[maybe_unused]] auto && device)
					{
						return StateIO{
							[queue_family_idx, queue_family_and_counts](auto && state)
							{
								return monad::io::query_queues_for_queue_family_and_counts(
										   state.device, queue_family_and_counts)
									.fmap(
										[queue_family_idx, device = state.device](auto && queues)
										{
											CHECK(device);
											// Check that the device has the expected
											// number of queues.
											CHECK(queues.size() == 1);
											CHECK(
												queues.at(queue_family_idx).size() ==
												kExpectedQueueCount);
											CHECK(queues.at(queue_family_idx)[0]);
											CHECK(queues.at(queue_family_idx)[1]);
											return true;
										})
									.pair_with(FW(state));
							}};
					});
		});

	const struct
	{
		LoggerPtr logger = create_logger("Create logical device with queues");
	} initial_state;

	auto const [result, state] = program(initial_state)();
	CHECK(result);
}

TEST_CASE("Create swapchain")
{
	LoggerPtr const logger = vulkandemo::create_logger("Create swapchain");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(logger, window, {}, {});
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto const program = bind_to_default_instance_and_physical_device_and_queue_family(
		[](auto && physical_device_and_queue_family)
		{
			REQUIRE(physical_device_and_queue_family.has_value());
			auto [physical_device, queue_family_idx] = *FW(physical_device_and_queue_family);

			return monad::stateio::create_surface()
				.bind(
					[physical_device](auto && surface)
					{
						return StateIO{
							[physical_device, surface = FW(surface)](auto && state)
							{
								return monad::io::query_available_surface_formats(
										   physical_device, surface)
									.fmap(make_filter_surface_formats(
										state.logger,
										std::array{
											VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}))

									.pair_with(FW(state));
							}};
					})
				.bind(
					[physical_device, queue_family_idx](auto && surface_formats)
					{
						REQUIRE(!surface_formats.empty());

						auto const surface_format = surface_formats.front();

						std::array const queue_family_and_counts{
							std::pair{queue_family_idx, types::VulkanQueueCount{1}}};

						return monad::stateio::create_device(
								   physical_device,
								   queue_family_and_counts,
								   std::array{types::AvailableDeviceExtensionNameView{
									   VK_KHR_SWAPCHAIN_EXTENSION_NAME}})
							.bind(
								[physical_device, surface_format](auto && device)
								{
									return StateIO{
										[physical_device, device = FW(device), surface_format](
											auto && state)
										{
											return fmap(
													   monad::io::query_surface_capabilities(
														   physical_device, state.surface),
													   monad::io::query_present_modes(
														   physical_device, state.surface),
													   [logger = state.logger,
														surface = state.surface,
														surface_format](
														   auto && surface_capabilities,
														   auto && surface_present_modes)
													   {
														   return exclusive_double_buffer_swapchain_create_info(
															   logger,
															   surface,
															   surface_format,
															   FW(surface_capabilities),
															   FW(surface_present_modes));
													   })
												.pair_with(FW(state));
										}};
								})
							.bind([](auto && create_info)
								  { return monad::stateio::create_swapchain(FW(create_info)); })
							.bind(
								[](auto && swapchain)
								{
									return StateIO{[swapchain = FW(swapchain)](auto && state)
												   {
													   return monad::io::query_swapchain_images(
																  state.device, swapchain)
														   .pair_with(FW(state));
												   }};
								})
							.bind(
								[surface_format](auto && images)
								{
									return monad::stateio::
										create_colour_aspect_single_mip_single_layer_image_views(
											surface_format, FW(images));
								})
							.bind(
								[](auto && image_views)
								{
									return StateIO{
										[image_views = FW(image_views)](auto && state)
										{
											return IO{[state, image_views]
													  {
														  CHECK(state.swapchain);
														  CHECK(!image_views.empty());
														  CHECK(image_views == state.image_views);
														  WARN(image_views.size() == 2);
														  return true;
													  }}
												.pair_with(FW(state));
										}};
								});
					});
		});

	const struct
	{
		LoggerPtr logger = create_logger("Create swapchain and image views");
	} initial_state;

	auto const [result, state] = program(initial_state)();
	CHECK(result);

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

	std::vector<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
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

	std::vector<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
		logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});
	auto const [format, color_space] = available_formats.at(0);

	auto render_pass = create_single_presentation_subpass_render_pass(format, device);

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

	std::vector<VkSurfaceFormatKHR> const available_formats = filter_available_surface_formats(
		logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	auto [swapchain, image_views] = create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto render_pass =
		create_single_presentation_subpass_render_pass(available_formats.at(0).format, device);

	std::vector<types::VulkanFramebufferPtr> const frame_buffers =
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

// NOLINTEND(readability-function-cognitive-complexity)
}  // namespace vulkandemo::setup
