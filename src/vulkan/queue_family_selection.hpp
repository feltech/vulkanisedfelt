// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_family_selection.hpp
 *
 * Pure selection of queue families from capability snapshots.
 */
#pragma once

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./strong_types.hpp"

namespace vulkandemo::vulkan
{
/** An eligible queue family's index and lower-is-better policy score. */
struct QueueFamilyScore
{
    /** Index in the original queue-family snapshot. */
    queue_family_index family_index;
    /** Policy cost; lower values are preferred. */
    queue_family_score score;
};

/**
 * Scores eligible queue families by their additional capabilities.
 *
 * Entries retain their original family indices and snapshot order.
 *
 * @param families Queue-family snapshot returned by Vulkan.
 * @param required_flags Capabilities every candidate must support, or zero for no requirements.
 * @return Scores for nonempty families supporting every required capability.
 */
[[nodiscard]] auto score_queue_families(
    immer::array<VkQueueFamilyProperties> const & families, VkQueueFlags required_flags)
    -> immer::array<QueueFamilyScore>;

/**
 * Selects the lowest-scored family, keeping the first entry on ties.
 *
 * Scores must identify eligible families in the original snapshot.
 *
 * @param scores Candidate indices and policy scores.
 * @return Selected family index, or no value when the score array is empty.
 */
[[nodiscard]] auto select_queue_family(immer::array<QueueFamilyScore> const & scores)
    -> std::optional<queue_family_index>;
}  // namespace vulkandemo::vulkan
