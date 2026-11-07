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

#include "./api_version_types.hpp"
#include "./errors.hpp"

namespace vulkandemo::vulkan
{
/// A Vulkan failure in the renderer error domain.
struct VulkanOperationFailure
{
    /// Original operation and status.
    VulkanFailure cause;
};

/** The instance implementation does not support the required Vulkan API version. */
struct UnsupportedVulkanVersion
{
    /** Minimum supported renderer API version. */
    api_version required;
    /** Instance version reported by the implementation. */
    api_version available;
};

/// Common failures of renderer workflows.
using renderer_error = std::variant<VulkanOperationFailure, UnsupportedVulkanVersion>;

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
