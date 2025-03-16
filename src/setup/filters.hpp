// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#pragma once
#include <cstdint>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/common.h>
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../macros.hpp"
#include "../types.hpp"
#include "logging.hpp"

namespace vulkandemo::setup
{

constexpr auto make_layer_description_filter_by_and_transform_to_instance_layer_name(
	AUTO(LoggerPtr) logger, AUTO(std::set<types::DesiredInstanceLayerNameView>) desired_layer_names)
{
	return [logger = FW(logger),
			desired_layer_names = FW(desired_layer_names)](auto && available_layer_descs)
	{
		auto const available_layer_names = available_layer_descs |
			std::views::transform(&VkLayerProperties::layerName) |
			ranges::to<std::set<types::AvailableInstanceLayerNameView>>;

		log_layer_info(logger, desired_layer_names, available_layer_names, available_layer_descs);

		// Get intersection of desired layers and available layers, converted to C strings.
		return ranges::views::set_intersection(
				   desired_layer_names | hof::views::value_of(),
				   available_layer_names | hof::views::value_of()) |
			ranges::views::transform(&std::string_view::data) |
			ranges::to<std::vector<types::AvailableInstanceLayerNameCstr>>;
	};
}

constexpr auto make_extension_properties_filter_by_and_transform_to_device_extension_name(
	AUTO(LoggerPtr) logger,
	VkPhysicalDevice physical_device,
	AUTO(std::set<types::DesiredDeviceExtensionNameView>) desired_device_extension_names)
{
	return [logger = FW(logger),
			physical_device,
			desired_device_extension_names = FW(desired_device_extension_names)](
			   AUTO(std::vector<VkExtensionProperties>) available_device_extensions)
	{
		if (desired_device_extension_names.empty())
			return std::vector<types::AvailableDeviceExtensionNameView>{};

		std::set<types::AvailableDeviceExtensionNameView> const available_device_extension_names =
			available_device_extensions |
			std::views::transform(
				[](VkExtensionProperties const & extension)
				{ return types::AvailableDeviceExtensionNameView{extension.extensionName}; }) |
			ranges::to<std::set>();

		// NOLINTNEXTLINE(misc-const-correctness): due to performance-no-automatic-move in return.
		std::vector<types::AvailableDeviceExtensionNameView> extensions_to_enable =
			ranges::views::set_intersection(
				available_device_extension_names | hof::views::value_of(),
				desired_device_extension_names | hof::views::value_of()) |
			ranges::to<std::vector<types::AvailableDeviceExtensionNameView>>();

		// TODO(DF): remove logging or move to IO. Not so bad since its just DEBUG level, but still.
		if (logger->should_log(spdlog::level::debug))
		{
			// Log requested extensions and whether they are available.
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(physical_device, &device_properties);
			logger->debug(
				"Requested device extensions for device {}:", device_properties.deviceName);

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
	};
}

constexpr auto make_queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
	VkQueueFlagBits const desired_queue_capabilities)
{
	return [desired_queue_capabilities](AUTO(std::vector<VkQueueFamilyProperties>)
											queue_family_properties)
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
			ranges::to<std::vector<types::VulkanQueueFamilyIdx>>;
	};
}

constexpr auto make_instance_extension_appender(
	AUTO(std::vector<types::AvailableInstanceExtensionNameCstr>) additional_instance_extensions)
{
	return [additional_instance_extensions = FW(additional_instance_extensions)](
			   std::vector<types::AvailableInstanceExtensionNameCstr> instance_extensions)
	{
		instance_extensions.insert(
			instance_extensions.end(),
			additional_instance_extensions.begin(),
			additional_instance_extensions.end());

		return instance_extensions;
	};
}

constexpr auto make_memory_properties_filter_by_and_transform_to_memory_type_idx(
	VkMemoryPropertyFlags const memory_property_flags)
{
	return [memory_property_flags](AUTO(VkPhysicalDeviceMemoryProperties) memory_properties)
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
	};
}

// Filters available instance layers by desired names and transforms to C string pointers
std::vector<types::AvailableInstanceLayerNameCstr>
layer_descriptions_filter_by_and_transform_to_instance_layer_name(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::vector<VkLayerProperties> const & available_layer_descs);

constexpr auto make_layer_descriptions_filter_by_and_transform_to_instance_layer_name(
	AUTO(LoggerPtr) logger, AUTO(std::set<types::DesiredInstanceLayerNameView>) desired_layer_names)
{
	return [logger = FW(logger), desired_layer_names = FW(desired_layer_names)](
			   AUTO(std::vector<VkLayerProperties>) available_layer_descs)
	{
		return layer_descriptions_filter_by_and_transform_to_instance_layer_name(
			logger, desired_layer_names, FW(available_layer_descs));
	};
}

/**
 * Filters available instance extensions by desired names and transforms to C string pointers
 */
std::vector<types::AvailableInstanceExtensionNameCstr>
extension_properties_filter_by_and_transform_to_instance_extension_name(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	std::vector<VkExtensionProperties> const & available_extensions);

constexpr auto make_extension_properties_filter_by_and_transform_to_instance_extension_name(
	AUTO(LoggerPtr) logger,
	AUTO(std::set<types::DesiredInstanceExtensionNameView>) desired_extension_names)
{
	return [logger = FW(logger), desired_extension_names = FW(desired_extension_names)](
			   AUTO(std::vector<VkExtensionProperties>) available_extensions)
	{
		return extension_properties_filter_by_and_transform_to_instance_extension_name(
			logger, desired_extension_names, available_extensions);
	};
}
}  // namespace vulkandemo::setup