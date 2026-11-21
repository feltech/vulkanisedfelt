// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_maintenance1_features.hpp
 *
 * Deferred discovery of the swapchain-maintenance1 feature.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/swapchain_maintenance1_features.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a query of the shared KHR/EXT swapchain-maintenance1 feature bit.
 *
 * The device must advertise a swapchain-maintenance1 extension. Its parent instance must
 * support Vulkan 1.1 and, together with the borrowed API, outlive execution.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param physical_device Borrowed physical device.
 * @return Reusable ReaderIO providing an independently owned support flag.
 */
template <detail::query_swapchain_maintenance1_feature_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_query_swapchain_maintenance1_feature(
    VkPhysicalDevice physical_device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing feature-query IO from the borrowed command implementation.
    auto query = std::bind_front(
        detail::query_swapchain_maintenance1_feature_reader_io_action<TEnvironment>,
        physical_device);

    // ReaderIO providing the shared maintenance1 feature without exposing a native pointer chain.
    return reader_io::create<"query_swapchain_maintenance1_feature"_ss, TEnvironment>(
        std::move(query));
}
}  // namespace vulkandemo::vulkan
