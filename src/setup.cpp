// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// Conflicts with clang-tidy wrt vulkan handle typedefs:
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppLocalVariableMayBeConst

#include "setup.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
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

#include <fmt/core.h>
#include <fmt/format.h>

#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)

#include <gsl/pointers>

#include <doctest/doctest.h>

#include <SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "Logger.hpp"
#include "hof.hpp"
#include "macros.hpp"
#include "types.hpp"

using namespace std::literals;

namespace vulkandemo::setup
{
// Forward declarations of utility functions.

namespace
{

/**
 * Create double buffering presentation swapchain, for exclusive use by a single queue.
 *
 * @param logger
 * @param physical_device
 * @param device
 * @param surface
 * @param surface_format
 * @param previous_swapchain
 * @return
 */
types::VulkanSwapchainPtr create_exclusive_double_buffer_swapchain(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanDevicePtr const & device,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR surface_format,
	types::VulkanSwapchainPtr const & previous_swapchain);

/**
 * Create swapchain image view of a single mip level and single array layer colour aspect.
 *
 * @param device
 * @param surface_format
 * @param swapchain
 * @return
 */
std::vector<types::VulkanImageViewPtr>
create_colour_aspect_single_mip_single_layer_swapchain_image_views(
	types::VulkanDevicePtr const & device,
	VkSurfaceFormatKHR surface_format,
	types::VulkanSwapchainPtr const & swapchain);

/**
 * Log desired instance extensions vs. available.
 *
 * @param logger
 * @param desired_extension_names
 * @param available_extension_names
 * @param available_extension_properties
 */
void log_instance_extensions_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	std::set<types::AvailableInstanceExtensionNameView> const & available_extension_names,
	std::span<VkExtensionProperties const> available_extension_properties);

/**
 * Callback to be called by the VK_EXT_debug_utils extension with log messages.
 *
 * @param message_severity
 * @param message_types
 * @param callback_data
 * @param user_data
 * @return
 */
VkBool32 vulkan_debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	VkDebugUtilsMessengerCallbackDataEXT const * callback_data,
	void * user_data);

/**
 * Log layer availability vs desired.
 *
 * @param logger
 * @param desired_layer_names
 * @param available_layer_names
 * @param available_layer_descs
 */
void log_layer_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::set<types::AvailableInstanceLayerNameView> const & available_layer_names,
	std::span<VkLayerProperties const> available_layer_descs);

}  // namespace

// Main functionality.

types::VulkanSemaphorePtr create_semaphore(types::VulkanDevicePtr const & device)
{
	constexpr VkSemaphoreCreateInfo semaphore_create_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr};

	VkSemaphore out = nullptr;
	VK_CHECK(
		vkCreateSemaphore(device.get(), &semaphore_create_info, nullptr, &out),
		"Failed to create semaphore");
	return types::make_semaphore_ptr(device, out);
}

types::VulkanCommandBuffersPtr create_primary_command_buffers(
	types::VulkanDevicePtr device,
	types::VulkanCommandPoolPtr pool,
	types::VulkanCommandBufferCount count)
{
	VkCommandBufferAllocateInfo const command_buffer_allocate_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool.get(),
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = count};

	std::vector<VkCommandBuffer> buffers(count);
	VK_CHECK(
		vkAllocateCommandBuffers(device.get(), &command_buffer_allocate_info, buffers.data()),
		"Failed to allocate command buffers");

	return types::make_command_buffers_ptr(std::move(device), std::move(pool), std::move(buffers));
}

types::VulkanCommandPoolPtr create_command_pool(
	types::VulkanDevicePtr device, types::VulkanQueueFamilyIdx const queue_family_idx)
{
	VkCommandPoolCreateInfo const command_pool_create_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue_family_idx};

	VkCommandPool command_pool = nullptr;
	VK_CHECK(
		vkCreateCommandPool(device.get(), &command_pool_create_info, nullptr, &command_pool),
		"Failed to create command pool");

	return types::make_command_pool_ptr(std::move(device), command_pool);
}

