// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_properties.hpp
 *
 * Deferred Vulkan physical device property queries.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/physical_device_properties.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a physical device property query.
 *
 * The parent instance and borrowed API must outlive execution.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @return Reusable ReaderIO producing an owned property snapshot.
 */
template <vk_get_physical_device_properties_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_properties(
    VkPhysicalDevice physical_device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing a property-query IO from an execution environment.
    auto io_get_physical_device_properties =
        std::bind_front(detail::io_get_physical_device_properties<TEnvironment>, physical_device);

    // ReaderIO producing an owned snapshot of the device's properties.
    return reader_io::create<"vk_get_physical_device_properties"_ss, TEnvironment>(
        std::move(io_get_physical_device_properties));
}
}  // namespace vulkandemo::vulkan
