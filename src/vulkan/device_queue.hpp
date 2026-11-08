// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/device_queue.hpp
 *
 * Deferred retrieval of a logical device's queues.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles.hpp"
#include "./strong_types.hpp"
#include "detail/device_queue.hpp"

namespace vulkandemo::vulkan
{
/**
 * Retrieves a queue while retaining its owning device through execution.
 *
 * The family and index must identify a queue requested during device creation with zero flags.
 * The returned handle borrows the device; retain a device owner throughout subsequent queue use.
 * The execution and cleanup APIs must outlive their deferred operations and retained owners.
 *
 * @tparam TEnvironment Execution environment type.
 * @param device Nonempty owner of the logical device.
 * @param queue_family_index Requested queue family.
 * @param queue_index Index within the family's requested queues.
 * @return Reusable ReaderIO producing a borrowed queue handle.
 */
template <vk_get_device_queue_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_device_queue(
    device_ptr device, queue_family_index queue_family_index, queue_index queue_index)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing queue-retrieval IO from an execution environment.
    auto io_get_device_queue = std::bind_front(
        detail::io_get_device_queue<TEnvironment>,
        std::move(device),
        queue_family_index,
        queue_index);

    // ReaderIO providing a borrowed queue while its plan retains the device.
    return reader_io::create<"vk_get_device_queue"_ss, TEnvironment>(
        std::move(io_get_device_queue));
}
}  // namespace vulkandemo::vulkan
