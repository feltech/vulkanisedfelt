// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_support.hpp
 *
 * Swapchain support snapshot composition.
 */
#pragma once

#include <tuple>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "../errors.hpp"
#include "../surface_capabilities.hpp"
#include "../surface_formats.hpp"
#include "../surface_present_modes.hpp"
#include "../swapchain_support_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to inspect swapchain support. */
template <class TEnvironment>
concept query_swapchain_support_environment =
    vk_get_physical_device_surface_capabilities_environment<TEnvironment> &&
    vk_get_physical_device_surface_formats_environment<TEnvironment> &&
    vk_get_physical_device_surface_present_modes_environment<TEnvironment>;

/** Successful independent surface support snapshots. */
using swapchain_support_query_values = std::tuple<
    VkSurfaceCapabilitiesKHR,
    immer::array<VkSurfaceFormatKHR>,
    immer::array<VkPresentModeKHR>>;

/**
 * Sequences surface query outcomes into successful snapshots.
 *
 * @param capabilities Capability-query outcome.
 * @param formats Format-query outcome.
 * @param present_modes Present-mode-query outcome.
 * @return Plain snapshots, any stop, or the first query error.
 */
auto sequence_swapchain_support_results(
    vulkan_result<VkSurfaceCapabilitiesKHR> capabilities,
    vulkan_result<immer::array<VkSurfaceFormatKHR>> formats,
    vulkan_result<immer::array<VkPresentModeKHR>> present_modes)
    -> vulkan_result<swapchain_support_query_values>;

/**
 * Combines successful surface support snapshots.
 *
 * @param capabilities Surface limits and supported operations.
 * @param formats Supported format and color-space combinations.
 * @param present_modes Supported presentation modes.
 * @return Owned swapchain support facts.
 */
auto make_swapchain_support(
    VkSurfaceCapabilitiesKHR capabilities,
    immer::array<VkSurfaceFormatKHR> formats,
    immer::array<VkPresentModeKHR> present_modes) -> SwapchainSupport;
}  // namespace vulkandemo::vulkan::detail
