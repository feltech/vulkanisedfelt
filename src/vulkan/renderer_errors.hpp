// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/renderer_errors.hpp
 *
 * Shared error domain for renderer workflows.
 */
#pragma once

#include <variant>

#include <fnfelt/result.hpp>

#include "./errors.hpp"

namespace vulkandemo::vulkan
{
/// A Vulkan failure in the renderer error domain.
struct VulkanOperationFailure
{
    /// Original operation and status.
    VulkanFailure cause;
};

/// No discovered device satisfies the requirements.
struct NoSuitablePhysicalDevice
{
};

/** Why the initial swapchain cannot use the queried surface and image parameters. */
enum class SwapchainConfigurationFailure
{
    /** The selected drawable area has a zero dimension. */
    unavailable_extent,
    /** No advertised format satisfies the renderer's format policy. */
    no_acceptable_surface_format,
    /** No advertised presentation mode satisfies the renderer's mode policy. */
    no_preferred_present_mode,
    /** The selected image description exceeds the physical device's support. */
    unsupported_image_parameters
};

/** The renderer cannot create a swapchain from the available surface and image choices. */
struct NoSuitableSwapchainConfiguration
{
    /** Unavailable requirement. */
    SwapchainConfigurationFailure reason;
};

/// Common failures of renderer workflows.
using renderer_error = std::
    variant<VulkanOperationFailure, NoSuitablePhysicalDevice, NoSuitableSwapchainConfiguration>;

/**
 * Converts a Vulkan failure into a renderer error.
 *
 * @param failure Original Vulkan operation and status.
 * @return Renderer-domain Vulkan failure.
 */
constexpr renderer_error renderer_error_from_vulkan_failure(VulkanFailure failure)
{
    return VulkanOperationFailure{failure};
}

/// Common outcome of fallible renderer workflows.
template <class T>
using renderer_result = fnfelt::Result<T, renderer_error>;
}  // namespace vulkandemo::vulkan
