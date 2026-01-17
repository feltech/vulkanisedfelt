// SPDX-License-Identifier: MIT
// Copyright 2025 David Feltell
#include "logging.hpp"

#include <immer/set.hpp>
#include <set>
#include <span>

#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(*-include-cleaner)
#include <vulkan/vulkan_core.h>

#include "../Logger.hpp"
#include "../types.hpp"

namespace vulkandemo::setup
{

void log_layer_info(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceLayerNameView> const & desired_layer_names,
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

void log_instance_extensions_info(
	LoggerPtr const & logger,
	immer::set<types::DesiredInstanceExtensionNameView> const & desired_extension_names,
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
}  // namespace vulkandemo::setup