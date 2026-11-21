// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_instance_extensions.hpp
 *
 * Pure selection of instance prerequisites for maintenance1 presentation.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./extension_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/**
 * Selects all available maintenance1 instance paths alongside required platform extensions.
 *
 * Platform names keep their first occurrence and precede additional dependencies. Both KHR and
 * EXT paths are retained when available; device-extension and feature support are checked later.
 *
 * @param required_names Platform-required instance extension names.
 * @param available_extensions Globally available instance extension properties.
 * @return Selected owned names or the unavailable mandatory requirements.
 */
auto select_presentation_instance_extensions(
    immer::array<extension_name> required_names,
    immer::array<VkExtensionProperties> available_extensions)
    -> renderer_result<immer::array<extension_name>>;
}  // namespace vulkandemo::vulkan
