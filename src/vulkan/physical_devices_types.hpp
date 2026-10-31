// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_devices_types.hpp
 *
 * Identity within physical-device snapshots.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct PhysicalDeviceIndexTag;
/** An index in an instance's physical-device snapshot. */
using physical_device_index = strong::type<std::uint32_t, PhysicalDeviceIndexTag, strong::regular>;
}  // namespace vulkandemo::vulkan
