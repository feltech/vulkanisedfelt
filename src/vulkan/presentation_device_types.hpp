// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_device_types.hpp
 *
 * Owned device, surface and queues for presentation work.
 */
#pragma once

#include <vulkan/vulkan_core.h>

#include "./managed_handles_types.hpp"
#include "./queue_family_selection_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/** A logical device retaining the parents of its presentation queues. */
struct PresentationDevice
{
    /** Surface retained for subsequent swapchain work. */
    surface_ptr surface;
    /** Device owner, transitively retaining its instance. */
    device_ptr device;
    /** Physical device belonging to the retained instance. */
    VkPhysicalDevice physical_device;
    /** Families containing the requested graphics and presentation queues. */
    PresentationQueueFamilies queue_families;
    /** Queue zero in the graphics family, borrowed from the device. */
    VkQueue graphics_queue;
    /** Queue zero in the presentation family, borrowed from the device. */
    VkQueue present_queue;
};

/** Outcome of presentation device creation. */
using presentation_device_result = renderer_result<PresentationDevice>;
}  // namespace vulkandemo::vulkan
