// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/submit_and_present.hpp
 *
 * Bounded submission, presentation and completion.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./clear_commands_types.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./submit_and_present_types.hpp"
#include "detail/submit_and_present.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans one submission and presentation, awaiting safe resource release before returning.
 *
 * Acquisition must already be host-completed, with no pending acquisition semaphore signal.
 * The recording must target the acquired image. Device, queue, pool and image ownership must
 * satisfy the native submission and presentation requirements, with maintenance1 enabled.
 * Supply fresh unsignaled fences and an unused rendering semaphore; execute this plan once.
 * Externally serialize queue, pool and swapchain access. Returned errors require abandoning the
 * swapchain, releasing all aliases and retained plans rather than attempting another frame.
 * Unknown pending-work completion is reported and terminates without unwinding pending owners.
 * Device loss is returned only after a wait permits teardown; it does not indicate successful work.
 *
 * @param context Retained presentation-capable device.
 * @param recording Executable commands for the acquired image.
 * @param acquired_image Acquired image whose signaling has completed on the host.
 * @param rendering_semaphore Unused binary semaphore consumed by presentation.
 * @param submission_fence Unsignaled unused submission fence.
 * @param presentation_fence Unsignaled unused presentation-release fence.
 * @return ReaderIO providing completed presentation or an error permitting resource release.
 */
template <detail::submit_and_present_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_submit_and_present(
    PresentationDevice context,
    RecordedClearCommands recording,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr rendering_semaphore,
    fence_ptr submission_fence,
    fence_ptr presentation_fence)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing bounded queue execution with ownership across every completion branch.
    auto submit_and_present = std::bind_front(
        detail::submit_and_present_reader_io_action<TEnvironment>,
        std::move(context),
        std::move(recording),
        std::move(acquired_image),
        std::move(rendering_semaphore),
        std::move(submission_fence),
        std::move(presentation_fence));

    // ReaderIO providing a completed frame or a failure after safe resource release.
    return reader_io::create<"submit_and_present"_ss, TEnvironment>(std::move(submit_and_present));
}
}  // namespace vulkandemo::vulkan
