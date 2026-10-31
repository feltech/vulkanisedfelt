// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_devices.hpp
 *
 * Deferred enumeration of borrowed physical devices belonging to a Vulkan instance.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/physical_devices.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans instance-scoped physical device enumeration with at most three count/fill attempts.
 *
 * Incomplete queries and oversized reported counts retry; exhaustion returns VK_INCOMPLETE.
 * The API borrowed through the environment's api reference_wrapper must outlive execution.
 * The instance must outlive execution and all uses of the returned borrowed handles.
 *
 * @param instance Borrowed instance to query.
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @return Reusable ReaderIO producing borrowed device handles or Vulkan error.
 */
template <vk_enumerate_physical_devices_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_enumerate_physical_devices(VkInstance instance)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing physical-device IO from an execution environment.
    auto io_enumerate_physical_devices =
        std::bind_front(detail::io_enumerate_physical_devices<TEnvironment>, instance);

    // ReaderIO producing an immutable snapshot of borrowed physical-device handles or query
    // failure.
    return reader_io::create<"vk_enumerate_physical_devices"_ss, TEnvironment>(
        std::move(io_enumerate_physical_devices));
}
}  // namespace vulkandemo::vulkan
