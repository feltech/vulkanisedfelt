// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/queue_submission.hpp
 *
 * Native queue submission with retained command resources.
 */
#pragma once

#include <concepts>
#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../../backend/detail/api.hpp"
#include "../clear_commands_types.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../queue_submission_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to submit a recording to a queue. */
template <class TEnvironment>
concept vk_queue_submit_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment const & environment, VkQueue queue, VkSubmitInfo const * info, VkFence fence)
{
    {environment.api.get().vk_queue_submit(queue, 1, info, fence)}->std::same_as<VkResult>;
};

/**
 * Submits one command buffer with its native Vulkan status.
 *
 * @param api Borrowed command implementation.
 * @param queue Borrowed graphics queue.
 * @param command_buffer Borrowed executable command buffer.
 * @param wait_semaphore Optional binary semaphore to wait for.
 * @param signal_semaphore Optional binary semaphore to signal.
 * @param completion_fence Unsignaled fence for accepted work.
 * @return The original submission status.
 */
template <class TApi>
auto queue_submit(
    std::reference_wrapper<TApi> api,
    VkQueue queue,
    VkCommandBuffer command_buffer,
    VkSemaphore wait_semaphore,
    VkSemaphore signal_semaphore,
    VkFence completion_fence) -> VkResult
    pre(queue != VK_NULL_HANDLE && command_buffer != VK_NULL_HANDLE &&
        completion_fence != VK_NULL_HANDLE)
{
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = wait_semaphore != VK_NULL_HANDLE ? 1 : 0;
    info.pWaitSemaphores = wait_semaphore != VK_NULL_HANDLE ? &wait_semaphore : nullptr;
    info.pWaitDstStageMask = wait_semaphore != VK_NULL_HANDLE ? &wait_stage : nullptr;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &command_buffer;
    info.signalSemaphoreCount = signal_semaphore != VK_NULL_HANDLE ? 1 : 0;
    info.pSignalSemaphores = signal_semaphore != VK_NULL_HANDLE ? &signal_semaphore : nullptr;
    return api.get().vk_queue_submit(queue, 1, &info, completion_fence);
}

/**
 * Executes queue submission as an IO action retaining its resources.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param queue Graphics queue from the recording's pool family.
 * @param recording Executable one-time recording.
 * @param wait_semaphore Optional binary semaphore to wait for.
 * @param signal_semaphore Optional binary semaphore to signal.
 * @param completion_fence Unsignaled fence for the accepted work.
 * @return Accepted submission keepalives or the original Vulkan failure.
 */
template <class TApi>
auto queue_submit_io_action(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    VkQueue queue,
    RecordedClearCommands recording,
    semaphore_ptr wait_semaphore,
    semaphore_ptr signal_semaphore,
    fence_ptr completion_fence) -> submitted_clear_commands_result
    pre(device != nullptr && queue != nullptr && recording.command_buffer != nullptr)
        pre(recording.framebuffer != nullptr && recording.render_pass != nullptr)
            pre(completion_fence != nullptr)
{
    namespace result = fnfelt::result;
    VkResult status = queue_submit(
        api,
        queue,
        recording.command_buffer.get(),
        wait_semaphore.get(),
        signal_semaphore.get(),
        completion_fence.get());
    if (status != VK_SUCCESS)
    {
        return result::error<submitted_clear_commands_result>(
            VulkanFailure{FallibleVulkanOperation::queue_submit, status});
    }
    return result::success<submitted_clear_commands_result>(SubmittedClearCommands{
        std::move(device),
        std::move(wait_semaphore),
        std::move(signal_semaphore),
        std::move(completion_fence),
        std::move(recording)});
}

/**
 * Provides queue submission from a borrowed command implementation.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param queue Graphics queue from the recording's pool family.
 * @param recording Executable one-time recording.
 * @param wait_semaphore Optional binary semaphore to wait for.
 * @param signal_semaphore Optional binary semaphore to signal.
 * @param completion_fence Unsignaled completion fence.
 * @return Deferred native submission.
 */
template <class TApi>
constexpr auto io_queue_submit(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    VkQueue queue,
    RecordedClearCommands recording,
    semaphore_ptr wait_semaphore,
    semaphore_ptr signal_semaphore,
    fence_ptr completion_fence)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing one native submission with its command and synchronization owners.
    auto submit = std::bind_front(
        queue_submit_io_action<TApi>,
        api,
        std::move(device),
        queue,
        std::move(recording),
        std::move(wait_semaphore),
        std::move(signal_semaphore),
        std::move(completion_fence));

    // IO providing accepted work keepalives or the native submission failure.
    return io::create<"vk_queue_submit"_ss>(std::move(submit));
}

/**
 * Provides queue submission from borrowed command capabilities.
 *
 * @param device Retained logical device.
 * @param queue Graphics queue from the recording's pool family.
 * @param recording Executable one-time recording.
 * @param wait_semaphore Optional binary semaphore to wait for.
 * @param signal_semaphore Optional binary semaphore to signal.
 * @param completion_fence Unsignaled completion fence.
 * @param environment Borrowed command implementation.
 * @return Deferred native submission.
 */
template <class TEnvironment>
constexpr auto queue_submit_reader_io_action(
    device_ptr device,
    VkQueue queue,
    RecordedClearCommands recording,
    semaphore_ptr wait_semaphore,
    semaphore_ptr signal_semaphore,
    fence_ptr completion_fence,
    TEnvironment const & environment)
{
    return io_queue_submit(
        environment.api,
        std::move(device),
        queue,
        std::move(recording),
        std::move(wait_semaphore),
        std::move(signal_semaphore),
        std::move(completion_fence));
}
}  // namespace vulkandemo::vulkan::detail
