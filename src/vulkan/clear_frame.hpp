// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/clear_frame.hpp
 *
 * One bounded acquisition, clear and presentation.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./synchronization_types.hpp"
#include "detail/clear_frame.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans one clear frame with acquisition, rendering and presentation completed before return.
 *
 * The environment owns the targets and command pool for the same presentation device and
 * graphics family. Externally serialize queue, command-pool and swapchain access. Each execution
 * creates fresh synchronization and a one-time recording. No cancellation check interrupts
 * acquired work; upstream cancellation can skip execution before acquisition.
 * After an error following successful acquisition, abandon the swapchain and release all aliases
 * and retained plans through RAII. A failed acquisition leaves no image to abandon.
 * Returned errors permit resource release; unproven pending completion is reported and terminal.
 * The acquisition timeout does not bound completion waits. No retries or swapchain recovery are
 * provided, and successful completion does not prove that the image has finished displaying.
 * Keep the API and main-thread cleanup context alive through final resource release.
 *
 * @tparam TEnvironment Execution capabilities and owned rendering resources.
 * @param clear_color Floating-point clear color.
 * @param acquisition_timeout Maximum image-acquisition wait in nanoseconds.
 * @return ReaderIO providing completed presentation or an error permitting resource release.
 */
template <detail::clear_and_present_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_clear_and_present(
    VkClearColorValue clear_color, timeout_nanoseconds acquisition_timeout)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing fresh synchronization on the environment's presentation device.
    auto synchronization = detail::reader_io_create_clear_frame_synchronization<TEnvironment>();

    // Continuation providing host-completed acquisition for these presentation targets.
    auto acquire_frame =
        std::bind_front(detail::reader_io_acquire_clear_frame<TEnvironment>, acquisition_timeout);

    // ReaderIO providing a host-completed acquisition with no pending signaling on error return.
    auto acquired_frame = reader_io::and_then<"acquire_clear_frame"_ss>(
        std::move(synchronization), std::move(acquire_frame));

    // Continuation providing a clear recording and bounded presentation for the acquired image.
    auto record_and_present = std::bind_front(
        detail::reader_io_record_and_present_clear_frame<TEnvironment>, clear_color);

    // ReaderIO providing recorded, submitted and presented work only when its resources are safe
    // to release.
    return reader_io::and_then<"record_and_present_clear_frame"_ss>(
        std::move(acquired_frame), std::move(record_and_present));
}
}  // namespace vulkandemo::vulkan