std::vector<types::VulkanFramebufferPtr> create_per_image_frame_buffers(
	types::VulkanDevicePtr const & device,
	types::VulkanRenderPassPtr const & render_pass,
	std::span<types::VulkanImageViewPtr const> const image_views,
	VkExtent2D const size)
{
	VkFramebufferCreateInfo frame_buffer_create_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = render_pass.get(),
		.attachmentCount = 1,
		// Width and height must be equal to or greater than the smallest image view.
		.width = size.width,
		.height = size.height,
		.layers = 1	 // Non-stereoscopic
	};

	std::vector<types::VulkanFramebufferPtr> frame_buffers;
	frame_buffers.reserve(image_views.size());
	std::ranges::transform(
		image_views,
		back_inserter(frame_buffers),
		[&](types::VulkanImageViewPtr const & image_view)
		{
			VkImageView image_view_handle = image_view.get();
			frame_buffer_create_info.pAttachments = &image_view_handle;
			VkFramebuffer out = nullptr;
			VK_CHECK(
				vkCreateFramebuffer(device.get(), &frame_buffer_create_info, nullptr, &out),
				"Failed to create framebuffer");
			frame_buffer_create_info.pAttachments = nullptr;  // reset.
			return types::make_framebuffer_ptr(device, out);
		});

	return frame_buffers;
}

types::VulkanRenderPassPtr create_single_presentation_subpass_render_pass(
	VkFormat surface_format, types::VulkanDevicePtr const & device)
{
	// Create color attachment.
	VkAttachmentDescription const color_attachment{
		.format = surface_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,	  // See VkRenderPassBeginInfo::pClearValues
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,  // Store after the pass so we can present it.
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

	constexpr VkAttachmentReference color_attachment_ref{
		.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// False positive:
	// ReSharper disable once CppVariableCanBeMadeConstexpr
	VkSubpassDescription const subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref};

	// Dependency to ensure all drawing operations on the attachment from previous render passes
	// have finished before this subpass begins.
	constexpr VkSubpassDependency subpass_external_dependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0};

	VkRenderPassCreateInfo const render_pass_create_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &subpass_external_dependency};

	// Create the render pass.
	VkRenderPass out = nullptr;
	VK_CHECK(
		vkCreateRenderPass(device.get(), &render_pass_create_info, nullptr, &out),
		"Failed to create render pass");
	return types::make_render_pass_ptr(device, out);
}

std::tuple<types::VulkanSwapchainPtr, std::vector<types::VulkanImageViewPtr>>
create_exclusive_double_buffer_swapchain_and_image_views(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanDevicePtr const & device,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR const surface_format,
	types::VulkanSwapchainPtr const & previous_swapchain)
{
	if (logger->should_log(spdlog::level::debug))
	{
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(physical_device, &device_properties);
		// Log name of device.
		logger->debug("Creating swapchain for device {}", device_properties.deviceName);
	}

	types::VulkanSwapchainPtr swapchain = create_exclusive_double_buffer_swapchain(
		logger, physical_device, device, surface, surface_format, previous_swapchain);

	// Query raw images associated with swapchain.

	// Construct image views.
	std::vector<types::VulkanImageViewPtr> image_views =
		create_colour_aspect_single_mip_single_layer_swapchain_image_views(
			device, surface_format, swapchain);

	return {std::move(swapchain), std::move(image_views)};
}

