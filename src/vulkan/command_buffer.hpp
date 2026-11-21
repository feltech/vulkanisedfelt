// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/command_buffer.hpp
 *
 * Deferred allocation of primary command buffers.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/command_buffer.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans allocation of one primary command buffer in its initial state.
 *
 * The nonempty pool must belong to the device. Keep the borrowed API alive through cleanup
 * and complete submitted work before final release. Externally synchronize all host access to
 * this pool and its buffers, including allocation, recording, freeing, reset and destruction.
 * Operations and final releases sharing a pool must not run concurrently.
 *
 * @param device Retained logical device.
 * @param command_pool Retained pool owning the buffer.
 * @return Reusable ReaderIO providing an owned primary buffer or the original Vulkan failure.
 */
template <detail::vk_allocate_command_buffer_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_allocate_command_buffer(
    device_ptr device, command_pool_ptr command_pool)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing primary-buffer allocation with its pool and device retained.
    auto allocate_command_buffer = std::bind_front(
        detail::allocate_command_buffer_reader_io_action<TEnvironment>,
        std::move(device),
        std::move(command_pool));

    // ReaderIO providing a single initial-state primary command buffer.
    return reader_io::create<"vk_allocate_command_buffer"_ss, TEnvironment>(
        std::move(allocate_command_buffer));
}
}  // namespace vulkandemo::vulkan
