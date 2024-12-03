#include "vulkandemo.hpp"

#include <format>

#include <SDL.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>
#include <doctest/doctest.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vulkandemo
{
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

Logger default_logger()
{
	Logger logger = spdlog::stdout_color_mt("console");
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
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

using SDLWindowPtr = std::unique_ptr<SDL_Window, decltype([](void * window)
{
	if (window != nullptr)
		SDL_DestroyWindow(static_cast<SDL_Window *>(window));
})>;

/**
 * Create a window
 *
 * @param title The title of the window
 * @param width The width of the window
 * @param height The height of the window
 * @return SDL_Window* The window
 */
static SDLWindowPtr create_window(const char * title, int width, int height)
{
	// Initialize SDL
	if (const int error_code = SDL_Init(SDL_INIT_VIDEO); error_code != 0)
	{
		throw std::runtime_error{std::format("Failed to initialize SDL: {}", SDL_GetError())};
	}

	// Initialize SDL_Vulkan
	if (const int error_code = SDL_Vulkan_LoadLibrary(nullptr); error_code != 0)
	{
		throw std::runtime_error{std::format("Failed to initialize SDL_Vulkan: {}", SDL_GetError())};
	}

	SDL_Window * window =
		SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_VULKAN);
	if (window == nullptr)
	{
		throw std::runtime_error{std::format("Failed to create window: {}", SDL_GetError())};
	}
	return SDLWindowPtr{window};
}  // namespace vulkandemo

TEST_CASE("Create a window")
{
	SUBCASE("successful creation")
	{
		constexpr int expected_width = 800;
		constexpr int expected_height = 600;
		constexpr const char * expected_name = "Hello Vulkan";

		// Create a window.
		SDLWindowPtr window = create_window(expected_name, expected_width, expected_height);
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
		SDLWindowPtr window = create_window("", 1, 1);
		CHECK(window);
	}
}
}  // namespace vulkandemo