namespace
{
std::vector<types::VulkanImageViewPtr>
create_colour_aspect_single_mip_single_layer_swapchain_image_views(
	types::VulkanDevicePtr const & device,
	VkSurfaceFormatKHR const surface_format,
	types::VulkanSwapchainPtr const & swapchain)
{
	// Query raw images associated with swapchain.
	std::vector<VkImage> const swapchain_images = [&]
	{
		uint32_t count = 0;
		VK_CHECK(
			vkGetSwapchainImagesKHR(device.get(), swapchain.get(), &count, nullptr),
			"Failed to get swapchain image count");

		std::vector<VkImage> out(count);
		VK_CHECK(
			vkGetSwapchainImagesKHR(device.get(), swapchain.get(), &count, out.data()),
			"Failed to get swapchain images");
		return out;
	}();

	// Construct image views.
	VkImageViewCreateInfo image_view_create_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = nullptr,  // Will be updated per-image, see below.
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = surface_format.format,
		.components =
			{VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY},
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

	std::vector<types::VulkanImageViewPtr> out;

	// Create an image view for each swapchain image.
	std::ranges::transform(
		swapchain_images,
		std::back_inserter(out),
		[&](VkImage image)
		{
			image_view_create_info.image = image;
			VkImageView image_view = nullptr;
			VK_CHECK(
				vkCreateImageView(device.get(), &image_view_create_info, nullptr, &image_view),
				"Failed to create image view");
			return types::make_image_view_ptr(device, image_view);
		});

	return out;
}

types::VulkanSwapchainPtr create_exclusive_double_buffer_swapchain(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanDevicePtr const & device,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR const surface_format,
	types::VulkanSwapchainPtr const & previous_swapchain)
{
	// Get surface capabilities.
	VkSurfaceCapabilitiesKHR surface_capabilities{};
	VK_CHECK(
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			physical_device, surface.get(), &surface_capabilities),
		"Failed to get surface capabilities");

	// Get present modes.
	std::vector<VkPresentModeKHR> const present_modes = [&]
	{
		uint32_t count = 0;
		VK_CHECK(
			vkGetPhysicalDeviceSurfacePresentModesKHR(
				physical_device, surface.get(), &count, nullptr),
			"Failed to get present mode count");

		std::vector<VkPresentModeKHR> out(count);
		VK_CHECK(
			vkGetPhysicalDeviceSurfacePresentModesKHR(
				physical_device, surface.get(), &count, out.data()),
			"Failed to get present modes");
		return out;
	}();

	// Log present modes at debug level.
	logger->debug(
		"\tAvailable present modes: {}",
		fmt::join(std::views::transform(present_modes, &string_VkPresentModeKHR), ", "));

	// Choose best present mode.
	VkPresentModeKHR const present_mode = [&]
	{
		if (std::ranges::contains(present_modes, VK_PRESENT_MODE_MAILBOX_KHR))
			return VK_PRESENT_MODE_MAILBOX_KHR;
		return VK_PRESENT_MODE_FIFO_KHR;
	}();

	logger->debug("\tChoosing present mode {}", string_VkPresentModeKHR(present_mode));

	// Choose double-buffer of images, or as close as we can get.
	uint32_t const swapchain_image_count = std::min(
		std::max(2U, surface_capabilities.minImageCount), surface_capabilities.maxImageCount);

	logger->debug("\tChoosing swapchain image count {}", swapchain_image_count);

	VkSurfaceTransformFlagBitsKHR const surface_transform =
		(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0U
		? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
		: surface_capabilities.currentTransform;

	logger->debug(
		"\tSwitching transform from {} to {}",
		string_VkSurfaceTransformFlagsKHR(surface_capabilities.currentTransform),
		string_VkSurfaceTransformFlagsKHR(surface_transform));

	if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
		throw std::runtime_error{"Surface size is undefined"};

	// Choose opaque composite alpha mode, or throw.
	constexpr VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if ((surface_capabilities.supportedCompositeAlpha & composite_alpha) == 0U)
		throw std::runtime_error{"VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR unavailable"};

	// Swapchain images should support colour attachment.
	// NOLINTNEXTLINE(*-signed-bitwise)
	constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ((surface_capabilities.supportedUsageFlags & usage) != usage)
		throw std::runtime_error{"Surface VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT unavailable"};

	// Create swapchain.
	VkSwapchainCreateInfoKHR const swapchain_create_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface.get(),
		.minImageCount = swapchain_image_count,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = surface_capabilities.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = usage,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = surface_transform,
		.compositeAlpha = composite_alpha,
		.presentMode = present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = previous_swapchain.get()};

	VkSwapchainKHR out = nullptr;
	VK_CHECK(
		vkCreateSwapchainKHR(device.get(), &swapchain_create_info, nullptr, &out),
		"Failed to create swapchain");
	return types::make_swapchain_ptr(device, out);
}

}  // namespace

