// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// The following CLion check conflicts with clang-tidy wrt vulkan handle typedefs.
// ReSharper disable CppParameterMayBeConst

#include "vulkandemo.hpp"

#include <array>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL_events.h>
#include <SDL_video.h>

#include <fmt/format.h>

#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner) for `logger`

#include <vulkan/vk_enum_string_helper.h>  // NOLINT(misc-include-cleaner) for `VK_CHECK`
#include <vulkan/vulkan_core.h>

#include "Logger.hpp"
#include "draw.hpp"
#include "macros.hpp"
#include "setup.hpp"
#include "types.hpp"

namespace vulkandemo
{
using namespace std::literals;

void vulkandemo(LoggerPtr const & logger)  // NOLINT(readability-function-cognitive-complexity)
{
	types::SDLWindowPtr const window = setup::create_window("", 100, 100);

	std::vector<types::AvailableInstanceLayerNameCstr> const optional_layers =
		setup::filter_available_layers(
			logger, {types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}});

	std::vector<types::AvailableInstanceExtensionNameCstr> const optional_instance_extensions =
		setup::filter_available_instance_extensions(
			logger,
			{types::DesiredInstanceExtensionNameView{
				std::string_view{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});

	types::VulkanInstancePtr const instance = setup::create_vulkan_instance(
		logger, window, optional_layers, optional_instance_extensions);

	types::VulkanDebugMessengerPtr const messenger = optional_instance_extensions.empty()
		? nullptr
		: setup::create_debug_messenger(logger, instance);

	types::VulkanSurfacePtr const surface = setup::create_surface(window, instance);

	auto [physical_device, queue_family_idx] = setup::select_physical_device(
		logger,
		setup::enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VK_QUEUE_GRAPHICS_BIT,
		0,
		surface);

	auto [device, queues] = setup::create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, types::VulkanQueueCount{1}}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	auto const image_available_semaphore = setup::create_semaphore(device);
	auto const rendering_finished_semaphore = setup::create_semaphore(device);

	std::vector<VkSurfaceFormatKHR> const available_formats =
		setup::filter_available_surface_formats(
			logger, physical_device, surface, {{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	auto [swapchain, image_views] = setup::create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto const render_pass = setup::create_single_presentation_subpass_render_pass(
		available_formats.at(0).format, device);

	VkExtent2D drawable_size = setup::window_drawable_size(window);

	std::vector<types::VulkanFramebufferPtr> frame_buffers =
		setup::create_per_image_frame_buffers(device, render_pass, image_views, drawable_size);

	types::VulkanCommandPoolPtr const command_pool =
		setup::create_command_pool(device, queue_family_idx);

	types::VulkanCommandBuffersPtr const command_buffers = setup::create_primary_command_buffers(
		device, command_pool, types::VulkanCommandBufferCount{frame_buffers.size()});

	VkQueue queue = queues.at(queue_family_idx).front();

	types::VulkanClearColour clear_colour{std::array{1.0F, .0F, .0F, 1.0F}};

	// Application loop.
	while (true)
	{
		// SDL event loop.
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT)
				return;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				VK_CHECK(vkDeviceWaitIdle(device.get()), "Failed to wait for device to be idle");

				// Recreate swapchain and dependent resources
				drawable_size = setup::window_drawable_size(window);
				std::tie(swapchain, image_views) =
					setup::create_exclusive_double_buffer_swapchain_and_image_views(
						logger,
						physical_device,
						device,
						surface,
						available_formats.at(0),
						swapchain);

				frame_buffers = setup::create_per_image_frame_buffers(
					device, render_pass, image_views, drawable_size);

				clear_colour[0] = 1.0F - clear_colour[0];
				clear_colour[2] = 1.0F - clear_colour[2];

				logger->debug(
					"New drawable size ({}, {})", drawable_size.width, drawable_size.height);
				logger->debug("Changing clear colour to ({})", fmt::join(clear_colour, ","));
			}
		}

		auto const image_idx =
			draw::acquire_next_swapchain_image(device, swapchain, image_available_semaphore);

		if (!image_idx.has_value())
		{
			logger->debug("Swapchain out of date");
			continue;
		}

		VkCommandBuffer command_buffer = command_buffers->at(*image_idx);
		types::VulkanFramebufferPtr const & frame_buffer = frame_buffers.at(*image_idx);

		draw::populate_cmd_render_pass(
			command_buffer, render_pass, frame_buffer, drawable_size, clear_colour);

		draw::submit_command_buffer(
			queue, command_buffer, image_available_semaphore, rendering_finished_semaphore);

		draw::submit_present_image_cmd(queue, swapchain, *image_idx, rendering_finished_semaphore);

		VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue to be idle");
	}
}
}  // namespace vulkandemo