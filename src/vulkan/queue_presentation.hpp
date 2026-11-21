// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_presentation.hpp
 *
 * Deferred presentation of one acquired swapchain image.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "detail/queue_presentation.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans presentation of one acquired image with optional binary semaphore ordering.
 *
 * All handles must belong to the device, and the queue must support the image's surface.
 * The acquired image must be in presentation layout with ownership available to that queue.
 * Complete rendering before presentation, using the optional semaphore when needed; its signal
 * must already be submitted and no other queue may wait on it. Externally serialize host access
 * to the queue, swapchain and wait semaphore. Present once and reacquire before further image use.
 * Each execution requires a currently acquired image; retaining its original index is not
 * acquisition.
 *
 * Keep successful request owners until presentation-safe completion is established, such as a
 * later reacquisition of the same image and its completed signal. Submission fences and idle waits
 * do not prove presentation completion. Retain original inputs across errors: out-of-date and
 * surface-lost results can still enqueue semaphore waits. Retain recorded/submitted resources
 * and acquisition owners separately until their uses complete. This plan adds no waits,
 * presentation-fence extension support, retries or cleanup policy.
 *
 * @param device Retained logical device.
 * @param queue Queue supporting presentation to the acquired image's surface.
 * @param acquired_image Acquired image index and retained signal owners.
 * @param wait_semaphore Optional binary semaphore ordering presentation after rendering.
 * @return ReaderIO providing queued request owners or the original Vulkan failure.
 */
template <detail::vk_queue_present_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_queue_present_khr(
    device_ptr device,
    VkQueue queue,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr wait_semaphore)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing one presentation request with its image and semaphore owners.
    auto io_queue_present = std::bind_front(
        detail::io_queue_present<TEnvironment>,
        std::move(device),
        queue,
        std::move(acquired_image),
        std::move(wait_semaphore));

    // ReaderIO providing queued request keepalives without claiming presentation completion.
    return reader_io::create<"vk_queue_present_khr"_ss, TEnvironment>(std::move(io_queue_present));
}
}  // namespace vulkandemo::vulkan
