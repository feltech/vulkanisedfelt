// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/present_mode_selection.hpp
 *
 * Pure selection of supported presentation modes.
 */
#pragma once

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

namespace vulkandemo::vulkan
{
/**
 * Selects the first preferred mode advertised by the surface.
 *
 * @param supported_modes Advertised presentation modes.
 * @param preferred_modes Presentation modes in policy preference order.
 * @return Supported preferred mode, or no value when no preference is available.
 */
[[nodiscard]] auto select_present_mode(
    immer::array<VkPresentModeKHR> const & supported_modes,
    immer::array<VkPresentModeKHR> const & preferred_modes = {
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR}) -> std::optional<VkPresentModeKHR>;
}  // namespace vulkandemo::vulkan
