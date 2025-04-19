// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#include "filters.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <ranges>
#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../types.hpp"
#include "logging.hpp"

namespace vulkandemo::setup
{

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
		ranges::to<std::vector<types::AvailableInstanceExtensionNameCstr>>();
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

}  // namespace vulkandemo::setup
