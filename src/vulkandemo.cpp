
#include "vulkandemo.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>
#include <map>
#include <ranges>

#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

using namespace std::literals;

#define VK_CHECK(func, msg)                                                                \
	do { /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                   \
		if (const VkResult result = func; result != VK_SUCCESS)                            \
		{                                                                                  \
			throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
		}                                                                                  \
	} while (false)

namespace vulkandemo
{
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

Logger create_logger(const std::string & name)
{
	Logger logger = spdlog::stdout_color_mt(name);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%L] %v");
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
	logger->set_level(spdlog::level::trace);
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO
	logger->set_level(spdlog::level::info);
#endif
	return logger;
}

void vulkandemo(Logger const & logger)
{
	logger->info("vulkandemo: Hello World!");
}

struct VulkanApp
{
	using SDLWindowPtr = std::shared_ptr<SDL_Window>;
	static std::shared_ptr<SDL_Window> make_window_ptr(SDL_Window * window)
	{
		return {
			window,
			[](SDL_Window * const ptr)
			{
				if (ptr != nullptr)
					SDL_DestroyWindow(ptr);
			}};
	}

	using VulkanInstancePtr = std::shared_ptr<std::remove_pointer_t<VkInstance>>;
	static VulkanInstancePtr make_instance_ptr(VkInstance instance)
	{
		return {
			instance,
			// ReSharper disable once CppParameterMayBeConst
			[](VkInstance ptr)
			{
				if (ptr != nullptr)
					vkDestroyInstance(ptr, nullptr);
			}};
	}

