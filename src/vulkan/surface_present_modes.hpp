// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/surface_present_modes.hpp
 *
 * Deferred presentation surface present-mode queries.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/surface_present_modes.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a surface present-mode query while retaining its surface.
 *
 * The physical device must support presentation to this surface and belong to its parent
 * instance. The borrowed API must outlive execution and surface cleanup.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param surface Nonempty owning presentation surface.
 * @return Reusable ReaderIO providing an owned snapshot or the original Vulkan failure.
 */
template <detail::vk_get_physical_device_surface_present_modes_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_surface_present_modes_khr(
    VkPhysicalDevice physical_device, surface_ptr surface)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing a snapshot query with the surface and its parents retained.
    auto get_surface_present_modes = std::bind_front(
        detail::get_physical_device_surface_present_modes_reader_io_action<TEnvironment>,
        physical_device,
        std::move(surface));

    // ReaderIO providing a complete snapshot or the original query failure.
    return reader_io::create<"vk_get_physical_device_surface_present_modes_khr"_ss, TEnvironment>(
        std::move(get_surface_present_modes));
}
}  // namespace vulkandemo::vulkan