std::tuple<types::VulkanDevicePtr, types::MapOfVulkanQueueFamilyIdxToVectorOfQueues>
create_device_and_queues(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> const device_extension_names)
{
	std::vector<char const *> const device_extension_cstr_names = [&]
	{
		std::vector<char const *> out;
		std::ranges::copy(
			device_extension_names |
				std::views::transform([](auto const & extension_name)
									  { return extension_name.value_of().data(); }),
			std::back_inserter(out));
		return out;
	}();

	// Queue priority of 1.0. Use same array for all VkDeviceQueueCreateInfo. Hence,
	// create a single array sized to the largest queue count. Array must exist until after
	// vkCreateDevice.
	std::vector const queue_priorities(
		std::ranges::max(queue_family_and_counts | std::views::transform(hof::attr::second())),
		1.0F);

	std::vector<VkDeviceQueueCreateInfo> const queue_create_infos =
		queue_family_and_counts |
		std::views::transform(
			[&](auto const & queue_family_and_count)
			{
				auto const [queue_family_idx, queue_count] = queue_family_and_count;

				VkDeviceQueueCreateInfo queue_create_info{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queue_family_idx,
					.queueCount = queue_count,
					.pQueuePriorities = queue_priorities.data()};

				return queue_create_info;
			}) |
		ranges::to<std::vector>();

	// Create the device.
	VkDevice device = [&]
	{
		VkDeviceCreateInfo const device_create_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
			.pQueueCreateInfos = queue_create_infos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(device_extension_cstr_names.size()),
			.ppEnabledExtensionNames = device_extension_cstr_names.data(),
		};
		VkDevice out = nullptr;
		VK_CHECK(
			vkCreateDevice(physical_device, &device_create_info, nullptr, &out),
			"Failed to create logical device");
		return out;
	}();

	// Construct a map of queue family to vector of queues.
	types::MapOfVulkanQueueFamilyIdxToVectorOfQueues queues;
	for (auto const & [queue_family_idx, queue_count] : queue_family_and_counts)
	{
		auto & queues_for_family = queues[queue_family_idx];
		queues_for_family.reserve(queue_count);
		for (types::VulkanQueueCount queue_idx{0}; queue_idx < queue_count; ++queue_idx)
		{
			VkQueue queue = nullptr;
			vkGetDeviceQueue(device, queue_family_idx, queue_idx, &queue);
			queues_for_family.push_back(queue);
		}
	}

	return {types::make_device_ptr(device), std::move(queues)};
}

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

	std::vector<VkSurfaceFormatKHR> filtered_surface_formats;
	for (auto const & available_surface_format : available_surface_formats)
		if (std::ranges::contains(desired_formats, available_surface_format.format))
			filtered_surface_formats.push_back(available_surface_format);

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
	for (VkPhysicalDevice physical_device : physical_devices)
	{
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

		return {physical_device, filtered_queue_families.front()};
	}

	throw std::runtime_error("Failed to find device with desired capabilities");
}