	using VulkanSurfacePtr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;
	// ReSharper disable once CppParameterMayBeConst
	static VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface)
	{
		return VulkanSurfacePtr{
			surface,
			// ReSharper disable once CppParameterMayBeConst
			[instance = std::move(instance)](VkSurfaceKHR ptr)
			{
				if (ptr != nullptr)
					vkDestroySurfaceKHR(instance.get(), ptr, nullptr);
			}};
	}

	using VulkanDevicePtr = std::shared_ptr<std::remove_pointer_t<VkDevice>>;
	// ReSharper disable once CppParameterMayBeConst
	static VulkanDevicePtr make_device_ptr(VkDevice device)
	{
		return VulkanDevicePtr{
			device,
			// ReSharper disable once CppParameterMayBeConst
			[](VkDevice ptr)
			{
				if (ptr != nullptr)
					vkDestroyDevice(ptr, nullptr);
			}};
	}

	/**
	 * Given a physical device, desired queue types, and desired extensions, get a logical device
	 * and corresponding queues.
	 *
	 * @param physical_device
	 * @param queue_family_and_counts
	 * @param device_extension_names
	 * @return
	 */
	static std::tuple<VulkanDevicePtr, std::map<uint32_t, std::vector<VkQueue>>>
	create_device_and_queues(
		// ReSharper disable once CppParameterMayBeConst
		VkPhysicalDevice physical_device,
		std::vector<std::pair<uint32_t, uint32_t>> const & queue_family_and_counts,
		std::vector<std::string_view> const & device_extension_names)
	{
		std::vector<char const *> const device_extension_cstr_names = [&]
		{
			std::vector<char const *> out;
			std::ranges::transform(
				device_extension_names,
				std::back_inserter(out),
				std::mem_fn(&std::string_view::data));
			return out;
		}();

		std::vector<VkDeviceQueueCreateInfo> const queue_create_infos = [&]
		{
			std::vector<VkDeviceQueueCreateInfo> out;
			std::ranges::transform(
				queue_family_and_counts,
				back_inserter(out),
				[](auto const & queue_family_and_count)
				{
					auto const [queue_family_idx, queue_count] = queue_family_and_count;
					std::vector<float> const queue_priorities(1.0F, queue_count);

					VkDeviceQueueCreateInfo queue_create_info{
						.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
						.queueFamilyIndex = queue_family_idx,
						.queueCount = queue_count,
						.pQueuePriorities = queue_priorities.data()};

					return queue_create_info;
				});
			return out;
		}();

		VkDeviceCreateInfo const device_create_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
			.pQueueCreateInfos = queue_create_infos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(device_extension_cstr_names.size()),
			.ppEnabledExtensionNames = device_extension_cstr_names.data(),
		};

		VkDevice device = nullptr;
		VK_CHECK(
			vkCreateDevice(physical_device, &device_create_info, nullptr, &device),
			"Failed to create logical device");

		// Construct a map of queue family to vector of queues.
		std::map<uint32_t, std::vector<VkQueue>> queues = [&]
		{
			std::map<uint32_t, std::vector<VkQueue>> out;
			for (auto const & queue_family_and_count : queue_family_and_counts)
			{
				auto const [queue_family_idx, queue_count] = queue_family_and_count;

				for (uint32_t queue_idx = 0; queue_idx < queue_count; ++queue_idx)
				{
					VkQueue queue = nullptr;
					vkGetDeviceQueue(device, queue_family_idx, queue_idx, &queue);
					out[queue_family_idx].push_back(queue);
				}
			}
			return out;
		}();

		return {make_device_ptr(device), std::move(queues)};
	}

	static std::vector<std::string_view> filter_available_device_extensions(
		Logger const & logger,
		// ReSharper disable once CppParameterMayBeConst
		VkPhysicalDevice physical_device,
		std::set<std::string_view> const & desired_device_extension_names)
	{
		if (desired_device_extension_names.empty())
			return {};

		// Get available device extension names.
		std::vector<VkExtensionProperties> const available_device_extensions = [&]
		{
			uint32_t extension_count = 0;
			vkEnumerateDeviceExtensionProperties(
				physical_device, nullptr, &extension_count, nullptr);
			std::vector<VkExtensionProperties> out(extension_count);
			vkEnumerateDeviceExtensionProperties(
				physical_device, nullptr, &extension_count, out.data());
			return out;
		}();

		std::set<std::string_view> const available_device_extension_names = [&]
		{
			std::set<std::string_view> out;
			std::ranges::transform(
				available_device_extensions,
				inserter(out, end(out)),
				&VkExtensionProperties::extensionName);
			return out;
		}();

		std::vector<std::string_view> const extensions_to_enable = [&]
		{
			std::vector<std::string_view> out;
			out.reserve(desired_device_extension_names.size());
			std::ranges::set_intersection(
				desired_device_extension_names,
				available_device_extension_names,
				back_inserter(out));
			return out;
		}();

		std::vector<std::string_view> const unavailable_extensions = [&]
		{
			std::vector<std::string_view> out;
			out.reserve(desired_device_extension_names.size());
			std::ranges::set_difference(
				desired_device_extension_names,
				available_device_extension_names,
				back_inserter(out));
			return out;
		}();

		for (auto const & unavailable_extension : unavailable_extensions)
			logger->warn("Requested extension is not available: {}", unavailable_extension);

		if (logger->should_log(spdlog::level::debug))
		{
			// Log requested extensions and whether they are available.
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(physical_device, &device_properties);
			logger->debug(
				"Requested device extensions for device {}:", device_properties.deviceName);

			for (auto const & extension_name : desired_device_extension_names)
			{
				if (available_device_extension_names.contains(extension_name))
					logger->debug("\t{} (available)", extension_name);
				else
					logger->debug("\t{} (unavailable)", extension_name);
			}

			// Log all available extensions.
			logger->debug("Available device extensions:");
			for (auto const & extension_name : available_device_extension_names)
				logger->debug("\t{}", extension_name);
		}

		return extensions_to_enable;
	}

	/**
	 * Given a list of physical devices, pick the first that has a queue with desired capabilities.
	 *
	 * @param logger
	 * @param physical_devices
	 * @param desired_queue_capabilities
	 * @return
	 */
	static std::tuple<VkPhysicalDevice, uint32_t> select_physical_device(
		Logger const & logger,
		std::vector<VkPhysicalDevice> const & physical_devices,
		VkQueueFlagBits const desired_queue_capabilities)
	{
		for (VkPhysicalDevice const & physical_device : physical_devices)
		{
			std::vector<VkQueueFamilyProperties> const queue_family_properties = [&]
			{
				std::vector<VkQueueFamilyProperties> out;
				uint32_t queue_family_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(
					physical_device, &queue_family_count, nullptr);
				out.resize(queue_family_count);
				vkGetPhysicalDeviceQueueFamilyProperties(
					physical_device, &queue_family_count, out.data());
				return out;
			}();

			if (auto iter = std::ranges::find_if(
					queue_family_properties,
					[desired_queue_capabilities](auto const & prop)
					{ return (prop.queueFlags & desired_queue_capabilities) != 0U; });
				iter != queue_family_properties.end())
			{
				auto queue_family_idx =
					std::ranges::distance(cbegin(queue_family_properties), iter);

				if (logger->should_log(spdlog::level::debug))
				{
					VkPhysicalDeviceProperties device_properties;
					vkGetPhysicalDeviceProperties(physical_device, &device_properties);
					logger->debug(
						"Selected device: {} and queue {}",
						device_properties.deviceName,
						queue_family_idx);
				}

				return {physical_device, static_cast<uint32_t>(queue_family_idx)};
			}
		}

		throw std::runtime_error("Failed to find device with desired queue capabilities");
	}

	/**
	 * Get a list of all physical devices, sorted by most likely to be useful (discrete GPU, ...).
	 *
	 * @param logger
	 * @param instance
	 * @return
	 */
	static std::vector<VkPhysicalDevice> enumerate_physical_devices(
		Logger const & logger, VulkanInstancePtr const & instance)
	{
		std::vector<VkPhysicalDevice> physical_devices;
		uint32_t device_count = 0;
		VK_CHECK(
			vkEnumeratePhysicalDevices(instance.get(), &device_count, nullptr),
			"Failed to enumerate physical devices");
		physical_devices.resize(device_count);
		VK_CHECK(
			vkEnumeratePhysicalDevices(instance.get(), &device_count, physical_devices.data()),
			"Failed to enumerate physical devices");
		// Sort in order of discrete GPU, integrated GPU, virtual GPU, CPU, other.
		std::ranges::sort(
			physical_devices,
			[&](VkPhysicalDevice const & lhs, VkPhysicalDevice const & rhs)
			{
				VkPhysicalDeviceProperties lhs_properties;
				vkGetPhysicalDeviceProperties(lhs, &lhs_properties);
				VkPhysicalDeviceProperties rhs_properties;
				vkGetPhysicalDeviceProperties(rhs, &rhs_properties);

				if (lhs_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
					rhs_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					return false;
				if (lhs_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
					rhs_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
					return false;
				if (lhs_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU &&
					rhs_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
					return false;
				if (lhs_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU &&
					rhs_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU)
					return false;

				return true;
			});

		// Log device information.
		if (logger->should_log(spdlog::level::debug))
		{
			for (auto const & device : physical_devices)
			{
				VkPhysicalDeviceProperties device_properties;
				vkGetPhysicalDeviceProperties(device, &device_properties);
				// Log all device properties.
				logger->debug("Device: {}", device_properties.deviceName);
				logger->debug(
					"\tDevice Type: {}", string_VkPhysicalDeviceType(device_properties.deviceType));
			}
		}
		return physical_devices;
	}

	/**
	 * Create vulkan surface compatible with SDL window to render to.
	 * @param window
	 * @param instance
	 * @return
	 *
	 */
	static VulkanSurfacePtr create_surface(SDLWindowPtr const & window, VulkanInstancePtr instance)
	{
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (SDL_Vulkan_CreateSurface(window.get(), instance.get(), &surface) != SDL_TRUE)
			throw std::runtime_error{
				std::format("Failed to create Vulkan surface: {}", SDL_GetError())};

		return make_surface_ptr(std::move(instance), surface);
	}

	/**
	 * Create a window
	 *
	 * @param title The title of the window
	 * @param width The width of the window
	 * @param height The height of the window
	 * @return The window
	 */
	static SDLWindowPtr create_window(char const * title, int const width, int const height)
	{
		// Initialize SDL
		if (int const error_code = SDL_Init(SDL_INIT_VIDEO); error_code != 0)
			throw std::runtime_error{std::format("Failed to initialize SDL: {}", SDL_GetError())};

		// Initialize SDL_Vulkan
		// if (const int error_code = SDL_Vulkan_LoadLibrary(nullptr); error_code != 0)
		// 	throw std::runtime_error{
		// 		std::format("Failed to initialize SDL_Vulkan: {}", SDL_GetError())};

		SDL_Window * window = SDL_CreateWindow(
			title,
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width,
			height,
			SDL_WINDOW_VULKAN);
		if (window == nullptr)
			throw std::runtime_error{std::format("Failed to create window: {}", SDL_GetError())};

		return make_window_ptr(window);
	}

	/**
	 * Create VkInstance using given window and layers.
	 *
	 * @param sdl_window
	 * @param layers_to_enable
	 * @return
	 */
	static VulkanInstancePtr create_vulkan_instance(
		SDLWindowPtr const & sdl_window, std::vector<char const *> const & layers_to_enable)
	{
		// Application metadata.
		VkApplicationInfo const app_info = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = SDL_GetWindowTitle(sdl_window.get()),
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = SDL_GetWindowTitle(sdl_window.get()),
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_3,
		};

		// Get the available extensions from SDL
		std::vector<char const *> const extensions = [&]
		{
			std::vector<char const *> out;
			uint32_t extension_count = 0;
			SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, nullptr);
			out.resize(extension_count);
			SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, out.data());
			return out;
		}();

		VkInstanceCreateInfo const create_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app_info,
			.enabledLayerCount = static_cast<uint32_t>(layers_to_enable.size()),
			.ppEnabledLayerNames = layers_to_enable.empty() ? nullptr : layers_to_enable.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data(),
		};

		VkInstance instance = VK_NULL_HANDLE;
		VK_CHECK(
			vkCreateInstance(&create_info, nullptr, &instance), "Failed to create Vulkan instance");

		return make_instance_ptr(instance);
	}

	/**
	 * Query available layers vs. desired layers.

	 * @param logger
	 * @param desired_layer_names
	 * @return
	 */
	static std::vector<char const *> filter_available_layers(
		Logger const & logger, std::set<std::string_view> const & desired_layer_names)
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
		std::set<std::string_view> const available_layer_names = [&]
		{
			std::set<std::string_view> out;
			std::ranges::transform(
				available_layer_descs,
				std::inserter(out, end(out)),
				[](VkLayerProperties const & layer_desc)
				{ return std::string_view{static_cast<char const *>(layer_desc.layerName)}; });
			return out;
		}();

		// Get intersection of desired layers and available layers.
		std::vector<char const *> layers_to_enable = [&]
		{
			std::vector<std::string_view> layer_names;
			layer_names.reserve(desired_layer_names.size());
			std::ranges::set_intersection(
				desired_layer_names, available_layer_names, std::back_inserter(layer_names));
			std::vector<char const *> out;
			out.reserve(layer_names.size());
			std::ranges::transform(
				layer_names,
				std::back_inserter(out),
				[](std::string_view const & name) { return name.data(); });
			return out;
		}();

		log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

		return layers_to_enable;
	}

	/**
	 * Log layer availability vs desired.
	 *
	 * @param logger
	 * @param desired_layer_names
	 * @param available_layer_names
	 * @param available_layer_descs
	 */
	static void log_layer_info(
		Logger const & logger,
		std::set<std::string_view> const & desired_layer_names,
		std::set<std::string_view> const & available_layer_names,
		std::vector<VkLayerProperties> const & available_layer_descs)
	{
		// Get unavailable layers.
		std::vector<std::string_view> const unavailable_layers = [&]
		{
			std::vector<std::string_view> out;
			std::ranges::set_difference(
				desired_layer_names, available_layer_names, std::back_inserter(out));
			return out;
		}();

		// Warn on requested but unavailable layers.
		for (auto const & layer_name : unavailable_layers)
			logger->warn("Requested layer is not available: {}", layer_name);

		if (logger->should_log(spdlog::level::debug))
		{
			// Log requested layers.
			if (!desired_layer_names.empty())
			{
				logger->debug("Requested layers:");
				for (auto const & layer_name : desired_layer_names)
				{
					if (available_layer_names.contains(layer_name))
						logger->debug("\t{} (available)", layer_name);
					else
						logger->debug("\t{} (unavailable)", layer_name);
				}
			}

			// Log available layers.
			if (!available_layer_descs.empty())
			{
				logger->debug("Available layers:");
				for (auto const & [layerName, specVersion, implementationVersion, description] :
					 available_layer_descs)
				{
					logger->debug(
						"\t{} (spec version: {}.{}.{}, implementation version: {})",
						layerName,
						VK_VERSION_MAJOR(specVersion),
						VK_VERSION_MINOR(specVersion),
						VK_VERSION_PATCH(specVersion),
						implementationVersion);
					logger->debug("\t\t{}", description);
				}
			}
		}
	}
};
}  // namespace vulkandemo

