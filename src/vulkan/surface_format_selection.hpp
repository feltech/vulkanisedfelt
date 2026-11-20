// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/surface_format_selection.hpp
 *
 * Pure selection of supported surface format and color-space pairs.
 */
#pragma once

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

namespace vulkandemo::vulkan
{
/**
 * Selects the first advertised pair with an acceptable image format.
 *
 * Acceptable formats form an allowlist, preserving the advertised order and color space.
 *
 * @param supported_formats Advertised surface format and color-space pairs.
 * @param acceptable_formats Image formats allowed by the rendering policy.
 * @return Supported pair, or no value when no advertised format is acceptable.
 */
[[nodiscard]] auto select_surface_format(
    immer::array<VkSurfaceFormatKHR> const & supported_formats,
    immer::array<VkFormat> const & acceptable_formats = {
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}) -> std::optional<VkSurfaceFormatKHR>;
}  // namespace vulkandemo::vulkan
