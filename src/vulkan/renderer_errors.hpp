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

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "./api_version_types.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"

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

/** The instance implementation does not support the required Vulkan API version. */
struct UnsupportedVulkanVersion
{
    /** Minimum supported renderer API version. */
    api_version required;
    /** Instance version reported by the implementation. */
    api_version available;
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

/** A selected image extent exceeds the device's framebuffer dimensions. */
struct UnsupportedFramebufferExtent
{
    /** Selected swapchain image extent. */
    VkExtent2D requested;
    /** Maximum framebuffer dimensions of the physical device. */
    VkExtent2D maximum;
};

/** Unavailable instance prerequisites for maintenance1 presentation support. */
enum class PresentationInstanceFailureReason
{
    /** Required platform or shared dependency names are unavailable. */
    missing_required_extensions,
    /** Neither maintenance1 surface-extension family is available. */
    no_maintenance1_path
};

/** The available instance extensions cannot support maintenance1 presentation. */
struct PresentationInstanceFailure
{
    /** Unavailable requirement. */
    PresentationInstanceFailureReason reason;
    /** Missing mandatory names; empty when neither alternative family is available. */
    immer::array<extension_name> missing_extensions;
};

/// Common failures of renderer workflows.
using renderer_error = std::variant<
    VulkanOperationFailure,
    UnsupportedVulkanVersion,
    NoSuitablePhysicalDevice,
    NoSuitableSwapchainConfiguration,
    UnsupportedFramebufferExtent,
    PresentationInstanceFailure>;

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