std::vector<types::AvailableDeviceExtensionNameView> filter_available_device_extensions(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	std::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names)
{
	if (desired_device_extension_names.empty())
		return {};

	// Get available device extension names.
	std::vector<VkExtensionProperties> const available_device_extensions = [&]
	{
		uint32_t extension_count = 0;
		VK_CHECK(
			vkEnumerateDeviceExtensionProperties(
				physical_device, nullptr, &extension_count, nullptr),
			"Failed to get device extension count");
		std::vector<VkExtensionProperties> out(extension_count);
		VK_CHECK(
			vkEnumerateDeviceExtensionProperties(
				physical_device, nullptr, &extension_count, out.data()),
			"Failed to get device extensions");

		return out;
	}();

	std::set<types::AvailableDeviceExtensionNameView> const available_device_extension_names =
		available_device_extensions |
		std::views::transform(
			[](VkExtensionProperties const & extension)
			{ return types::AvailableDeviceExtensionNameView{extension.extensionName}; }) |
		ranges::to<std::set>();

	std::vector<types::AvailableDeviceExtensionNameView> const extensions_to_enable =
		ranges::views::set_intersection(
			available_device_extension_names | ranges::views::transform(hof::mem_fn::value_of()),
			desired_device_extension_names | ranges::views::transform(hof::mem_fn::value_of())) |
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

std::vector<VkPhysicalDevice> enumerate_physical_devices(
	LoggerPtr const & logger, types::VulkanInstancePtr const & instance)
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

types::VulkanSurfacePtr create_surface(
	types::SDLWindowPtr const & window, types::VulkanInstancePtr instance)
{
	VkSurfaceKHR surface = nullptr;
	if (SDL_Vulkan_CreateSurface(window.get(), instance.get(), &surface) != SDL_TRUE)
		throw std::runtime_error{
			std::format("Failed to create Vulkan surface: {}", SDL_GetError())};

	return types::make_surface_ptr(std::move(instance), surface);
}

types::VulkanDebugMessengerPtr create_debug_messenger(
	LoggerPtr logger, types::VulkanInstancePtr instance)
{
	// ReSharper disable once CppDFAMemoryLeak
	// ReSharper disable once CppUseAuto
	gsl::owner<LoggerPtr *> const plogger = new LoggerPtr(std::move(logger));  // NOLINT(*-use-auto)

	VkDebugUtilsMessengerCreateInfoEXT const messenger_create_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		// NOLINTBEGIN(*-signed-bitwise)
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		// NOLINTEND(*-signed-bitwise)
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = &vulkan_debug_messenger_callback,
		.pUserData = plogger};

	auto const pvkCreateDebugUtilsMessengerEXT =  // NOLINT(*-identifier-naming)
												  // NOLINTNEXTLINE(*-reinterpret-cast)
		reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance.get(), "vkCreateDebugUtilsMessengerEXT"));

	if (pvkCreateDebugUtilsMessengerEXT == nullptr)
		throw std::runtime_error{"Failed to load vkCreateDebugUtilsMessengerEXT"};

	VkDebugUtilsMessengerEXT messenger = nullptr;
	VK_CHECK(
		pvkCreateDebugUtilsMessengerEXT(
			instance.get(), &messenger_create_info, nullptr, &messenger),
		"Failed to create Vulkan debug messenger");

	return types::make_debug_messenger_ptr(std::move(instance), plogger, messenger);
}

namespace
{
VkBool32 vulkan_debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT const message_severity,
	VkDebugUtilsMessageTypeFlagsEXT const message_types,
	VkDebugUtilsMessengerCallbackDataEXT const * callback_data,
	void * user_data)
{
	LoggerPtr const & log = *static_cast<LoggerPtr *>(user_data);

	// Short-circuit if logger is not interested.

	if (!log->should_log(spdlog::level::info))
		return VkBool32{0};

	if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT &&
		!log->should_log(spdlog::level::warn))
		return VkBool32{0};

	if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT &&
		!log->should_log(spdlog::level::err))
		return VkBool32{0};

	// Construct message from provided data.
	std::string const msg = std::format(
		"Vulkan [{}] [{}] Queues[{}] CmdBufs[{}] Objects[{}]: {}",
		[message_types]
		{
			std::vector<std::string> type_strings;
			if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
				type_strings.emplace_back("GENERAL");
			if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
				type_strings.emplace_back("VALIDATION");
			if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				type_strings.emplace_back("PERFORMANCE");
			if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
				type_strings.emplace_back("DEVICE_ADDRESS");
			return fmt::format("{}", fmt::join(type_strings, "|"));
		}(),
		callback_data->pMessageIdName,
		[&]
		{
			auto labels = std::span{callback_data->pQueueLabels, callback_data->queueLabelCount} |
				std::views::transform(&VkDebugUtilsLabelEXT::pLabelName);
			return format("{}", fmt::join(labels, "|"));
		}(),
		[&]
		{
			auto labels = std::span{callback_data->pCmdBufLabels, callback_data->cmdBufLabelCount} |
				std::views::transform(&VkDebugUtilsLabelEXT::pLabelName);
			return format("{}", fmt::join(labels, "|"));
		}(),
		[&]
		{
			auto names = std::span{callback_data->pObjects, callback_data->objectCount} |
				std::views::transform(
							 [](auto const & object_info)
							 {
								 if (object_info.pObjectName)
									 return object_info.pObjectName;
								 return string_VkObjectType(object_info.objectType);
							 });
			return format("{}", fmt::join(names, "|"));
		}(),
		callback_data->pMessage);

	// Log the message at appropriate severity level.
	if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		log->error(msg);
	else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		log->warn(msg);
	else
		log->info(msg);

	return VkBool32{1};
}
}  // namespace

