// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/surface_capabilities.hpp
 *
 * Deferred presentation surface capability queries.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/surface_capabilities.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a capability query while retaining its surface.
 *
 * The physical device must support presentation to this surface and belong to its parent
 * instance. The borrowed API must outlive execution and surface cleanup.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param surface Nonempty owning presentation surface.
 * @return Reusable ReaderIO providing capabilities or the original Vulkan failure.
 */
template <detail::vk_get_physical_device_surface_capabilities_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_surface_capabilities_khr(
    VkPhysicalDevice physical_device, surface_ptr surface)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing a capability query with the surface and its parents retained.
    auto io_get_surface_capabilities = std::bind_front(
        detail::io_get_physical_device_surface_capabilities<TEnvironment>,
        physical_device,
        std::move(surface));

    // ReaderIO providing surface capabilities or the original query failure.
    return reader_io::create<"vk_get_physical_device_surface_capabilities_khr"_ss, TEnvironment>(
        std::move(io_get_surface_capabilities));
}
}  // namespace vulkandemo::vulkan
