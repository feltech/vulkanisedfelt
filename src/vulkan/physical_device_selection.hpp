// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_selection.hpp
 *
 * Pure ranking of physical-device snapshots.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./strong_types.hpp"

namespace vulkandemo::vulkan
{
/** A physical device's original index and lower-is-better policy score. */
struct PhysicalDeviceScore
{
    /** Index in the original physical-device snapshot. */
    physical_device_index device_index;
    /** Policy cost; lower values are preferred. */
    physical_device_score score;
};

/**
 * Ranks discrete GPUs ahead of other physical-device types.
 *
 * Entries retain their original indices and snapshot order. Ranking does not establish suitability.
 *
 * @param properties Physical-device property snapshots.
 * @return Scores of zero for discrete GPUs and one for every other type.
 */
[[nodiscard]] auto score_physical_devices(
    immer::array<VkPhysicalDeviceProperties> const & properties)
    -> immer::array<PhysicalDeviceScore>;
}  // namespace vulkandemo::vulkan
