// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_selection_types.hpp
 *
 * Owned result of physical-device selection.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include "./managed_handles.hpp"
#include "./renderer_errors.hpp"
#include "./strong_types.hpp"

namespace vulkandemo::vulkan
{
/// A selected physical device with its parent lifetime retained.
struct SelectedPhysicalDevice
{
    /// Parent owner. Its borrowed API must outlive this value.
    instance_ptr instance;
    /// Physical device belonging to the instance.
    VkPhysicalDevice physical_device;
    /// Queue family satisfying the requested capabilities.
    vulkan::queue_family_index queue_family_index;
};

/// Outcome of headless device selection.
using physical_device_selection_result = renderer_result<SelectedPhysicalDevice>;
}  // namespace vulkandemo::vulkan
