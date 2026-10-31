// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/strong_types.hpp
 *
 * Distinct numeric domains for Vulkan selection and queue operations.
 */
#pragma once

#include <cstdint>

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct QueueFamilyIndexTag;
/** An index in a physical device's queue-family snapshot. */
using queue_family_index = strong::type<std::uint32_t, QueueFamilyIndexTag, strong::regular>;

struct QueueFamilyScoreTag;
/** A lower-is-better queue-family policy cost. */
using queue_family_score =
    strong::type<std::uint32_t, QueueFamilyScoreTag, strong::regular, strong::strongly_ordered>;
}  // namespace vulkandemo::vulkan