TEST_CASE("Create a window")
{
	constexpr int expected_width = 800;
	constexpr int expected_height = 600;
	constexpr auto expected_name = "Hello Vulkan";

	// Create a window.
	vulkandemo::VulkanApp::SDLWindowPtr window =
		vulkandemo::VulkanApp::create_window(expected_name, expected_width, expected_height);
	// Check that the window was created.
	CHECK(window);
	// Check that the window has the given width and height.
	// Get the window's width and height.
	int width = 0;
	int height = 0;
	SDL_GetWindowSize(window.get(), &width, &height);
	CHECK(width == expected_width);
	CHECK(height == expected_height);
	// Check the window has the expected name.
	CHECK(SDL_GetWindowTitle(window.get()) == std::string_view{expected_name});
}

TEST_CASE("Create a Vulkan instance")
{
	using vulkandemo::VulkanApp;
	vulkandemo::Logger const logger = vulkandemo::create_logger("Create a Vulkan instance");
	VulkanApp::VulkanInstancePtr instance = VulkanApp::create_vulkan_instance(
		VulkanApp::create_window("", 0, 0),
		VulkanApp::filter_available_layers(
			logger, {"some_unavailable_layer", "VK_LAYER_KHRONOS_validation"}));
	CHECK(instance);
}

TEST_CASE("Create a Vulkan surface")
{
	using vulkandemo::VulkanApp;
	vulkandemo::Logger const logger = vulkandemo::create_logger("Create a Vulkan surface");
	VulkanApp::SDLWindowPtr const window = VulkanApp::create_window("", 0, 0);
	VulkanApp::VulkanInstancePtr const instance = VulkanApp::create_vulkan_instance(window, {});
	VulkanApp::VulkanSurfacePtr surface = VulkanApp::create_surface(window, instance);
	CHECK(surface);

	// To trace unknown memory addresses spat out by ASan/LSan
	// std::filesystem::copy_file(
	// 	std::filesystem::path{"/proc/self/maps"},
	// 	std::filesystem::path{"/tmp/maps"},
	// 	std::filesystem::copy_options::update_existing);
}

