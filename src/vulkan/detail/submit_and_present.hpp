// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/submit_and_present.hpp
 *
 * Bounded queue execution and safe resource release.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/state_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../clear_commands_types.hpp"
#include "../errors.hpp"
#include "../image_acquisition_types.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_device_types.hpp"
#include "../queue_presentation_types.hpp"
#include "../queue_submission_types.hpp"
#include "../submit_and_present_types.hpp"
#include "../synchronization_types.hpp"
#include "./fence.hpp"
#include "./queue_presentation.hpp"
#include "./queue_submission.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed to await resource release or report a fatal completion failure. */
template <class TEnvironment>
concept frame_release_environment = vk_wait_for_fence_environment<TEnvironment> &&
    requires(TEnvironment const & environment, VulkanFailure failure)
{
    {environment.api.get().report_fatal_error(failure)}->std::same_as<void>;
};

/** Capabilities needed for bounded submission and presentation. */
template <class TEnvironment>
concept submit_and_present_environment = vk_queue_submit_environment<TEnvironment> &&
    vk_queue_present_environment<TEnvironment> && frame_release_environment<TEnvironment>;

/**
 * Waits until resources are safe to release or reports an unrecoverable failure.
 *
 * @param api Borrowed command implementation.
 * @param device Device owning the pending operations.
 * @param fence Fence associated with those operations.
 * @return Native success or device loss permitting resource release.
 */
template <class TApi>
auto wait_for_frame_release(std::reference_wrapper<TApi> api, VkDevice device, VkFence fence)
    -> VkResult
{
    constexpr std::uint64_t indefinitely = std::numeric_limits<std::uint64_t>::max();
    VkResult status = wait_for_fence(api, device, fence, indefinitely);
    if (status != VK_SUCCESS && status != VK_ERROR_DEVICE_LOST)
    {
        api.get().report_fatal_error(
            VulkanFailure{FallibleVulkanOperation::wait_for_fence, status});
    }
    return status;
}

/**
 * Executes a frame-release wait as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param fence Retained completion fence.
 * @return Success or a device-loss error, both permitting resource release.
 */
template <class TApi>
auto wait_for_frame_release_io_action(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence) -> vulkan_result<void>
{
    namespace result = fnfelt::result;
    VkResult status = wait_for_frame_release(api, device.get(), fence.get());
    if (status == VK_ERROR_DEVICE_LOST)
    {
        return result::error<vulkan_result<void>>(
            VulkanFailure{FallibleVulkanOperation::wait_for_fence, status});
    }
    return result::success<vulkan_result<void>>();
}

/** Original owners retained independently of native operation results. */
struct FrameExecution
{
    /** Presentation-capable device and its queue parents. */
    PresentationDevice context;
    /** Executable clear commands and their render targets. */
    RecordedClearCommands recording;
    /** Host-completed acquisition and its swapchain owner. */
    AcquiredSwapchainImage acquired_image;
    /** Unused binary rendering signal. */
    semaphore_ptr rendering_semaphore;
    /** Unsignaled completion fence for submitted commands. */
    fence_ptr submission_fence;
    /** Unsignaled release fence for presentation resources. */
    fence_ptr presentation_fence;
};

/**
 * Handles a native submission failure as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Device owning the submitted work.
 * @param fence Submission's completion fence.
 * @param failure Native submission failure.
 * @return The original failure after any required completion wait.
 */
template <class TApi>
auto handle_submission_failure_io_action(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence, VulkanFailure failure)
    -> submitted_clear_commands_result
{
    namespace result = fnfelt::result;
    if (failure.result == VK_ERROR_DEVICE_LOST)
    {
        (void)wait_for_frame_release(api, device.get(), fence.get());
    }
    else if (
        failure.result != VK_ERROR_OUT_OF_HOST_MEMORY &&
        failure.result != VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        api.get().report_fatal_error(failure);
    }
    return result::error<submitted_clear_commands_result>(failure);
}

/**
 * Handles a native presentation failure as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Device owning the presentation work.
 * @param fence Presentation's release fence.
 * @param failure Native presentation failure.
 * @return The original failure after any required completion wait.
 */
template <class TApi>
auto handle_presentation_failure_io_action(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence, VulkanFailure failure)
    -> queued_presentation_result
{
    namespace result = fnfelt::result;
    if (failure.result == VK_ERROR_OUT_OF_HOST_MEMORY ||
        failure.result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        // Allocation failure leaves the presentation fence unassociated.
        return result::error<queued_presentation_result>(failure);
    }
    if (failure.result != VK_ERROR_OUT_OF_DATE_KHR && failure.result != VK_ERROR_SURFACE_LOST_KHR &&
        failure.result != VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT &&
        failure.result != VK_ERROR_DEVICE_LOST)
    {
        api.get().report_fatal_error(failure);
    }
    // Rejections may enqueue waits; device loss during the wait does not replace their error.
    (void)wait_for_frame_release(api, device.get(), fence.get());
    return result::error<queued_presentation_result>(failure);
}

