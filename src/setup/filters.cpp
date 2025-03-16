// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#include "filters.hpp"

#include <set>
#include <string_view>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <ranges>
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../hof.hpp"
#include "../types.hpp"
#include "logging.hpp"

// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
namespace vulkandemo::setup
{
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

std::vector<types::AvailableInstanceLayerNameCstr>
layer_descriptions_filter_by_and_transform_to_instance_layer_name(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::vector<VkLayerProperties> const& available_layer_descs)
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

}  // namespace vulkandemo::setup
