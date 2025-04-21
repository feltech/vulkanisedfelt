// SPDX-License-Identifier: MIT
// Copyright 2024-2025 David Feltell
#include "io.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <frozen/unordered_map.h>
#include <gsl/pointers>
#include <range/v3/range/conversion.hpp>
#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(*-include-cleaner)
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../macros.hpp"
#include "../types.hpp"

namespace vulkandemo::setup
{

types::VulkanPipelineLayoutPtr create_minimal_pipeline_layout(types::VulkanDevicePtr const & device)
{
	constexpr VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};

	VkPipelineLayout out = nullptr;
	VK_CHECK(
		vkCreatePipelineLayout(device.get(), &pipeline_layout_create_info, nullptr, &out),
		"Failed to create pipeline layout");
	return types::make_pipeline_layout_ptr(device, out);
}

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

	return image_views |
		std::views::transform(
			   [&](types::VulkanImageViewPtr const & image_view)
			   {
				   VkImageView image_view_handle = image_view.get();
				   frame_buffer_create_info.pAttachments = &image_view_handle;
				   VkFramebuffer out = nullptr;
				   VK_CHECK(
					   vkCreateFramebuffer(device.get(), &frame_buffer_create_info, nullptr, &out),
					   "Failed to create framebuffer");
				   frame_buffer_create_info.pAttachments = nullptr;	 // reset.
				   return types::make_framebuffer_ptr(device, out);
			   }) |
		ranges::to<std::vector>();
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

VkSurfaceCapabilitiesKHR query_surface_capabilities(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface)
{
	VkSurfaceCapabilitiesKHR surface_capabilities{};
	VK_CHECK(
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			physical_device, surface.get(), &surface_capabilities),
		"Failed to get surface capabilities");
	return surface_capabilities;
}

std::vector<VkPresentModeKHR> query_present_modes(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface)
{
	uint32_t count = 0;
	VK_CHECK(
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface.get(), &count, nullptr),
		"Failed to get present mode count");
	std::vector<VkPresentModeKHR> out(count);
	VK_CHECK(
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, surface.get(), &count, out.data()),
		"Failed to get present modes");
	return out;
}

types::VulkanSwapchainPtr create_swapchain(
	types::VulkanDevicePtr const & device, VkSwapchainCreateInfoKHR const & create_info)
{
	VkSwapchainKHR out = nullptr;
	VK_CHECK(
		vkCreateSwapchainKHR(device.get(), &create_info, nullptr, &out),
		"Failed to create swapchain");
	return types::make_swapchain_ptr(device, out);
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

	// Create an image view for each swapchain image.
	return swapchain_images |
		std::views::transform(
			   [&](VkImage image)
			   {
				   image_view_create_info.image = image;
				   VkImageView image_view = nullptr;
				   VK_CHECK(
					   vkCreateImageView(
						   device.get(), &image_view_create_info, nullptr, &image_view),
					   "Failed to create image view");
				   return types::make_image_view_ptr(device, image_view);
			   }) |
		ranges::to<std::vector>;
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
	VkSurfaceCapabilitiesKHR surface_capabilities =
		query_surface_capabilities(physical_device, surface);

	// Get present modes.
	std::vector<VkPresentModeKHR> const present_modes =
		query_present_modes(physical_device, surface);

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
	uint32_t const swapchain_image_count = [&]
	{
		uint32_t count = std::max(2U, surface_capabilities.minImageCount);
		// maxImageCount==0 means unlimited.
		if (surface_capabilities.maxImageCount > 0)
			count = std::min(surface_capabilities.maxImageCount, count);
		return count;
	}();

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

	return create_swapchain(device, swapchain_create_info);
}

}  // namespace

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

std::vector<VkSurfaceFormatKHR> enumerate_physical_device_surface_formats(
	VkPhysicalDevice physical_device, types::VulkanSurfacePtr const & surface)
{
	uint32_t count = 0;
	VK_CHECK(
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface.get(), &count, nullptr),
		"Failed to get surface format count");
	std::vector<VkSurfaceFormatKHR> out(count);
	VK_CHECK(
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface.get(), &count, out.data()),
		"Failed to get surface formats");
	return out;
}