types::VulkanInstancePtr create_vulkan_instance(
	LoggerPtr const & logger,
	types::SDLWindowPtr const & sdl_window,
	std::span<types::AvailableInstanceLayerNameCstr const> layers_to_enable,
	std::span<types::AvailableInstanceExtensionNameCstr const> extensions_to_enable)
{
	// Get the available extensions from SDL
	std::vector<char const *> extensions_to_enable_cstr = [&]
	{
		std::vector<char const *> out;
		uint32_t extension_count = 0;
		SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, nullptr);
		out.resize(extension_count);
		SDL_Vulkan_GetInstanceExtensions(sdl_window.get(), &extension_count, out.data());
		return out;
	}();

	// Merge additional extensions with SDL-provided extensions.
	std::ranges::copy(
		extensions_to_enable | std::views::transform(hof::mem_fn::value_of()),
		back_inserter(extensions_to_enable_cstr));

	logger->debug("Enabling instance extensions: {}", fmt::join(extensions_to_enable_cstr, ", "));

	// Application metadata.
	VkApplicationInfo const app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = SDL_GetWindowTitle(sdl_window.get()),
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = SDL_GetWindowTitle(sdl_window.get()),
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3,
	};

	// Instance creation info.

	std::vector<char const *> layers_to_enable_cstr;
	std::ranges::copy(
		layers_to_enable | std::views::transform(hof::mem_fn::value_of()),
		back_inserter(layers_to_enable_cstr));

	logger->debug("Enabling layers: {}", fmt::join(layers_to_enable_cstr, ", "));

	VkInstanceCreateInfo const create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = static_cast<uint32_t>(layers_to_enable_cstr.size()),
		.ppEnabledLayerNames = layers_to_enable_cstr.data(),
		.enabledExtensionCount = static_cast<uint32_t>(extensions_to_enable_cstr.size()),
		.ppEnabledExtensionNames = extensions_to_enable_cstr.data(),
	};

	VkInstance out = nullptr;
	VK_CHECK(vkCreateInstance(&create_info, nullptr, &out), "Failed to create Vulkan instance");

	return types::make_instance_ptr(out);
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
			   desired_layer_names | ranges::views::transform(hof::mem_fn::value_of()),
			   available_layer_names | ranges::views::transform(hof::mem_fn::value_of())) |
		ranges::views::transform(std::mem_fn(&std::string_view::data)) |
		ranges::to<std::vector<types::AvailableInstanceLayerNameCstr>>;
}

namespace
{
void log_layer_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::set<types::AvailableInstanceLayerNameView> const & available_layer_names,
	std::span<VkLayerProperties const> const available_layer_descs)
{
	if (!logger->should_log(spdlog::level::debug))
		return;

	// Log requested layers.
	if (!desired_layer_names.empty())
	{
		logger->debug("Requested layers:");
		for (auto const & layer_name : desired_layer_names)
		{
			if (available_layer_names.contains(
					static_cast<types::AvailableInstanceLayerNameView>(layer_name)))
				logger->debug("\t{} (available)", layer_name);
			else
				logger->debug("\t{} (unavailable)", layer_name);
		}
	}

	// Log available layers.
	if (!available_layer_descs.empty() && logger->should_log(spdlog::level::trace))
	{
		logger->trace("Available layers:");
		for (auto const & [layerName, specVersion, implementationVersion, description] :
			 available_layer_descs)
		{
			logger->trace(
				"\t{} (spec version: {}.{}.{}, implementation version: {})",
				layerName,
				VK_VERSION_MAJOR(specVersion),
				VK_VERSION_MINOR(specVersion),
				VK_VERSION_PATCH(specVersion),
				implementationVersion);
			logger->trace("\t\t{}", description);
		}
	}
}
}  // namespace

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
	std::set<types::AvailableInstanceExtensionNameView> const available_extension_names{
		[&]
		{
			std::set<types::AvailableInstanceExtensionNameView> out;
			std::ranges::transform(
				available_extensions,
				std::inserter(out, end(out)),
				[](VkExtensionProperties const & extension_desc) {
					return types::AvailableInstanceExtensionNameView{extension_desc.extensionName};
				});
			return out;
		}()};

	// Intersection of available extensions and desired extensions to return.
	std::vector<types::AvailableInstanceExtensionNameCstr> extensions_to_enable{
		[&]
		{
			std::vector<types::AvailableInstanceExtensionNameView> extension_names;
			extension_names.reserve(desired_extension_names.size());
			std::ranges::set_intersection(
				desired_extension_names |
					std::views::transform(hof::cast<types::AvailableInstanceExtensionNameView>()),
				available_extension_names,
				std::back_inserter(extension_names));
			std::vector<types::AvailableInstanceExtensionNameCstr> out;
			out.reserve(extension_names.size());
			std::ranges::transform(
				extension_names,
				std::back_inserter(out),
				[](types::AvailableInstanceExtensionNameView const & name)
				{ return types::AvailableInstanceExtensionNameCstr{name.value_of().data()}; });
			;
			return out;
		}()};

	log_instance_extensions_info(
		logger, desired_extension_names, available_extension_names, available_extensions);

	return extensions_to_enable;
}

