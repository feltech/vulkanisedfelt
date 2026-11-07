// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/renderer_errors.hpp
 *
 * Shared error domain for renderer workflows.
 */
#pragma once

#include <optional>
#include <variant>

#include <fnfelt/result.hpp>

#include "./errors.hpp"
#include "./strong_types.hpp"

namespace vulkandemo::vulkan
{
/// A Vulkan failure with renderer operation context.
struct VulkanOperationFailure
{
    /// Original operation and status.
    VulkanFailure cause;
    /// Original device index, absent when no enumeration index is available.
    std::optional<physical_device_index> device_index;
};

/// No discovered device satisfies the requirements.
struct NoSuitablePhysicalDevice
{
};

/// Common failures of renderer workflows.
using renderer_error = std::variant<VulkanOperationFailure, NoSuitablePhysicalDevice>;

/// Common outcome of fallible renderer workflows.
template <class T>
using renderer_result = fnfelt::Result<T, renderer_error>;
}  // namespace vulkandemo::vulkan
