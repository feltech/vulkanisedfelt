// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// The following CLion check conflicts with clang-tidy wrt vulkan handle typedefs.
// ReSharper disable CppParameterMayBeConst

#include "vulkandemo.hpp"

#include <array>
#include <immer/set.hpp>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL_events.h>
#include <SDL_video.h>

#include <fmt/format.h>

#include <immer/array.hpp>
#include <immer/box.hpp>

#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner) for `logger`

#include <vulkan/vk_enum_string_helper.h>  // NOLINT(misc-include-cleaner) for `VK_CHECK`
#include <vulkan/vulkan_core.h>

#include "Logger.hpp"
#include "draw.hpp"
#include "hof.hpp"
#include "monad/io.hpp"
#include "monad/readerio.hpp"
#include "monad/stateio.hpp"
#include "setup.hpp"
#include "setup/io.hpp"
#include "setup/monadic.hpp"
#include "types.hpp"

#include "macros_push.hpp"

namespace vulkandemo
{
using namespace std::literals;

void vulkandemo(LoggerPtr const & logger)  // NOLINT(readability-function-cognitive-complexity)
{
	struct state_t
	{
		LoggerPtr logger = create_logger("Create frame buffers");
		types::SDLWindowPtr window;
		types::VulkanInstancePtr instance;
		types::VulkanDebugMessengerPtr messenger;
		types::VulkanSurfacePtr surface;
		VkPhysicalDevice physical_device;
		immer::array<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount>>
			queue_family_and_counts;
		types::VulkanDevicePtr device;
		VkSurfaceFormatKHR surface_format{};
		types::VulkanSwapchainPtr swapchain;
		immer::array<types::VulkanImageViewPtr> image_views;
		types::VulkanRenderPassPtr render_pass;
	} initial_state_v;

	immer::box<state_t> initial_state{std::move(initial_state_v)};

	auto const program =
		setup::monadic::create_window_t::stateio_t{}("", 100, 100)
			.then(
				// Gather arguments for create_instance.
				monad::stateio::lift_readerio(
					monad::readerio::sequence(
						// arg: name
						monad::readerio::pure("the instance"),
						// arg: layers_to_enable
						monad::readerio::lift_io(
							setup::monadic::enumerate_instance_layer_properties_t::io_t{}())
							.bind(
								setup::monadic::
									layer_properties_filter_by_and_transform_to_instance_layer_name_t::
										readerio_t::with_desired_layer_names_t{
											{types::DesiredInstanceLayerNameView{
												"VK_LAYER_KHRONOS_validation"}}}),
						// arg: extensions_to_enable
						monad::readerio::sequence(
							// SDL window extensions
							setup::monadic::query_sdl_instance_extension_names_t::readerio_t{}(),
							// Other extensions
							monad::readerio::lift_io(
								setup::monadic::query_available_instance_extensions_t::io_t{}())
								.bind(
									// Filter down to desired extensions.
									setup::monadic::
										extension_properties_filter_by_and_transform_to_instance_extension_name_t::
											readerio_t::with_desired_extension_names_t{
												immer::set{{types::DesiredInstanceExtensionNameView{
													VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}}}))
							// Concatenate SDL and extra extensions.
							.fmap(hof::transform_concat_t{}))))
			// Create instance.
			.bind(setup::monadic::create_instance_t::stateio_t{});

	auto const [result, state] = program(std::move(initial_state))().sync_wait();

	types::SDLWindowPtr const window = setup::create_window("", 100, 100);

	immer::array<types::AvailableInstanceLayerNameCstr> const optional_layers =
		setup::filter_available_layers(
			logger, {{types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}}});

	immer::array<types::AvailableInstanceExtensionNameCstr> const optional_instance_extensions =
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

	immer::array<VkSurfaceFormatKHR> const available_formats =
		setup::filter_available_surface_formats(
			logger,
			physical_device,
			surface,
			{{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}});

	auto [swapchain, image_views] = setup::create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto const render_pass = setup::create_single_presentation_subpass_render_pass(
		available_formats.at(0).format, device);

	VkExtent2D drawable_size = setup::window_drawable_size(window);

	immer::array<types::VulkanFramebufferPtr> frame_buffers =
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

				drawable_size = setup::window_drawable_size(window);
				logger->debug(
					"New drawable size ({}, {})", drawable_size.width, drawable_size.height);

				clear_colour[0] = 1.0F - clear_colour[0];
				clear_colour[2] = 1.0F - clear_colour[2];
				logger->debug("Changing clear colour to ({})", fmt::join(clear_colour, ","));

				// Recreate swapchain and dependent resources
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
			}
		}

		auto const image_idx =
			draw::acquire_next_swapchain_image(device, swapchain, image_available_semaphore);

		if (!image_idx.has_value())
		{
			logger->debug("Swapchain out of date");
			// Race condition - window resized before event processed.
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