namespace
{
void log_instance_extensions_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	std::set<types::AvailableInstanceExtensionNameView> const & available_extension_names,
	std::span<VkExtensionProperties const> const available_extension_properties)
{
	if (!logger->should_log(spdlog::level::debug))
		return;

	// Log requested extensions.
	if (!desired_extension_names.empty())
	{
		logger->debug("Requested extensions:");
		for (auto const & extension_name : desired_extension_names)
		{
			if (available_extension_names.contains(
					static_cast<types::AvailableInstanceExtensionNameView>(extension_name)))
				logger->debug("\t{} (available)", extension_name);
			else
				logger->debug("\t{} (unavailable)", extension_name);
		}
	}

	if (!logger->should_log(spdlog::level::debug))
		return;

	// Log available extensions.
	if (available_extension_properties.empty())
		return;

	logger->trace("Available extensions:");
	for (auto const & [extensionName, specVersion] : available_extension_properties)
	{
		logger->trace(
			"\t{} ({}.{}.{})",
			extensionName,
			VK_VERSION_MAJOR(specVersion),
			VK_VERSION_MINOR(specVersion),
			VK_VERSION_PATCH(specVersion));
	}
}
}  // namespace

VkExtent2D window_drawable_size(types::SDLWindowPtr const & window)
{
	int width = 0;
	int height = 0;
	SDL_Vulkan_GetDrawableSize(window.get(), &width, &height);
	return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

types::SDLWindowPtr create_window(char const * title, int const width, int const height)
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
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	if (window == nullptr)
		throw std::runtime_error{std::format("Failed to create window: {}", SDL_GetError())};

	return types::make_window_ptr(window);
}