void log_surface_format_selection(
	LoggerPtr const & logger,
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	std::span<VkSurfaceFormatKHR const> filtered_surface_formats,
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats)
{
	if (logger->should_log(spdlog::level::debug))
	{
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

		if (logger->should_log(spdlog::level::trace))
		{
			for (auto const & [format, color_space] : available_surface_formats)
				logger->trace(
					"\tAvailable surface format: {} {}",
					string_VkFormat(format),
					string_VkColorSpaceKHR(color_space));
		}
	}
}

std::tuple<types::VulkanDevicePtr, types::MapOfVulkanQueueFamilyIdxToVectorOfQueues>
create_device_and_queues(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> device_extension_names)
{
	types::VulkanDevicePtr device =
		create_device(physical_device, queue_family_and_counts, device_extension_names);
	auto queues = query_queues_for_queue_family_and_counts(device.get(), queue_family_and_counts);
	return {std::move(device), std::move(queues)};
}

types::VulkanDevicePtr create_device(
	VkPhysicalDevice physical_device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const> const
		queue_family_and_counts,
	std::span<types::AvailableDeviceExtensionNameView const> device_extension_names)
{
	std::vector<char const *> const device_extension_cstr_names = device_extension_names |
		hof::views::value_of() | std::views::transform(&std::string_view::data) |
		ranges::to<std::vector>();

	std::vector const queue_priorities(
		std::ranges::max(queue_family_and_counts | std::views::values), 1.0F);

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
	return types::make_device_ptr(device);
}

types::MapOfVulkanQueueFamilyIdxToVectorOfQueues query_queues_for_queue_family_and_counts(
	VkDevice device,
	std::span<std::pair<types::VulkanQueueFamilyIdx, types::VulkanQueueCount> const>
		queue_family_and_counts)
{
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
	return queues;
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

std::vector<VkExtensionProperties> enumerate_physical_device_extension_properties(
	VkPhysicalDevice physical_device)
{
	uint32_t extension_count = 0;
	VK_CHECK(
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr),
		"Failed to get device extension count");
	std::vector<VkExtensionProperties> out(extension_count);
	VK_CHECK(
		vkEnumerateDeviceExtensionProperties(
			physical_device, nullptr, &extension_count, out.data()),
		"Failed to get device extensions");
	return out;
}

VkPhysicalDeviceProperties query_physical_device_properties(VkPhysicalDevice physical_device)
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physical_device, &properties);
	return properties;
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

namespace
{
constexpr auto kMessageTypeToString =
	frozen::make_unordered_map<VkDebugUtilsMessageTypeFlagBitsEXT, char const *>(
		{{VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, "GENERAL"},
		 {VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, "VALIDATION"},
		 {VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, "PERFORMANCE"},
		 {VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT, "DEVICE_ADDRESS"}});

/**
 * Callback function for Vulkan debug messenger.
 *
 * This function is called by Vulkan when a debug message is generated. It filters
 * messages based on the provided severity and type, constructs a detailed message
 * string, and logs it using the specified logger at the appropriate severity level.
 *
 * @param message_severity The severity of the message (info, warning, error).
 * @param message_types The type(s) of the message (general, validation, performance).
 * @param callback_data Detailed data about the message, including message ID and objects involved.
 * @param user_data User-defined data passed to the callback. Expected to be a pointer to LoggerPtr.
 *
 * @return VkBool32 indicating whether the callback should be removed (VK_FALSE) or kept (VK_TRUE).
 */
VkBool32 vulkan_debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
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
			auto type_strings = kMessageTypeToString |
				std::views::filter([&](auto const & type_and_name)
								   { return message_types & type_and_name.first; }) |
				std::views::values;
			return format("{}", fmt::join(type_strings, "|"));
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
		extensions_to_enable | hof::views::value_of(), back_inserter(extensions_to_enable_cstr));

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

	std::vector<char const *> const layers_to_enable_cstr =
		layers_to_enable | hof::views::value_of() | ranges::to<std::vector>;

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
}  // namespace vulkandemo::setup