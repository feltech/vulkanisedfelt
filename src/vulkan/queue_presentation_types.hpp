// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_presentation_types.hpp
 *
 * Queued presentation requests and retained owners.
 */
#pragma once

#include "./errors.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * A queued presentation request with its image and synchronization owners.
 *
 * Keep all owners until the presentation fence signals, and release the swapchain only after all
 * its presentation fences signal. Signaling permits resource release rather than proving display
 * completion. Rendering and acquisition signals require their own completion before final release.
 */
struct QueuedPresentation
{
    /** Device owning the presented resources. */
    device_ptr device;
    /** Original acquisition keepalives; presenting releases the image's acquisition. */
    AcquiredSwapchainImage acquired_image;
    /** Optional binary semaphore consumed by presentation. */
    semaphore_ptr wait_semaphore;
    /** Presentation-resource release fence associated with this request. */
    fence_ptr presentation_fence;
    /** Whether presentation succeeded with a suboptimal swapchain. */
    bool is_suboptimal;
};

/** Outcome of queuing one presentation request with a release fence. */
using queued_presentation_result = vulkan_result<QueuedPresentation>;
}  // namespace vulkandemo::vulkan
