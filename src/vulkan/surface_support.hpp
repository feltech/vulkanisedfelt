// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/surface_support.hpp
 *
 * Deferred queue-family presentation-support queries.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./queue_types.hpp"
#include "detail/surface_support.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a presentation-support query while retaining its surface.
 *
 * The physical device must belong to the surface's parent instance and the index must identify
 * one of its queue families. The borrowed API must outlive execution and surface cleanup.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param family_index Queue family to inspect.
 * @param surface Nonempty owning presentation surface.
 * @return Reusable ReaderIO producing support or a query failure.
 */
template <detail::vk_get_physical_device_surface_support_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_surface_support_khr(
    VkPhysicalDevice physical_device, queue_family_index family_index, surface_ptr surface)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing a presentation query with its surface lifetime retained.
    auto io_get_surface_support = std::bind_front(
        detail::io_get_physical_device_surface_support<TEnvironment>,
        physical_device,
        family_index,
        std::move(surface));

    // ReaderIO providing a support flag or the original Vulkan failure.
    return reader_io::create<"vk_get_physical_device_surface_support_khr"_ss, TEnvironment>(
        std::move(io_get_surface_support));
}
}  // namespace vulkandemo::vulkan