/**
 * Provides one completion wait with the frame's terminal policy.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param fence Fence associated with pending work.
 * @return IO establishing resource release or device loss.
 */
template <class TApi>
constexpr auto io_wait_for_frame_release(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    // Callable providing native completion while retaining its resource owners.
    auto wait = std::bind_front(
        wait_for_frame_release_io_action<TApi>, api, std::move(device), std::move(fence));

    // IO permitting release only after completion or device loss has been established.
    return io::create<"wait_for_frame_release"_ss>(std::move(wait));
}

/**
 * Provides submission failure handling within the frame's ownership scope.
 *
 * @param api Borrowed command implementation.
 * @param device Device owning the submitted work.
 * @param fence Submission's completion fence.
 * @param failure Native submission failure.
 * @return IO preserving the original failure after any required completion wait.
 */
template <class TApi>
constexpr auto io_handle_submission_failure(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence, VulkanFailure failure)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing the original submission failure after any required completion wait.
    auto handle_failure = std::bind_front(
        handle_submission_failure_io_action<TApi>,
        api,
        std::move(device),
        std::move(fence),
        failure);

    // IO providing the submission failure only after its pending work permits release.
    return io::create<"handle_submission_failure"_ss>(std::move(handle_failure));
}

/**
 * Provides presentation failure handling within the frame's ownership scope.
 *
 * @param api Borrowed command implementation.
 * @param device Device owning the presentation work.
 * @param fence Presentation's release fence.
 * @param failure Native presentation failure.
 * @return IO preserving the original failure after any required completion wait.
 */
template <class TApi>
constexpr auto io_handle_presentation_failure(
    std::reference_wrapper<TApi> api, device_ptr device, fence_ptr fence, VulkanFailure failure)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing the original presentation failure after any required completion wait.
    auto handle_failure = std::bind_front(
        handle_presentation_failure_io_action<TApi>,
        api,
        std::move(device),
        std::move(fence),
        failure);

    // IO providing the presentation failure only after its pending work permits release.
    return io::create<"handle_presentation_failure"_ss>(std::move(handle_failure));
}

/**
 * Provides completion of accepted submission work.
 *
 * @param api Borrowed command implementation.
 * @param submission Accepted submission.
 * @return IO permitting resource release on success or device loss.
 */
template <class TApi>
constexpr auto io_complete_submission(
    std::reference_wrapper<TApi> api, SubmittedClearCommands submission)
{
    // The enclosing StateIO retains every frame resource throughout this wait.
    return io_wait_for_frame_release(
        api, std::move(submission.device), std::move(submission.completion_fence));
}

/**
 * Provides a completed presentation after its release wait.
 *
 * @param api Borrowed command implementation.
 * @param image_index Acquired swapchain image index.
 * @param is_acquisition_suboptimal Whether acquisition requested future recreation.
 * @param presentation Accepted presentation request.
 * @return IO providing a completed frame or device loss.
 */
template <class TApi>
constexpr auto io_complete_presentation(
    std::reference_wrapper<TApi> api,
    swapchain_image_index image_index,
    bool is_acquisition_suboptimal,
    QueuedPresentation presentation)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    // IO establishing release of presentation resources before the summary can be returned.
    auto completion = io_wait_for_frame_release(
        api, std::move(presentation.device), std::move(presentation.presentation_fence));

    // Summary data survives the wait; resource ownership belongs to the enclosing StateIO.
    CompletedPresentation summary{
        image_index, is_acquisition_suboptimal || presentation.is_suboptimal};
    auto summarize = [summary] { return summary; };
    return io::transform<"completed_presentation"_ss>(std::move(completion), std::move(summarize));
}

/**
 * Provides submission and its required completion before presentation may begin.
 *
 * @param api Borrowed command implementation.
 * @param frame Original frame owners.
 * @return IO providing submission completion or a failure permitting resource release.
 */
template <class TApi>
constexpr auto io_submit_frame(std::reference_wrapper<TApi> api, FrameExecution const & frame)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    // IO providing native submission owners, independently of later wait failures.
    auto submission = io_queue_submit(
        api,
        frame.context.device,
        frame.context.graphics_queue,
        frame.recording,
        {},
        frame.rendering_semaphore,
        frame.submission_fence);

    // Recovery applies only to native submission errors inside the frame's ownership scope.
    auto handle_failure = std::bind_front(
        io_handle_submission_failure<TApi>, api, frame.context.device, frame.submission_fence);
    auto submission_with_failure_handling = io::or_else<"handle_failed_submission"_ss>(
        std::move(submission), std::move(handle_failure));

    // Continuation waits only after a successful submission; its errors bypass native recovery.
    auto complete_submission = std::bind_front(io_complete_submission<TApi>, api);
    return io::and_then<"complete_submitted_frame"_ss>(
        std::move(submission_with_failure_handling), std::move(complete_submission));
}

