// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// Conflicts with clang-tidy wrt vulkan handle typedefs:
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppLocalVariableMayBeConst

#include "draw.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)

#include <doctest/doctest.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include <strong_type/type.hpp>

#include "Logger.hpp"
#include "macros.hpp"
#include "setup.hpp"
#include "types.hpp"

using namespace std::literals;

namespace vulkandemo::draw
{

bool submit_present_image_cmd(
	VkQueue queue,
	types::VulkanSwapchainPtr const & swapchain,
	types::VulkanImageIdx const image_idx,
	types::VulkanSemaphorePtr const & wait_semaphore)
{
	VkSwapchainKHR swapchain_handle = swapchain.get();
	VkSemaphore wait_semaphore_handle = wait_semaphore.get();

	VkPresentInfoKHR const present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphore_handle != nullptr),
		.pWaitSemaphores = &wait_semaphore_handle,
		.swapchainCount = 1,
		.pSwapchains = &swapchain_handle,
		.pImageIndices = &image_idx.value_of(),
		.pResults = nullptr};

	// Attempt to add commands to present image, returning false if out of date or suboptimal.
	VkResult const result = vkQueuePresentKHR(queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		return false;
	if (result != VK_SUCCESS)
		throw std::runtime_error{
			std::format("Failed to present image: {}", string_VkResult(result))};
	return true;
}

void submit_command_buffer(
	VkQueue queue,
	VkCommandBuffer command_buffer,
	types::VulkanSemaphorePtr const & wait_semaphore,
	types::VulkanSemaphorePtr const & signal_semaphore)
{
	// Pipeline stage(s) to associate with wait_semaphore. Ensure dependent operations do not
	// start at this stage of the pipeline until the semaphore is signaled.
	constexpr VkPipelineStageFlags wait_dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphore wait_semaphore_handle = wait_semaphore.get();
	VkSemaphore signal_semaphore_handle = signal_semaphore.get();

	// Submit command buffers to queue.
	VkSubmitInfo const submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphore_handle != nullptr),
		.pWaitSemaphores = &wait_semaphore_handle,
		.pWaitDstStageMask = &wait_dst_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffer,
		.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphore_handle != nullptr),
		.pSignalSemaphores = &signal_semaphore_handle};

	VK_CHECK(
		vkQueueSubmit(queue, 1, &submit_info, nullptr), "Failed to submit command buffer to queue");
}

void populate_cmd_render_pass(
	VkCommandBuffer command_buffer,
	types::VulkanRenderPassPtr const & render_pass,
	types::VulkanFramebufferPtr const & frame_buffer,
	VkExtent2D const extent,
	types::VulkanClearColour const & clear_colour)
{
	VkClearValue clear_value{};
	std::ranges::copy(clear_colour.value_of(), begin(std::span(clear_value.color.float32)));

	constexpr VkCommandBufferBeginInfo command_buffer_begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr};
	VK_CHECK(
		vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info),
		"Failed to begin command buffer");

	VkRenderPassBeginInfo const render_pass_begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = render_pass.get(),
		.framebuffer = frame_buffer.get(),
		.renderArea = {.offset = {.x = 0, .y = 0}, .extent = extent},
		.clearValueCount = 1,
		.pClearValues = &clear_value};
	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport const viewport{
		.x = 0,
		.y = 0,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height)};
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	VkRect2D const scissor{.offset = {0, 0}, .extent = extent};
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	// End render pass e.g. transition colour attachment to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// ready for presentation.
	vkCmdEndRenderPass(command_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
}

std::optional<types::VulkanImageIdx> acquire_next_swapchain_image(
	types::VulkanDevicePtr const & device,
	types::VulkanSwapchainPtr const & swapchain,
	types::VulkanSemaphorePtr const & semaphore)
{
	types::VulkanImageIdx out{strong::uninitialized};
	VkResult const result = vkAcquireNextImageKHR(
		device.get(),
		swapchain.get(),
		std::numeric_limits<uint64_t>::max(),
		semaphore.get(),
		nullptr,
		&out.value_of());

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		return std::nullopt;

	if (result != VK_SUCCESS)
		throw std::runtime_error{
			std::format("Failed to acquire next swapchain image: {}", string_VkResult(result))};

	return out;
}

