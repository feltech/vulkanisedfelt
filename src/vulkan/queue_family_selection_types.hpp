// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_family_selection_types.hpp
 *
 * Queue-family policy scores and selections.
 */
#pragma once

#include <cstdint>

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "./queue_types.hpp"

namespace vulkandemo::vulkan
{
struct QueueFamilyScoreTag;
/** A lower-is-better queue-family policy cost. */
using queue_family_score =
    strong::type<std::uint32_t, QueueFamilyScoreTag, strong::regular, strong::strongly_ordered>;

/** An eligible queue family's index and lower-is-better policy score. */
struct QueueFamilyScore
{
    /** Index in the original queue-family snapshot. */
    queue_family_index family_index;
    /** Policy cost; lower values are preferred. */
    queue_family_score score;
};

/** Queue families providing graphics and surface presentation. */
struct PresentationQueueFamilies
{
    /** Family supporting graphics commands. */
    queue_family_index graphics_family_index;
    /** Family supporting presentation to the queried surface. */
    queue_family_index present_family_index;
};
}  // namespace vulkandemo::vulkan