TEST_CASE("Enumerate devices")
{
	using vulkandemo::VulkanApp;
	vulkandemo::Logger const logger = vulkandemo::create_logger("Enumerate devices");
	VulkanApp::SDLWindowPtr const window = VulkanApp::create_window("", 0, 0);
	VulkanApp::VulkanInstancePtr const instance = VulkanApp::create_vulkan_instance(window, {});
	std::vector<VkPhysicalDevice> const physical_devices =
		VulkanApp::enumerate_physical_devices(logger, instance);

	CHECK(!physical_devices.empty());

	if (physical_devices.size() > 1)
	{
		VkPhysicalDeviceProperties first_device_properties;
		vkGetPhysicalDeviceProperties(physical_devices[0], &first_device_properties);
		// Should be sorted in order of GPU-first.
		CHECK(first_device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);
	}
}

TEST_CASE("Select device with capability")
{
	using vulkandemo::VulkanApp;
	vulkandemo::Logger const logger = vulkandemo::create_logger("Select device with capability");
	VulkanApp::SDLWindowPtr const window = VulkanApp::create_window("", 0, 0);
	VulkanApp::VulkanInstancePtr const instance = VulkanApp::create_vulkan_instance(window, {});
	std::vector<VkPhysicalDevice> const physical_devices =
		VulkanApp::enumerate_physical_devices(logger, instance);

	auto [device, queue_family_idx] =
		VulkanApp::select_physical_device(logger, physical_devices, VK_QUEUE_GRAPHICS_BIT);

	CHECK(device);
	CHECK(queue_family_idx >= 0);

	if (physical_devices.size() > 1)
	{
		// Get device type.
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(device, &device_properties);
		// Should be sorted in order of GPU-first.
		CHECK(device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);
	}
}

