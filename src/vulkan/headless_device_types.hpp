// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/headless_device_types.hpp
 *
 * Owned device and queue for headless work.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include "./managed_handles.hpp"
#include "./renderer_errors.hpp"
#include "./strong_types.hpp"

namespace vulkandemo::vulkan
{
/** A logical device retaining the parents of its borrowed handles. */
struct HeadlessDevice
{
    /** Device owner, transitively retaining its instance. */
    device_ptr device;
    /** Physical device belonging to the retained instance. */
    VkPhysicalDevice physical_device;
    /** Family containing the device's single requested queue. */
    vulkan::queue_family_index queue_family_index;
    /** Queue zero in the selected family, borrowed from the device. */
    VkQueue queue;
};

/** Outcome of headless device creation. */
using headless_device_result = renderer_result<HeadlessDevice>;
}  // namespace vulkandemo::vulkan
