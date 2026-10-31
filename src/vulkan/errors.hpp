// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/errors.hpp
 *
 * Common failures for fallible Vulkan actions.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>

namespace vulkandemo::vulkan
{
/// Wrapped Vulkan operations that can report a failure.
enum class FallibleVulkanOperation
{
    /// Instance creation.
    create_instance,
    /// Instance extension enumeration.
    enumerate_instance_extensions,
    /// Instance layer enumeration.
    enumerate_instance_layers,
    /// Physical device enumeration.
    enumerate_physical_devices,
    /// Device extension enumeration.
    enumerate_device_extensions,
    /// Logical device creation.
    create_device
};

/// A failed Vulkan operation and its original status.
struct VulkanFailure
{
    /// Operation that failed.
    FallibleVulkanOperation operation;
    /// Original Vulkan status.
    VkResult result;
};

/// Common outcome of fallible Vulkan actions.
template <class T>
using vulkan_result = fnfelt::Result<T, VulkanFailure>;
}  // namespace vulkandemo::vulkan