TEST_CASE("Create a window")
{
	constexpr int expected_width = 800;
	constexpr int expected_height = 600;
	constexpr auto expected_name = "Hello Vulkan";

	// Create a window.
	types::SDLWindowPtr window = create_window(expected_name, expected_width, expected_height);
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
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create a Vulkan instance");
	types::VulkanInstancePtr instance = create_vulkan_instance(
		logger,
		create_window("", 0, 0),
		filter_available_layers(
			logger,
			{types::DesiredInstanceLayerNameView{"some_unavailable_layer"},
			 types::DesiredInstanceLayerNameView{"VK_LAYER_KHRONOS_validation"}}),
		filter_available_instance_extensions(
			logger,
			{types::DesiredInstanceExtensionNameView{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
			 types::DesiredInstanceExtensionNameView{"some_unavailable_extension"}}));
	CHECK(instance);
}

TEST_CASE("Create a Vulkan debug utils messenger")
{
	vulkandemo::LoggerPtr const logger =
		vulkandemo::create_logger("Create a Vulkan debug utils messenger");

	auto instance_extensions = filter_available_instance_extensions(
		logger, {types::DesiredInstanceExtensionNameView{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}});

	REQUIRE(!instance_extensions.empty());

	types::VulkanInstancePtr instance =
		create_vulkan_instance(logger, create_window("", 0, 0), {}, instance_extensions);
	types::VulkanDebugMessengerPtr messenger = create_debug_messenger(logger, std::move(instance));

	CHECK(messenger);
}

TEST_CASE("Create a Vulkan surface")
{
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create a Vulkan surface");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);

	types::VulkanSurfacePtr surface = create_surface(window, instance);

	CHECK(surface);

	// To trace unknown memory addresses spat out by ASan/LSan
	// std::filesystem::copy_file(
	// 	std::filesystem::path{"/proc/self/maps"},
	// 	std::filesystem::path{"/tmp/maps"},
	// 	std::filesystem::copy_options::update_existing);
}

TEST_CASE("Enumerate devices")
{
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Enumerate devices");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	std::vector<VkPhysicalDevice> const physical_devices =
		enumerate_physical_devices(logger, instance);

	REQUIRE(!physical_devices.empty());

	VkPhysicalDeviceProperties first_device_properties;
	vkGetPhysicalDeviceProperties(physical_devices.front(), &first_device_properties);
	// Should be sorted in order of GPU-first.

	WARN(first_device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);

	std::vector<types::VulkanQueueFamilyIdx> const available_queue_families =
		filter_available_queue_families(physical_devices.front(), VK_QUEUE_GRAPHICS_BIT, surface);

	CHECK(!available_queue_families.empty());

	std::vector<types::VulkanMemoryTypeIdx> const available_memory_types =
		filter_available_memory_types(
			logger, physical_devices.front(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	CHECK(!available_memory_types.empty());

	std::vector<types::AvailableDeviceExtensionNameView> const available_device_extensions =
		filter_available_device_extensions(
			logger,
			physical_devices.front(),
			{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
			 types::DesiredDeviceExtensionNameView{"some_unsupported_extension"}});

	CHECK(available_device_extensions.size() == 1);
}

TEST_CASE("Select device with capability")
{
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Select device with capability");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(
		logger,
		window,
		{{types::AvailableInstanceLayerNameCstr{"VK_LAYER_KHRONOS_validation"}}},
		{{types::AvailableInstanceExtensionNameCstr{VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}});
	types::VulkanDebugMessengerPtr const messenger = create_debug_messenger(logger, instance);

	auto [device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VK_QUEUE_GRAPHICS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	CHECK(device);
	CHECK(queue_family_idx >= types::VulkanQueueFamilyIdx{0});

	// Get device type.
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);
	// Should be sorted in order of GPU-first.
	WARN(device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU);
}

TEST_CASE("Create logical device with queues")
{
	vulkandemo::LoggerPtr const logger =
		vulkandemo::create_logger("Create logical device with queues");
	types::SDLWindowPtr const window = create_window("", 0, 0);
	types::VulkanInstancePtr const instance = create_vulkan_instance(logger, window, {}, {});
	types::VulkanSurfacePtr const surface = create_surface(window, instance);

	auto const [physical_device, queue_family_idx] = select_physical_device(
		logger,
		enumerate_physical_devices(logger, instance),
		{types::DesiredDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}},
		VK_QUEUE_GRAPHICS_BIT,
		0,
		surface);

	constexpr types::VulkanQueueCount expected_queue_count{2};

	auto [device, queues] = create_device_and_queues(
		physical_device,
		{{std::pair{queue_family_idx, expected_queue_count}}},
		{{types::AvailableDeviceExtensionNameView{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}});

	CHECK(device);
	// Check that the device has the expected number of queues.
	CHECK(queues.size() == 1);
	CHECK(queues.at(queue_family_idx).size() == expected_queue_count);
	CHECK(queues.at(queue_family_idx)[0]);
	CHECK(queues.at(queue_family_idx)[1]);
}

TEST_CASE("Create swapchain")
{
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create swapchain");
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
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create render pass");
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
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create frame buffers");
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
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create command buffers");
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
	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger("Create semaphores");
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
}  // namespace vulkandemo::setup
