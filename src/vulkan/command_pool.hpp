// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/command_pool.hpp
 *
 * Deferred creation of command pools.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./queue_types.hpp"
#include "detail/command_pool.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of a command pool for one requested queue family.
 *
 * The nonempty device must have requested a queue in the selected family. The pool uses no
 * optional flags; individual command-buffer reset is not enabled. Keep the borrowed API alive
 * through cleanup, complete submitted work before final release, and externally synchronize
 * all host access to the pool, including allocation, recording, freeing and destruction.
 *
 * @param device Retained logical device.
 * @param queue_family Queue family to which the pool's command buffers may be submitted.
 * @return Reusable ReaderIO providing an owned pool or the original Vulkan failure.
 */
template <detail::vk_create_command_pool_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_command_pool(
    device_ptr device, queue_family_index queue_family)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing command-pool creation with the device retained.
    auto create_command_pool = std::bind_front(
        detail::create_command_pool_reader_io_action<TEnvironment>,
        std::move(device),
        queue_family);

    // ReaderIO providing a command pool for the selected queue family.
    return reader_io::create<"vk_create_command_pool"_ss, TEnvironment>(
        std::move(create_command_pool));
}
}  // namespace vulkandemo::vulkan
