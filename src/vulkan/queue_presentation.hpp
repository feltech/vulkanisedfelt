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
 * Plans presentation of one acquired image with a presentation-resource release fence.
 *
 * The device must have its selected swapchain maintenance1 extension and feature enabled, as
 * established by reader_io_create_presentation_device. All handles must belong to the device,
 * and the queue must support the image's surface.
 * The acquired image must be in presentation layout with ownership available to that queue.
 * Complete rendering before presentation, using the optional semaphore when needed; its signal
 * must already be submitted and no other queue may wait on it. Externally serialize host access
 * to the queue, swapchain, wait semaphore and fence. The fence must be unsignaled and have no
 * outstanding association. Present once and reacquire before further image use.
 * Each execution requires a currently acquired image; retaining its original index is not
 * acquisition.
 *
 * Keep copies of every input across every outcome. Success and suboptimal presentation associate
 * the fence, as do out-of-date, surface-lost and exclusive-mode-lost rejections: wait for the fence
 * before releasing their presentation resources. Host/device allocation failure leaves referenced
 * state unchanged, so do not wait for that unassociated fence. Device loss requires a device-loss
 * cleanup policy; another error or a timed-out wait is not proof of safe release.
 *
 * Fence signaling permits release of the wait semaphore and, after all its requests complete,
 * the swapchain. It does not prove display completion or completion of acquisition or rendering.
 * Keep the API alive through final release. No waits, retries or device-loss recovery are added.
 *
 * @param device Retained logical device.
 * @param queue Queue supporting presentation to the acquired image's surface.
 * @param acquired_image Acquired image index and retained signal owners.
 * @param wait_semaphore Optional binary semaphore ordering presentation after rendering.
 * @param presentation_fence Nonempty unsignaled fence for presentation-resource release.
 * @return ReaderIO providing queued request owners or the original Vulkan failure.
 */
template <detail::vk_queue_present_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_queue_present_khr(
    device_ptr device,
    VkQueue queue,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr wait_semaphore,
    fence_ptr presentation_fence)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing one presentation request with its image and semaphore owners.
    auto queue_present = std::bind_front(
        detail::queue_present_reader_io_action<TEnvironment>,
        std::move(device),
        queue,
        std::move(acquired_image),
        std::move(wait_semaphore),
        std::move(presentation_fence));

    // ReaderIO providing queued request keepalives without claiming presentation completion.
    return reader_io::create<"vk_queue_present_khr"_ss, TEnvironment>(std::move(queue_present));
}
}  // namespace vulkandemo::vulkan
