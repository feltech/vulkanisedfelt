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

#include "./queue_family_selection_types.hpp"
#include "./queue_types.hpp"

namespace vulkandemo::vulkan
{
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

/**
 * Selects graphics and presentation families, preferring a shared family.
 *
 * Each role prefers the fewest additional capabilities, retaining snapshot order on ties.
 * Presentation support must describe the same complete queue-family snapshot.
 *
 * @param families Queue-family capability snapshot.
 * @param presentation_support Support flags in matching family order.
 * @return Selected families, or no value when either role is unavailable.
 */
[[nodiscard]] auto select_presentation_queue_families(
    immer::array<VkQueueFamilyProperties> const & families,
    immer::array<bool> const & presentation_support)
    -> std::optional<PresentationQueueFamilies> pre(families.size() == presentation_support.size());
}  // namespace vulkandemo::vulkan
