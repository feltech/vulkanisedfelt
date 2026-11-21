// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_maintenance1_selection.hpp
 *
 * Compatible maintenance1 extension selection for an instance and physical device.
 */
#pragma once

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./extension_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Selects an extension candidate whose instance and device dependencies match.
 *
 * KHR is preferred when both paths are available. Feature support still requires a separate
 * physical-device query before enabling the selected extension.
 *
 * @param enabled_instance_extensions Extension names enabled on the parent instance.
 * @param device_extensions Extension properties advertised by the physical device.
 * @return Compatible device-extension name, or no candidate.
 */
[[nodiscard]] auto select_swapchain_maintenance1_extension(
    immer::array<extension_name> enabled_instance_extensions,
    immer::array<VkExtensionProperties> device_extensions) -> std::optional<extension_name>;
}  // namespace vulkandemo::vulkan