TEST_CASE("Acquire swapchain image")
{
	static int test_num = 0;
	++test_num;
	vulkandemo::LoggerPtr const logger =
		vulkandemo::create_logger(std::format("Acquire swapchain image {}", test_num));

	types::SDLWindowPtr const window = setup::create_window("", 10, 10);
	types::VulkanInstancePtr const instance = setup::create_vulkan_instance(
		logger,
		window,
		types::VectorOfAvailableInstanceLayerNameCstrs{"VK_LAYER_KHRONOS_validation"},
		types::VectorOfAvailableInstanceExtensionNameCstrs{VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
	types::VulkanDebugMessengerPtr const messenger =
		setup::create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = setup::create_surface(window, instance);
	auto [physical_device, queue_family_idx] = setup::select_physical_device(
		logger,
		setup::enumerate_physical_devices(logger, instance),
		types::SetOfDesiredDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		{},
		surface.get());

	auto [device, queues] = setup::create_device_and_queues(
		physical_device,
		{{queue_family_idx, types::VulkanQueueCount{1}}},
		types::VectorOfAvailableDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}});

	std::vector<VkSurfaceFormatKHR> const available_formats =
		setup::filter_available_surface_formats(
			logger, physical_device, surface, {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM});

	auto [swapchain, image_views] = setup::create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto const image_available_semaphore = setup::create_semaphore(device);

	SUBCASE("Acquire successful")
	{
		auto const image_idx =
			acquire_next_swapchain_image(device, swapchain, image_available_semaphore);

		REQUIRE(image_idx);
		CHECK(*image_idx == 0U);  // NOLINT(bugprone-unchecked-optional-access)
	}

	// TODO(DF): Figure out how to simulate this.
	// SUBCASE("Acquire out of date")
	// {
	// 	// Resize window.
	// 	SDL_SetWindowSize(window.get(), 1, 1);
	// 	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	//
	// 	auto const image_idx =
	// 		acquire_next_swapchain_image(device, swapchain, image_available_semaphore);
	//
	// 	CHECK(!image_idx);
	// }
}

TEST_CASE("Populate render pass")
{
	static int test_num = 0;
	++test_num;
	vulkandemo::LoggerPtr const logger =
		vulkandemo::create_logger(std::format("Populate render pass {}", test_num));

	types::SDLWindowPtr const window = setup::create_window("", 1, 1);
	types::VulkanInstancePtr const instance = setup::create_vulkan_instance(
		logger,
		window,
		types::VectorOfAvailableInstanceLayerNameCstrs{"VK_LAYER_KHRONOS_validation"},
		types::VectorOfAvailableInstanceExtensionNameCstrs{VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
	types::VulkanDebugMessengerPtr const messenger =
		setup::create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = setup::create_surface(window, instance);
	auto [physical_device, queue_family_idx] = setup::select_physical_device(
		logger,
		setup::enumerate_physical_devices(logger, instance),
		types::SetOfDesiredDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		{},
		surface.get());

	auto [device, queues] = setup::create_device_and_queues(
		physical_device,
		{{queue_family_idx, types::VulkanQueueCount{1}}},
		types::VectorOfAvailableDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}});

	auto const image_available_semaphore = setup::create_semaphore(device);
	auto const rendering_finished_semaphore = setup::create_semaphore(device);

	std::vector<VkSurfaceFormatKHR> const available_formats =
		setup::filter_available_surface_formats(
			logger, physical_device, surface, {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM});

	auto [swapchain, image_views] = setup::create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto const render_pass = setup::create_single_presentation_subpass_render_pass(
		available_formats.at(0).format, device);

	VkExtent2D const drawable_size = setup::window_drawable_size(window);

	std::vector<types::VulkanFramebufferPtr> const frame_buffers =
		setup::create_per_image_frame_buffers(device, render_pass, image_views, drawable_size);

	types::VulkanCommandPoolPtr const command_pool =
		setup::create_command_pool(device, queue_family_idx);

	types::VulkanCommandBuffersPtr command_buffers = setup::create_primary_command_buffers(
		device, command_pool, types::VulkanCommandBufferCount{frame_buffers.size()});

	// Re-create command buffers and ensure we can populate with new command buffer - regression
	// test against VulkanCommandBuffers destructor logic.
	command_buffers = setup::create_primary_command_buffers(
		device, command_pool, types::VulkanCommandBufferCount{frame_buffers.size()});

	VkCommandBuffer command_buffer = command_buffers->front();

	types::VulkanFramebufferPtr const & frame_buffer = frame_buffers.front();

	populate_cmd_render_pass(
		command_buffer,
		render_pass,
		frame_buffer,
		drawable_size,
		types::VulkanClearColour{std::array{1.0F, .0F, .0F, 1.0F}});
}

