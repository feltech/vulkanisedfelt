// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_selection.hpp
 *
 * Pure ranking of physical-device snapshots.
 */
#pragma once

#include <optional>

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

/**
 * Selects the first entry with the lowest supplied score.
 *
 * @param scores Physical-device policy scores.
 * @return Original device index, or an empty value when no scores are supplied.
 */
[[nodiscard]] auto select_physical_device(immer::array<PhysicalDeviceScore> const & scores)
    -> std::optional<physical_device_index>;
}  // namespace vulkandemo::vulkan
