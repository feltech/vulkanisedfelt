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

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "./physical_devices_types.hpp"

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
}  // namespace vulkandemo::vulkan