TEST_CASE("Populate command queue and present")	 // NOLINT(*-function-cognitive-complexity)
{
	static int test_num = 0;
	++test_num;
	vulkandemo::LoggerPtr const logger =
		vulkandemo::create_logger(std::format("Populate command queue and present {}", test_num));

	types::SDLWindowPtr const window = setup::create_window("", 100, 100);
	types::VulkanInstancePtr const instance = setup::create_vulkan_instance(
		logger,
		window,
		types::VectorOfAvailableInstanceLayerNameCstrs{"VK_LAYER_KHRONOS_validation"},
		types::VectorOfAvailableInstanceExtensionNameCstrs{VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
	types::VulkanDebugMessengerPtr const messenger =
		setup::create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = setup::create_surface(window, instance);
	auto [physical_device, queue_family_idx] = setup::select_physical_device(
		logger,
		setup::enumerate_physical_devices(logger, instance),
		types::SetOfDesiredDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		{},
		surface.get());

	auto [device, queues] = setup::create_device_and_queues(
		physical_device,
		{{queue_family_idx, types::VulkanQueueCount{1}}},
		types::VectorOfAvailableDeviceExtensionNameViews{
			std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}});

	auto const image_available_semaphore = setup::create_semaphore(device);
	auto const rendering_finished_semaphore = setup::create_semaphore(device);

	std::vector<VkSurfaceFormatKHR> const available_formats =
		setup::filter_available_surface_formats(
			logger, physical_device, surface, {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM});

	auto [swapchain, image_views] = setup::create_exclusive_double_buffer_swapchain_and_image_views(
		logger, physical_device, device, surface, available_formats.at(0));

	auto render_pass = setup::create_single_presentation_subpass_render_pass(
		available_formats.at(0).format, device);

	VkExtent2D const drawable_size = setup::window_drawable_size(window);

	std::vector<types::VulkanFramebufferPtr> const frame_buffers =
		setup::create_per_image_frame_buffers(device, render_pass, image_views, drawable_size);

	types::VulkanCommandPoolPtr const command_pool =
		setup::create_command_pool(device, queue_family_idx);

	types::VulkanCommandBuffersPtr const command_buffers = setup::create_primary_command_buffers(
		device, command_pool, types::VulkanCommandBufferCount{frame_buffers.size()});

	VkQueue queue = queues[queue_family_idx].front();

	// Begin "render loop".

	SUBCASE("render once")
	{
		auto const maybe_image_idx =
			acquire_next_swapchain_image(device, swapchain, image_available_semaphore);

		CHECK(maybe_image_idx);

		auto const image_idx =
			maybe_image_idx.value();  // NOLINT(bugprone-unchecked-optional-access)
		VkCommandBuffer command_buffer = command_buffers->at(image_idx);
		types::VulkanFramebufferPtr const & frame_buffer = frame_buffers.at(image_idx);

		populate_cmd_render_pass(
			command_buffer,
			render_pass,
			frame_buffer,
			drawable_size,
			types::VulkanClearColour{std::array{1.0F, .0F, .0F, 1.0F}});

		submit_command_buffer(
			queue, command_buffer, image_available_semaphore, rendering_finished_semaphore);

		submit_present_image_cmd(queue, swapchain, image_idx, rendering_finished_semaphore);

		VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue to be idle");
	}

	SUBCASE("render twice")
	{
		auto const maybe_image_idx =
			acquire_next_swapchain_image(device, swapchain, image_available_semaphore);
		REQUIRE(maybe_image_idx);
		auto const image_idx =
			maybe_image_idx.value();  // NOLINT(bugprone-unchecked-optional-access)

		{
			VkCommandBuffer command_buffer = command_buffers->at(image_idx);
			types::VulkanFramebufferPtr const & frame_buffer = frame_buffers.at(image_idx);

			// Red.
			populate_cmd_render_pass(
				command_buffer,
				render_pass,
				frame_buffer,
				drawable_size,
				types::VulkanClearColour{std::array{1.0F, .0F, .0F, 1.0F}});

			submit_command_buffer(
				queue, command_buffer, image_available_semaphore, rendering_finished_semaphore);

			submit_present_image_cmd(queue, swapchain, image_idx, rendering_finished_semaphore);
		}

		VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue to be idle");

		auto const maybe_image_idx_2 =
			acquire_next_swapchain_image(device, swapchain, image_available_semaphore);
		REQUIRE(maybe_image_idx_2);

		{
			auto const image_idx_2 =
				maybe_image_idx_2.value();	// NOLINT(bugprone-unchecked-optional-access)
			CHECK(image_idx_2 != image_idx);

			VkCommandBuffer command_buffer = command_buffers->at(image_idx_2);
			types::VulkanFramebufferPtr const & frame_buffer = frame_buffers.at(image_idx_2);

			// Green
			populate_cmd_render_pass(
				command_buffer,
				render_pass,
				frame_buffer,
				drawable_size,
				types::VulkanClearColour{std::array{.0F, .0F, 1.0F, 1.0F}});

			submit_command_buffer(
				queue, command_buffer, image_available_semaphore, rendering_finished_semaphore);

			submit_present_image_cmd(queue, swapchain, image_idx_2, rendering_finished_semaphore);
		}

		VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue to be idle");
	}
}
}  // namespace vulkandemo::draw
