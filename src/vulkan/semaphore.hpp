// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/semaphore.hpp
 *
 * Deferred creation of owned binary semaphores.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/semaphore.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of an owned binary semaphore.
 *
 * Binary semaphores begin unsignaled. Keep the borrowed API alive through cleanup, complete
 * all submitted batches using the semaphore, and externally synchronize host access before
 * final release.
 *
 * @param device Retained logical device.
 * @return Reusable ReaderIO providing ownership or the original Vulkan failure.
 */
template <detail::vk_create_semaphore_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_semaphore(device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing semaphore creation with the logical device retained.
    auto create_semaphore =
        std::bind_front(detail::create_semaphore_reader_io_action<TEnvironment>, std::move(device));

    // ReaderIO providing an owned binary semaphore.
    return reader_io::create<"vk_create_semaphore"_ss, TEnvironment>(std::move(create_semaphore));
}
}  // namespace vulkandemo::vulkan
