// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/managed_handles_types.hpp
 *
 * Shared ownership types for Vulkan handles.
 */
#pragma once

#include <memory>

#include <vulkan/vulkan_core.h>

namespace vulkandemo::vulkan
{
/** Shared instance owner. */
using instance_ptr = std::shared_ptr<VkInstance_T>;

/** Shared logical device owner. */
using device_ptr = std::shared_ptr<VkDevice_T>;
}  // namespace vulkandemo::vulkan
