// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_types.hpp
 *
 * Distinct queue-family and queue indices.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct QueueFamilyIndexTag;
/** An index in a physical device's queue-family snapshot. */
using queue_family_index = strong::type<std::uint32_t, QueueFamilyIndexTag, strong::regular>;

struct QueueIndexTag;
/** An index within a device's requested queues in one family. */
using queue_index = strong::type<std::uint32_t, QueueIndexTag, strong::regular>;
}  // namespace vulkandemo::vulkan
