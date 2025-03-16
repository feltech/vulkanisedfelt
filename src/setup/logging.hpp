// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#pragma once

#include <set>
#include <span>

#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../types.hpp"

namespace vulkandemo::setup
{

void log_layer_info(
	LoggerPtr const & logger,
	std::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
	std::set<types::AvailableInstanceLayerNameView> const & available_layer_names,
	std::span<VkLayerProperties const> available_layer_descs);

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

}  // namespace vulkandemo::setup