TEST_CASE("Create logical device with queues")
{
	using vulkandemo::VulkanApp;
	vulkandemo::Logger const logger =
		vulkandemo::create_logger("Create logical device with queues");
	VulkanApp::SDLWindowPtr const window = VulkanApp::create_window("", 0, 0);
	VulkanApp::VulkanInstancePtr const instance = VulkanApp::create_vulkan_instance(window, {});
	std::vector<VkPhysicalDevice> const physical_devices =
		VulkanApp::enumerate_physical_devices(logger, instance);
	auto [physical_device, queue_family_idx] =
		VulkanApp::select_physical_device(logger, physical_devices, VK_QUEUE_GRAPHICS_BIT);
	constexpr uint32_t expected_queue_count = 2;

	auto [device, queues] = VulkanApp::create_device_and_queues(
		physical_device,
		{{queue_family_idx, expected_queue_count}},
		VulkanApp::filter_available_device_extensions(
			logger, physical_device, {"VK_KHR_device_group", "some_unsupported_extension"}));

	CHECK(device);
	// Check that the device has the expected number of queues.
	CHECK(queues.size() == 1);
	CHECK(queues.at(queue_family_idx).size() == expected_queue_count);
	CHECK(queues.at(queue_family_idx)[0]);
	CHECK(queues.at(queue_family_idx)[1]);
}