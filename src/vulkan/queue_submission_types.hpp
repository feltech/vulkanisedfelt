// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_submission_types.hpp
 *
 * Submitted clear commands and their completion keepalives.
 */
#pragma once

#include "./clear_commands_types.hpp"
#include "./errors.hpp"
#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Accepted queue work with its recorded resources and synchronization owners.
 *
 * Keep the complete value alive until the completion fence signals. A timed-out wait or device
 * loss does not establish completion. This value adds no waits or automatic device-loss policy.
 * Retain acquisition owners separately until their own signaling operations complete.
 */
struct SubmittedClearCommands
{
    /** Device owning the submitted resources and completion fence. */
    device_ptr device;
    /** Optional binary semaphore consumed before color-attachment output. */
    semaphore_ptr wait_semaphore;
    /** Optional binary semaphore signaled by this submission. */
    semaphore_ptr signal_semaphore;
    /** Fence identifying completion of the accepted work. */
    fence_ptr completion_fence;
    // Release the command buffer before the synchronization objects and logical device.
    /** Recording retaining its framebuffer and render pass. */
    RecordedClearCommands recording;
};

/** Outcome of submitting one clear-command recording. */
using submitted_clear_commands_result = vulkan_result<SubmittedClearCommands>;
}  // namespace vulkandemo::vulkan
