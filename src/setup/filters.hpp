// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#pragma once
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/set.hpp>

#include "../Logger.hpp"
#include "../macros.hpp"
#include "../types.hpp"

namespace vulkandemo::setup
{

std::optional<std::pair<VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
maybe_select_best_scoring_physical_device_and_queue_family_idx(
	immer::array<std::tuple<std::size_t, VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
		candidates);

std::optional<std::tuple<std::size_t, VkPhysicalDevice, types::VulkanQueueFamilyIdx>>
maybe_score_physical_device_and_queue_family(
	VkPhysicalDevice physical_device,
	VkPhysicalDeviceProperties const & physical_device_properties,
	immer::array<types::VulkanQueueFamilyIdx> const & filtered_queue_family_idxs);

// Filters available device extensions by desired names and transforms to those present on the
// device
immer::array<types::AvailableDeviceExtensionNameView>
extension_properties_filter_by_and_transform_to_device_extension_name(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	immer::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names,
	immer::array<VkExtensionProperties> const & available_device_extensions);

struct extension_properties_filter_by_and_transform_to_device_extension_name_t
{
	struct with_logger_and_physical_device_and_desired_device_extension_names_t
	{
		LoggerPtr logger;
		VkPhysicalDevice physical_device;
		immer::set<types::DesiredDeviceExtensionNameView> desired_device_extension_names;
		constexpr immer::array<types::AvailableDeviceExtensionNameView> operator()(
			immer::array<VkExtensionProperties> const & available_device_extensions) const
		{
			return extension_properties_filter_by_and_transform_to_device_extension_name(
				logger,
				physical_device,
				desired_device_extension_names,
				available_device_extensions);
		}
	};
};

immer::array<types::VulkanQueueFamilyIdx>
queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
	VkQueueFlagBits desired_queue_capabilities,
	immer::array<VkQueueFamilyProperties> const & queue_family_properties);

struct transform_queue_family_properties_to_queue_family_idxs_filtered_by_capability_t
{
	struct with_desired_queue_capabilities_t
	{
		VkQueueFlagBits desired_queue_capabilities;

		constexpr immer::array<types::VulkanQueueFamilyIdx> operator()(
			immer::array<VkQueueFamilyProperties> const & queue_family_properties) const
		{
			return queue_family_properties_filter_by_capability_and_transform_to_queue_family_idx(
				desired_queue_capabilities, queue_family_properties);
		}
	};
};

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

// Filters memory types by property flags and transforms to memory type indices
immer::array<types::VulkanMemoryTypeIdx>
memory_properties_filter_by_and_transform_to_memory_type_idx(
	VkMemoryPropertyFlags memory_property_flags,
	VkPhysicalDeviceMemoryProperties const & memory_properties);

struct memory_properties_filter_by_and_transform_to_memory_type_idx_t
{
	struct with_memory_property_flags_t
	{
		VkMemoryPropertyFlags memory_property_flags;

		constexpr immer::array<types::VulkanMemoryTypeIdx> operator()(
			VkPhysicalDeviceMemoryProperties const & memory_properties) const
		{
			return memory_properties_filter_by_and_transform_to_memory_type_idx(
				memory_property_flags, memory_properties);
		}
	};
};

// Filters available instance layers by desired names and transforms to C string pointers
immer::array<types::AvailableInstanceLayerNameCstr>
layer_description_filter_by_and_transform_to_instance_layer_name(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	immer::array<VkLayerProperties> const & available_layer_descs);

struct transform_to_instance_layer_name_filtered_by_instance_layer_name_t
{
	LoggerPtr logger;
	immer::set<types::DesiredInstanceLayerNameView> desired_layer_names;

	constexpr immer::array<types::AvailableInstanceLayerNameCstr> operator()(
		immer::array<VkLayerProperties> const & available_layer_descs) const
	{
		return layer_description_filter_by_and_transform_to_instance_layer_name(
			logger, desired_layer_names, available_layer_descs);
	}
};

constexpr auto make_layer_description_filter_by_and_transform_to_instance_layer_name(
	AUTO(LoggerPtr) logger, AUTO(std::set<types::DesiredInstanceLayerNameView>) desired_layer_names)
{
	return [logger = FW(logger), desired_layer_names = FW(desired_layer_names)](
			   AUTO(std::vector<VkLayerProperties>) available_layer_descs)
	{
		return layer_description_filter_by_and_transform_to_instance_layer_name(
			logger, desired_layer_names, FW(available_layer_descs));
	};
}

/**
 * Filters available instance extensions by desired names and transforms to C string pointers
 */
immer::array<types::AvailableInstanceExtensionNameCstr>
extension_properties_filter_by_and_transform_to_instance_extension_name(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
	immer::array<VkExtensionProperties> const & available_extensions);

struct transform_to_instance_extension_name_filtered_by_instance_extension_name_t
{
	LoggerPtr logger;
	immer::set<types::DesiredInstanceExtensionNameView> desired_extension_names;

	constexpr immer::array<types::AvailableInstanceExtensionNameCstr> operator()(
		this auto && self, immer::array<VkExtensionProperties> const & available_extensions)
	{
		return extension_properties_filter_by_and_transform_to_instance_extension_name(
			FW(self).logger, FW(self).desired_extension_names, available_extensions);
	}
};

/**
 * Filter and prioritize available VkSurfaceFormatKHRs by desired VkFormat order.
 * @param available_surface_formats
 * @param desired_formats
 * @return
 */
immer::array<VkSurfaceFormatKHR> filter_surface_formats(
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats);

// Filters and logs available VkSurfaceFormatKHRs by desired VkFormat order.
immer::array<VkSurfaceFormatKHR> filter_surface_formats(
	LoggerPtr const & logger,
	std::span<VkSurfaceFormatKHR const> available_surface_formats,
	std::span<VkFormat const> desired_formats);


struct filter_surface_formats_t
{
	static constexpr auto operator()(
		LoggerPtr const & logger,
		std::span<VkFormat const> desired_formats,
		std::span<VkSurfaceFormatKHR const> formats)
	{
		return filter_surface_formats(logger, formats, desired_formats);
	}

	struct with_logger_and_desired_formats_t
	{
		LoggerPtr logger;
		immer::array<VkFormat> desired_formats;

		constexpr auto operator()(std::span<VkSurfaceFormatKHR const> formats) const
		{
			return filter_surface_formats(logger, formats, desired_formats);
		}
	};
};

// Returns a VkSwapchainCreateInfoKHR configured for exclusive sharing mode and double buffering (or
// as close as possible).
VkSwapchainCreateInfoKHR exclusive_double_buffer_swapchain_create_info(
	LoggerPtr const & logger,
	types::VulkanSurfacePtr const & surface,
	VkSurfaceFormatKHR const & surface_format,
	VkSurfaceCapabilitiesKHR const & surface_capabilities,
	std::span<VkPresentModeKHR const> present_modes,
	types::VulkanSwapchainPtr const & previous_swapchain = nullptr);

}  // namespace vulkandemo::setup