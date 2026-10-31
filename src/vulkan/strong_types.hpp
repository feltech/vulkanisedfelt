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

struct PhysicalDeviceIndexTag;
/** An index in an instance's physical-device snapshot. */
using physical_device_index = strong::type<std::uint32_t, PhysicalDeviceIndexTag, strong::regular>;

struct PhysicalDeviceScoreTag;
/** A lower-is-better physical-device policy cost. */
using physical_device_score =
    strong::type<std::uint32_t, PhysicalDeviceScoreTag, strong::regular, strong::strongly_ordered>;
}  // namespace vulkandemo::vulkan
