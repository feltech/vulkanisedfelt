// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/fence.hpp
 *
 * Deferred creation of owned unsignaled fences.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/fence.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of an owned unsignaled fence.
 *
 * The fence begins unsignaled, ready for its first queue submission. Keep the borrowed API alive
 * through cleanup, complete queue submissions using the fence, and externally synchronize host
 * access before final release.
 *
 * @param device Retained logical device.
 * @return Reusable ReaderIO providing ownership or the original Vulkan failure.
 */
template <detail::vk_create_fence_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_fence(device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing fence creation with the logical device retained.
    auto create_fence =
        std::bind_front(detail::create_fence_reader_io_action<TEnvironment>, std::move(device));

    // ReaderIO providing an owned unsignaled fence.
    return reader_io::create<"vk_create_fence"_ss, TEnvironment>(std::move(create_fence));
}
}  // namespace vulkandemo::vulkan