/**
 * Provides presentation and its required release after completed rendering.
 *
 * @param api Borrowed command implementation.
 * @param frame Original frame owners.
 * @return IO providing completed presentation or a failure permitting resource release.
 */
template <class TApi>
constexpr auto io_present_frame(std::reference_wrapper<TApi> api, FrameExecution const & frame)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    // IO providing the native presentation outcome after rendering has completed.
    auto presentation = io_queue_present(
        api,
        frame.context.device,
        frame.context.present_queue,
        frame.acquired_image,
        frame.rendering_semaphore,
        frame.presentation_fence);

    // Recovery applies only to native presentation errors, including enqueued rejections.
    auto handle_failure = std::bind_front(
        io_handle_presentation_failure<TApi>, api, frame.context.device, frame.presentation_fence);
    auto presentation_with_failure_handling = io::or_else<"handle_failed_presentation"_ss>(
        std::move(presentation), std::move(handle_failure));

    // Continuation waits after successful presentation, independently of native-error recovery.
    auto complete_presentation = std::bind_front(
        io_complete_presentation<TApi>,
        api,
        frame.acquired_image.image_index,
        frame.acquired_image.is_suboptimal);
    return io::and_then<"complete_presented_frame"_ss>(
        std::move(presentation_with_failure_handling), std::move(complete_presentation));
}

/**
 * Provides bounded queue execution from a borrowed command implementation.
 *
 * @param api Borrowed command implementation.
 * @param context Retained presentation device.
 * @param recording Executable recording.
 * @param acquired_image Completed acquisition.
 * @param rendering_semaphore Unused rendering semaphore.
 * @param submission_fence Unused submission fence.
 * @param presentation_fence Unused presentation fence.
 * @return IO providing a completed frame or a failure permitting resource release.
 */
template <class TApi>
constexpr auto io_submit_and_present(
    std::reference_wrapper<TApi> api,
    PresentationDevice context,
    RecordedClearCommands recording,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr rendering_semaphore,
    fence_ptr submission_fence,
    fence_ptr presentation_fence)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    namespace state_io = fnfelt::monad::state_io;
    using frame_state = immer::box<FrameExecution>;
    // GCC does not support preconditions on this deduced-return factory.
    contract_assert(rendering_semaphore != nullptr);
    contract_assert(submission_fence != nullptr && presentation_fence != nullptr);
    contract_assert(submission_fence != presentation_fence);
    frame_state state{FrameExecution{
        std::move(context),
        std::move(recording),
        std::move(acquired_image),
        std::move(rendering_semaphore),
        std::move(submission_fence),
        std::move(presentation_fence)}};

    // IO providing submission completion before presentation can begin.
    auto submission = io_submit_frame(api, state.get());

    // The borrowed frame belongs to the boxed StateIO scope throughout this continuation.
    auto present = std::bind_front(io_present_frame<TApi>, api, std::cref(state.get()));
    auto execution =
        io::and_then<"submit_and_present"_ss>(std::move(submission), std::move(present));

    // StateIO retains the complete frame on success, error and stopped outcomes.
    auto scoped = state_io::lift<"frame_ownership_scope"_ss, frame_state>(std::move(execution));
    auto bound = std::move(scoped).build(std::move(state)).io();

    // Leaving the inner scope forwards its whole Result and discards the retained state.
    return io::transform<"leave_frame_ownership_scope"_ss>(
        std::move(bound),
        [](auto outcome_and_state) { return std::move(outcome_and_state.first); });
}

/**
 * Provides bounded queue execution from borrowed capabilities.
 *
 * @param context Retained presentation device.
 * @param recording Executable recording.
 * @param acquired_image Completed acquisition.
 * @param rendering_semaphore Unused rendering semaphore.
 * @param submission_fence Unused submission fence.
 * @param presentation_fence Unused presentation fence.
 * @param environment Borrowed command capabilities.
 * @return IO providing a completed frame or a failure permitting resource release.
 */
template <class TEnvironment>
constexpr auto submit_and_present_reader_io_action(
    PresentationDevice context,
    RecordedClearCommands recording,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr rendering_semaphore,
    fence_ptr submission_fence,
    fence_ptr presentation_fence,
    TEnvironment const & environment)
{
    return io_submit_and_present(
        environment.api,
        std::move(context),
        std::move(recording),
        std::move(acquired_image),
        std::move(rendering_semaphore),
        std::move(submission_fence),
        std::move(presentation_fence));
}
}  // namespace vulkandemo::vulkan::detail
