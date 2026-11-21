// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_presentation_types.hpp
 *
 * Queued presentation requests and retained owners.
 */
#pragma once

#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * A queued presentation request with its image and synchronization owners.
 *
 * Retention does not establish completion. Keep all owners until presentation-safe completion
 * is established. A rendering fence or queue/device idle wait is not a presentation completion
 * guarantee. Acquisition signals require their own completion before final release.
 */
struct QueuedPresentation
{
    /** Device owning the presented resources. */
    device_ptr device;
    /** Original acquisition keepalives; presenting releases the image's acquisition. */
    AcquiredSwapchainImage acquired_image;
    /** Optional binary semaphore consumed by presentation. */
    semaphore_ptr wait_semaphore;
    /** Whether presentation succeeded with a suboptimal swapchain. */
    bool is_suboptimal;
};
}  // namespace vulkandemo::vulkan
