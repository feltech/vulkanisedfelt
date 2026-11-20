// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_support.hpp
 *
 * Deferred swapchain support discovery.
 */
#pragma once

#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./surface_capabilities.hpp"
#include "./surface_formats.hpp"
#include "./surface_present_modes.hpp"
#include "./swapchain_support_types.hpp"
#include "detail/swapchain_support.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans independent capability, format and present-mode queries for one surface.
 *
 * The physical device must support presentation to this surface and belong to its parent
 * instance. The borrowed API must outlive execution and surface cleanup. Every query completes
 * before any stop takes precedence over errors. Otherwise the first error in capability, format
 * and present-mode order is returned.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param surface Nonempty owning presentation surface.
 * @return Reusable ReaderIO providing owned support facts, any stop, or the first query error.
 */
template <detail::query_swapchain_support_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_query_swapchain_support(
    VkPhysicalDevice physical_device, surface_ptr surface)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIOs providing independent snapshots while retaining their shared surface.
    auto capabilities = reader_io_vk_get_physical_device_surface_capabilities_khr<TEnvironment>(
        physical_device, surface);
    auto formats = reader_io_vk_get_physical_device_surface_formats_khr<TEnvironment>(
        physical_device, surface);
    auto present_modes = reader_io_vk_get_physical_device_surface_present_modes_khr<TEnvironment>(
        physical_device, std::move(surface));

    // ReaderIO providing all outcomes after the independent queries complete.
    auto outcomes = reader_io::sequence<"query_swapchain_support"_ss, TEnvironment>(
        std::move(capabilities), std::move(formats), std::move(present_modes));

    // ReaderIO providing successful snapshots, any stop, or the first query error.
    auto query_values = reader_io::transform<"sequence_swapchain_support_results"_ss>(
        std::move(outcomes), detail::sequence_swapchain_support_results);

    // ReaderIO providing support facts only from successful plain snapshots.
    return reader_io::transform<"make_swapchain_support"_ss>(
        std::move(query_values), detail::make_swapchain_support);
}
}  // namespace vulkandemo::vulkan
