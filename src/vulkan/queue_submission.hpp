// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_submission.hpp
 *
 * Deferred submission of one recorded clear pass.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./clear_commands_types.hpp"
#include "./managed_handles_types.hpp"
#include "detail/queue_submission.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans submission of one executable clear-command recording.
 *
 * All handles must belong to the device. The queue and command pool must use the same graphics
 * family. Submit the one-time recording once, externally serializing host queue and pool access.
 * The completion fence must be unsignaled and unused; semaphore states must satisfy native binary
 * semaphore requirements. An optional acquisition wait uses color-attachment output, matching the
 * clear pass's incoming dependency. Otherwise complete image acquisition on the host before use.
 * Retain the successful submission until its fence signals; timeout or device loss is not proof
 * of completion. Keep original input owners across failure until their resources are safe to
 * release, including acquisition owners whose signals may remain pending. This plan adds no
 * waits, retries, reset, presentation or device-loss cleanup policy.
 *
 * @param device Retained logical device.
 * @param queue Graphics queue compatible with the recording's pool.
 * @param recording Executable clear commands and their referenced resources.
 * @param wait_semaphore Optional binary semaphore to consume before color output.
 * @param signal_semaphore Optional binary semaphore to signal when work finishes.
 * @param completion_fence Unsignaled fence identifying submission completion.
 * @return ReaderIO providing accepted work keepalives or the original Vulkan failure.
 */
template <detail::vk_queue_submit_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_queue_submit(
    device_ptr device,
    VkQueue queue,
    RecordedClearCommands recording,
    semaphore_ptr wait_semaphore,
    semaphore_ptr signal_semaphore,
    fence_ptr completion_fence)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing queue submission while retaining all directly submitted resources.
    auto queue_submit = std::bind_front(
        detail::queue_submit_reader_io_action<TEnvironment>,
        std::move(device),
        queue,
        std::move(recording),
        std::move(wait_semaphore),
        std::move(signal_semaphore),
        std::move(completion_fence));

    // ReaderIO providing accepted work with keepalives that can span a later completion wait.
    return reader_io::create<"vk_queue_submit"_ss, TEnvironment>(std::move(queue_submit));
}
}  // namespace vulkandemo::vulkan
