// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/fence_types.hpp
 *
 * Outcomes of waiting for a Vulkan fence.
 */
#pragma once

namespace vulkandemo::vulkan
{
/** Whether a fence became signaled before its wait expired. */
enum class FenceWaitStatus
{
    /** The fence is signaled. */
    signaled,
    /** The fence did not become signaled within the requested time. */
    timed_out
};
}  // namespace vulkandemo::vulkan
