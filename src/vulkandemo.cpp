// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#include "vulkandemo.hpp"

#include <SDL_events.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <vulkan/vk_enum_string_helper.h>

#include "Logger.hpp"
#include "VulkanApp.hpp"

namespace vulkandemo
{
void vulkandemo(LoggerPtr const & logger)
{
	VulkanApp::SDLWindowPtr const window = VulkanApp::create_window("", 100, 100);

	auto const optional_layers =
		VulkanApp::filter_available_layers(logger, {"VK_LAYER_KHRONOS_validation"});
	auto const optional_instance_extensions = VulkanApp::filter_available_instance_extensions(
		logger, {VK_EXT_DEBUG_UTILS_EXTENSION_NAME});

	VulkanApp::VulkanInstancePtr const instance = VulkanApp::create_vulkan_instance(
		logger, window, optional_layers, optional_instance_extensions);

	VulkanApp::VulkanDebugMessengerPtr messenger = optional_instance_extensions.empty()
		? nullptr
		: VulkanApp::create_debug_messenger(logger, instance);

	VulkanApp::VulkanSurfacePtr const surface = VulkanApp::create_surface(window, instance);

	auto [physical_device, queue_family_idx] = VulkanApp::select_physical_device(
		logger,
		VulkanApp::enumerate_physical_devices(logger, instance),
		{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
		{VK_QUEUE_GRAPHICS_BIT},
		surface.get());

	auto [device, queues] = VulkanApp::create_device_and_queues(
		physical_device, {{queue_family_idx, 1}}, {VK_KHR_SWAPCHAIN_EXTENSION_NAME});

	auto const image_available_semaphore = VulkanApp::create_semaphore(device);
	auto const rendering_finished_semaphore = VulkanApp::create_semaphore(device);

	std::vector<VkSurfaceFormatKHR> const available_formats =
		VulkanApp::filter_available_surface_formats(
			logger, physical_device, surface, {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM});

	auto [swapchain, image_views] = VulkanApp::create_double_buffer_swapchain(
		logger, physical_device, device, surface, available_formats.at(0));

	auto const render_pass = VulkanApp::create_single_presentation_subpass_render_pass(
		available_formats.at(0).format, device);

	VkExtent2D drawable_size = VulkanApp::window_drawable_size(window);

	std::vector<VulkanApp::VulkanFramebufferPtr> frame_buffers =
		VulkanApp::create_per_image_frame_buffers(device, render_pass, image_views, drawable_size);

	VulkanApp::VulkanCommandPoolPtr const command_pool =
		VulkanApp::create_command_pool(device, queue_family_idx);

	VulkanApp::VulkanCommandBuffers command_buffers =
		VulkanApp::create_primary_command_buffers(device, command_pool, frame_buffers.size());

	VkQueue const queue = queues[queue_family_idx].front();

	std::array<float, 4> clear_colour = {1.0F, 0, 0, 1.0F};

	// Application loop.
	while (true)
	{
		// SDL event loop.
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				return;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				VK_CHECK(vkDeviceWaitIdle(device.get()), "Failed to wait for device to be idle");

				// Recreate swapchain and dependent resources
				drawable_size = VulkanApp::window_drawable_size(window);
				std::tie(swapchain, image_views) = VulkanApp::create_double_buffer_swapchain(
					logger, physical_device, device, surface, available_formats.at(0), swapchain);

				frame_buffers = VulkanApp::create_per_image_frame_buffers(
					device, render_pass, image_views, drawable_size);

				clear_colour[0] = 1.0F - clear_colour[0];
				clear_colour[2] = 1.0F - clear_colour[2];

				logger->debug(
					"New drawable size ({}, {})", drawable_size.width, drawable_size.height);
				logger->debug("Changing clear colour to ({})", fmt::join(clear_colour, ","));
			}
		}

		auto const image_idx =
			VulkanApp::acquire_next_swapchain_image(device, swapchain, image_available_semaphore);

		if (!image_idx.has_value())
		{
			logger->debug("Swapchain out of date");
			continue;
		}

		VkCommandBuffer const command_buffer = command_buffers.as_vector()[*image_idx];
		VulkanApp::VulkanFramebufferPtr const & frame_buffer = frame_buffers[*image_idx];

		VulkanApp::populate_cmd_render_pass(
			command_buffer, render_pass, frame_buffer, drawable_size, clear_colour);

		VulkanApp::submit_command_buffer(
			queue, command_buffer, image_available_semaphore, rendering_finished_semaphore);

		VulkanApp::submit_present_image_cmd(
			queue, swapchain, *image_idx, rendering_finished_semaphore);

		VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue to be idle");
	}
}
}  // namespace vulkandemo