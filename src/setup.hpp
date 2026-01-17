// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <concepts>
#include <cstdint>
#include <immer/set.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <ranges>
#include <set>
#include <span>
#include <spdlog/common.h>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/to_container.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include "Logger.hpp"
#include "hof.hpp"
#include "macros.hpp"
#include "types.hpp"

namespace vulkandemo::setup
{
/**
 * Given some desired image/surface formats (e.g. VK_FORMAT_B8G8R8_UNORM), filter to only those
 * suppored by the device and surface.
 *
 * Will preserve ordering, so that @p desired_formats can be in priority order.
 *
 * @param logger
 * @param physical_device
 * @param surface
 * @param desired_formats
 * @return
 */
// [DEPRECATED] Use enumerate_physical_device_surface_formats (io.hpp) and filter_surface_formats
// (filters.hpp) instead.
[[deprecated]] std::vector<VkSurfaceFormatKHR> filter_available_surface_formats(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	types::VulkanSurfacePtr const & surface,
	std::span<VkFormat const> desired_formats);

/**
 * Given a list of physical devices, pick the first that has desired capabilities.
 *
 * @param logger
 * @param physical_devices
 * @param required_device_extensions
 * @param required_queue_capabilities
 * @param required_memory_type
 * @param required_surface_support
 * @return
 */
[[deprecated]] std::tuple<VkPhysicalDevice, types::VulkanQueueFamilyIdx> select_physical_device(
	LoggerPtr const & logger,
	std::vector<VkPhysicalDevice> const & physical_devices,
	std::set<types::DesiredDeviceExtensionNameView> const & required_device_extensions,
	VkQueueFlagBits required_queue_capabilities,
	VkMemoryPropertyFlags required_memory_type = 0,
	types::VulkanSurfacePtr const & required_surface_support = nullptr);

/**
 * Given a device and set of desired device extensions, filter to only those extensions that
 * are supported by the device.
 *
 * @param logger
 * @param physical_device
 * @param desired_device_extension_names
 * @return
 */
[[deprecated]] [[nodiscard]] std::vector<types::AvailableDeviceExtensionNameView>
filter_available_device_extensions(
	LoggerPtr const & logger,
	VkPhysicalDevice physical_device,
	std::set<types::DesiredDeviceExtensionNameView> const & desired_device_extension_names);

/**
 * Filter queue families to find those with desired capabilities
 *
 * @param physical_device Device to check queue families for
 * @param desired_queue_capabilities Required queue capabilities
 * @param desired_surface
 * @return
 */
[[deprecated]] [[nodiscard]] std::vector<types::VulkanQueueFamilyIdx>
filter_available_queue_families(
	VkPhysicalDevice const & physical_device,
	VkQueueFlagBits desired_queue_capabilities,
	types::VulkanSurfacePtr const & desired_surface = nullptr);

/**
 * Given a device and set of desired memory properties, filter to only those memory types that
 * are supported by the device and have the desired properties.
 *
 * @param logger
 * @param physical_device Device to check memory types for.
 * @param memory_flags Required memory properties.
 * @return
 */
[[deprecated]] [[nodiscard]] std::vector<types::VulkanMemoryTypeIdx> filter_available_memory_types(
	LoggerPtr const & logger, VkPhysicalDevice physical_device, VkMemoryPropertyFlags memory_flags);

/**
 * Query available layers vs. desired layers.

 * @param logger
 * @param desired_layer_names
 * @return
 */
[[deprecated]] std::vector<types::AvailableInstanceLayerNameCstr> filter_available_layers(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceLayerNameView> const & desired_layer_names);

/**
 * Query available generic instance extensions vs. desired..
 *
 * @param logger
 * @param desired_extension_names
 * @return
 */
[[deprecated]] std::vector<types::AvailableInstanceExtensionNameCstr>
filter_available_instance_extensions(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names);

}  // namespace vulkandemo::setup