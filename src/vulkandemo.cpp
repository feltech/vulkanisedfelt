#include "vulkandemo.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <memory>
#include <set>
#include <string_view>
#include <type_traits>
#include <vector>

#include <SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <doctest/doctest.h>

#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

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

Logger create_logger()
{
	Logger logger = spdlog::stdout_color_mt("console");
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%L] %v");
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
	logger->set_level(spdlog::level::trace);
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO
	logger->set_level(spdlog::level::info);
#endif
	return logger;
}

void vulkandemo(const Logger & logger)
{
	logger->info("vulkandemo: Hello World!");
}

namespace
{

using SDLWindowPtr = std::unique_ptr<SDL_Window, decltype([](void * window)
{
	if (window != nullptr)
		SDL_DestroyWindow(static_cast<SDL_Window *>(window));
})>;

using VulkanInstancePtr = std::unique_ptr<
	std::remove_pointer_t<VkInstance>,
	decltype([](void * instance)
			 { vkDestroyInstance(static_cast<VkInstance>(instance), nullptr); })>;

/**
 * Create a window
 *
 * @param title The title of the window
 * @param width The width of the window
 * @param height The height of the window
 * @return The window
 */
SDLWindowPtr create_window(const char * title, const int width, const int height)
{
	// Initialize SDL
	if (const int error_code = SDL_Init(SDL_INIT_VIDEO); error_code != 0)
		throw std::runtime_error{std::format("Failed to initialize SDL: {}", SDL_GetError())};

	// Initialize SDL_Vulkan
	if (const int error_code = SDL_Vulkan_LoadLibrary(nullptr); error_code != 0)
		throw std::runtime_error{
			std::format("Failed to initialize SDL_Vulkan: {}", SDL_GetError())};

	SDL_Window * window = SDL_CreateWindow(
		title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_VULKAN);
	if (window == nullptr)
		throw std::runtime_error{std::format("Failed to create window: {}", SDL_GetError())};

	return SDLWindowPtr{window};
}

// Helper function to log layer availability information
void log_layer_info(
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
	for (const auto & layer_name : unavailable_layers)
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
			for (const auto & [layerName, specVersion, implementationVersion, description] :
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

// Helper function to query and compare available vs desired layers
std::vector<const char *> filter_available_layers(
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
			{ return std::string_view{static_cast<const char *>(layer_desc.layerName)}; });
		return out;
	}();

	// Get intersection of desired layers and available layers.
	std::vector<const char *> layers_to_enable = [&]
	{
		std::vector<std::string_view> layer_names;
		layer_names.reserve(desired_layer_names.size());
		std::ranges::set_intersection(
			desired_layer_names, available_layer_names, std::back_inserter(layer_names));
		std::vector<const char *> out;
		out.reserve(layer_names.size());
		std::ranges::transform(
			layer_names,
			std::back_inserter(out),
			[](const std::string_view & sv) { return sv.data(); });
		return out;
	}();

	log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

	return layers_to_enable;
}

VulkanInstancePtr create_vulkan_instance(
	SDLWindowPtr const & sdl_window, std::vector<const char *> const & layers_to_enable)
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
	std::vector<const char *> const extensions = [&]
	{
		std::vector<const char *> out;
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

	return VulkanInstancePtr{instance};
}

}  // namespace
}  // namespace vulkandemo

TEST_CASE("Create a window")
{
	SUBCASE("successful creation")
	{
		constexpr int expected_width = 800;
		constexpr int expected_height = 600;
		constexpr auto expected_name = "Hello Vulkan";

		// Create a window.
		vulkandemo::SDLWindowPtr window =
			vulkandemo::create_window(expected_name, expected_width, expected_height);
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

	SUBCASE("creating again")
	{
		vulkandemo::SDLWindowPtr window = vulkandemo::create_window("", 1, 1);
		CHECK(window);
	}
}

TEST_CASE("Create a Vulkan instance")
{
	vulkandemo::Logger const logger = vulkandemo::create_logger();
	vulkandemo::VulkanInstancePtr instance = vulkandemo::create_vulkan_instance(
		vulkandemo::create_window("", 0, 0),
		vulkandemo::filter_available_layers(
			logger, {"some_unavailable_layer", "VK_LAYER_KHRONOS_validation"}));
	CHECK(instance);
}