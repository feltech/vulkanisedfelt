// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/image_acquisition.hpp
 *
 * Deferred acquisition of a swapchain image.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./synchronization_types.hpp"
#include "detail/image_acquisition.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans acquisition of one presentable image.
 *
 * The swapchain must be non-retired and all handles must belong to the device. Supply at least
 * one signal owner: a binary semaphore or a fence, each unsignaled with no pending use. Externally
 * synchronize host access to those objects and the swapchain. An unbounded timeout is permitted
 * only when the surface guarantees forward progress.
 *
 * Successful acquisition retains its owners but does not wait for signaling to finish. Keep them
 * alive until signaling and image use complete; consume the semaphore signal or wait for the fence
 * before image use. The result neither presents nor automatically releases the acquired image.
 * Success always provides an acquired image, including suboptimal acquisition. Timeout, not-ready
 * and other non-acquisition statuses use the error channel with their original Vulkan status, so
 * the frame layer can recover with `or_else`. This synchronous call occupies its executing worker.
 *
 * @param device Retained logical device.
 * @param swapchain Retained non-retired image owner.
 * @param timeout Maximum wait in nanoseconds.
 * @param semaphore Optional binary semaphore to signal.
 * @param fence Optional fence to signal.
 * @return ReaderIO providing an acquired image or the original non-acquisition Vulkan status.
 */
template <detail::vk_acquire_next_image_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_acquire_next_image_khr(
    device_ptr device,
    swapchain_ptr swapchain,
    timeout_nanoseconds timeout,
    semaphore_ptr semaphore,
    fence_ptr fence)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing image acquisition with all native owners retained.
    auto acquire_next_image = std::bind_front(
        detail::acquire_next_image_reader_io_action<TEnvironment>,
        std::move(device),
        std::move(swapchain),
        timeout,
        std::move(semaphore),
        std::move(fence));

    // ReaderIO providing a retained image index or a native status for recovery.
    return reader_io::create<"vk_acquire_next_image_khr"_ss, TEnvironment>(
        std::move(acquire_next_image));
}
}  // namespace vulkandemo::vulkan
