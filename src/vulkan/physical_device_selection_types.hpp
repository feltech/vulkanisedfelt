// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_selection_types.hpp
 *
 * Physical-device policy scores and owned selections.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "./extension_types.hpp"
#include "./managed_handles_types.hpp"
#include "./physical_devices_types.hpp"
#include "./queue_family_selection_types.hpp"
#include "./queue_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
struct PhysicalDeviceScoreTag;
/** A lower-is-better physical-device policy cost. */
using physical_device_score =
    strong::type<std::uint32_t, PhysicalDeviceScoreTag, strong::regular, strong::strongly_ordered>;

/** A physical device's original index and lower-is-better policy score. */
struct PhysicalDeviceScore
{
    /** Index in the original physical-device snapshot. */
    physical_device_index device_index;
    /** Policy cost; lower values are preferred. */
    physical_device_score score;
};

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

/** A presentation-capable physical device retaining its instance and surface. */
struct SelectedPresentationPhysicalDevice
{
    /** Parent instance; its borrowed API must outlive this value. */
    instance_ptr instance;
    /** Surface whose presentation requirements were assessed. */
    surface_ptr surface;
    /** Physical device belonging to the retained instance. */
    VkPhysicalDevice physical_device;
    /** Graphics and presentation queue families. */
    PresentationQueueFamilies queue_families;
    /** Compatible device extension whose maintenance1 feature must be enabled at creation. */
    extension_name maintenance1_extension;
};

/** Outcome of presentation-aware physical-device selection. */
using presentation_physical_device_selection_result =
    renderer_result<SelectedPresentationPhysicalDevice>;
}  // namespace vulkandemo::vulkan
