// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/queue_family_properties.hpp
 *
 * Deferred Vulkan queue family property queries.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/queue_family_properties.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans one count/fill queue family query.
 *
 * The parent instance and borrowed API must outlive execution.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @return Reusable ReaderIO producing an immutable written snapshot.
 */
template <vk_get_physical_device_queue_family_properties_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_queue_family_properties(
    VkPhysicalDevice physical_device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing queue-family IO from an execution environment.
    auto io_get_physical_device_queue_family_properties = std::bind_front(
        detail::io_get_physical_device_queue_family_properties<TEnvironment>, physical_device);

    // ReaderIO producing an immutable queue family property snapshot.
    return reader_io::create<"vk_get_physical_device_queue_family_properties"_ss, TEnvironment>(
        std::move(io_get_physical_device_queue_family_properties));
}
}  // namespace vulkandemo::vulkan
