// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#include "filters.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../types.hpp"
#include "logging.hpp"

namespace vulkandemo::setup
{

std::optional<std::tuple<std::size_t, VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
maybe_score_physical_device_and_queue_family(
	VkPhysicalDevice physical_device,
	VkPhysicalDeviceProperties const & physical_device_properties,
	std::vector<types::VulkanQueueFamilyIdx> const & filtered_queue_family_idxs)
{
	if (filtered_queue_family_idxs.empty())
		return std::nullopt;

	std::size_t const score =
		(physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 1 : 0;
	return std::make_tuple(score, physical_device, filtered_queue_family_idxs.front());
}

std::optional<std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
maybe_select_best_scoring_physical_device_and_queue_family_idx(
	std::vector<std::tuple<std::size_t, VkPhysicalDevice, types::VulkanQueueFamilyIdx>> candidates)
{
	if (candidates.empty())
		return std::nullopt;

	std::ranges::sort(
		candidates,
		[](auto const & lhs, auto const & rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });
	auto const & [score, physical_device, queue_family_idx] = candidates.back();
	return std::pair{physical_device, queue_family_idx};
}

std::vector<types::AvailableDeviceExtensionNameView>
extension_properties_filter_by_and_transform_to_device_extension_name(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	std::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names,
	std::vector<VkExtensionProperties> const & available_device_extensions)
{
	if (desired_device_extension_names.empty())
		return {};

	std::set<types::AvailableDeviceExtensionNameView> const available_device_extension_names =
		available_device_extensions |
		std::views::transform(
			[](VkExtensionProperties const & extension)
			{ return types::AvailableDeviceExtensionNameView{extension.extensionName}; }) |
		ranges::to<std::set>();

	std::vector<types::AvailableDeviceExtensionNameView> extensions_to_enable =
		ranges::views::set_intersection(
			available_device_extension_names | hof::views::value_of(),
			desired_device_extension_names | hof::views::value_of()) |
		ranges::to<std::vector<types::AvailableDeviceExtensionNameView>>();

	if (logger->should_log(spdlog::level::debug))
	{
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

		logger->trace("Available device extensions:");
		for (auto const & extension_name : available_device_extension_names)
			logger->trace("\t{}", extension_name);
	}

	return extensions_to_enable;
}

std::vector<types::VulkanQueueFamilyIdx>
queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
	VkQueueFlagBits desired_queue_capabilities,
	std::vector<VkQueueFamilyProperties> const & queue_family_properties)
{
	return std::views::enumerate(queue_family_properties) |
		std::views::filter(
			   [&](auto const idx_and_queue_family_properties)
			   {
				   return (std::get<1>(idx_and_queue_family_properties).queueFlags &
						   desired_queue_capabilities) == desired_queue_capabilities;
			   }) |
		std::views::transform([](auto const idx_and_queue_family_properties)
							  { return std::get<0>(idx_and_queue_family_properties); }) |
		ranges::to<std::vector<types::VulkanQueueFamilyIdx>>();
}

std::vector<types::VulkanMemoryTypeIdx>
memory_properties_filter_by_and_transform_to_memory_type_idx(
	VkMemoryPropertyFlags const memory_property_flags,
	VkPhysicalDeviceMemoryProperties const & memory_properties)
{
	std::span const memory_types =
		std::span{memory_properties.memoryTypes}.subspan(0, memory_properties.memoryTypeCount);
	return std::views::iota(0U, memory_types.size()) |
		std::views::filter(
			   [&](uint32_t const idx)
			   {
				   return (memory_types[idx].propertyFlags & memory_property_flags) ==
					   memory_property_flags;
			   }) |
		ranges::to<std::vector<types::VulkanMemoryTypeIdx>>();
}

std::vector<types::AvailableInstanceLayerNameCstr>
layer_description_filter_by_and_transform_to_instance_layer_name(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::vector<VkLayerProperties> const & available_layer_descs)
{
	auto const available_layer_names = available_layer_descs |
		std::views::transform(&VkLayerProperties::layerName) |
		ranges::to<std::set<types::AvailableInstanceLayerNameView>>;

	log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

	// Get intersection of desired layers and available layers, converted to C strings.
	return ranges::views::set_intersection(
			   desired_layer_names | hof::views::value_of(),
			   available_layer_names | hof::views::value_of()) |
		std::views::transform(&std::string_view::data) |
		ranges::to<std::vector<types::AvailableInstanceLayerNameCstr>>;
}

std::vector<types::AvailableInstanceExtensionNameCstr>
extension_properties_filter_by_and_transform_to_instance_extension_name(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	std::vector<VkExtensionProperties> const & available_extensions)
{
	std::set available_extension_names = available_extensions |
		std::views::transform(&VkExtensionProperties::extensionName) |
		ranges::to<std::set<types::AvailableInstanceExtensionNameView>>;

	log_instance_extensions_info(
		logger, desired_extension_names, available_extension_names, available_extensions);

	// Intersection of available extensions and desired extensions to return.
	return ranges::views::set_intersection(
			   desired_extension_names | hof::views::value_of(),
			   available_extension_names | hof::views::value_of()) |
		std::views::transform(&std::string_view::data) |
		ranges::to<std::vector<types::AvailableInstanceExtensionNameCstr>>;
}

std::vector<VkSurfaceFormatKHR> filter_surface_formats(
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats)
{
	return available_surface_formats |
		std::views::filter(
			   [&](auto const & available_surface_format)
			   {
				   return std::ranges::contains(desired_formats, available_surface_format.format);
			   }) |
		ranges::to<std::vector>();
}

std::vector<VkSurfaceFormatKHR> filter_surface_formats(
	LoggerPtr const & logger,
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats)
{
	std::vector<VkSurfaceFormatKHR> filtered_surface_formats =
		filter_surface_formats(available_surface_formats, desired_formats);

	if (logger && logger->should_log(spdlog::level::debug))
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

		for (auto const & [format, color_space] : available_surface_formats)
			logger->debug(
				"\tAvailable surface format: {} {}",
				string_VkFormat(format),
				string_VkColorSpaceKHR(color_space));
	}

	return filtered_surface_formats;
}

VkSwapchainCreateInfoKHR exclusive_double_buffer_swapchain_create_info(
	LoggerPtr const & logger,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR const & surface_format,
	VkSurfaceCapabilitiesKHR const & surface_capabilities,
	std::span<VkPresentModeKHR const> present_modes,
	types::VulkanSwapchainPtr const & previous_swapchain)
{
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
	constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ((surface_capabilities.supportedUsageFlags & usage) != usage)
		throw std::runtime_error{"Surface VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT unavailable"};

	VkSwapchainCreateInfoKHR swapchain_create_info{
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
		.oldSwapchain = previous_swapchain ? previous_swapchain.get() : nullptr};
	return swapchain_create_info;
}

}  // namespace vulkandemo::setup
