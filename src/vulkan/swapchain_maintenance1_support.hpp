// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_maintenance1_support.hpp
 *
 * Deferred discovery of a device's compatible maintenance1 extension and feature.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./device_extensions.hpp"
#include "./presentation_instance_types.hpp"
#include "detail/swapchain_maintenance1_support.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans maintenance1 eligibility discovery for a physical device and its parent instance.
 *
 * The physical device must belong to the supplied instance. The executed IO retains that
 * instance; the borrowed API must outlive execution. Discovery does not enable a device feature
 * or extension. Missing support is successful absence, not a native failure or cancellation.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param instance Owned parent instance and its enabled extension names.
 * @param physical_device Borrowed physical device.
 * @return ReaderIO providing an optional supported extension name, stop, or native query error.
 */
template <detail::discover_swapchain_maintenance1_support_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_discover_swapchain_maintenance1_support(
    PresentationInstance instance, VkPhysicalDevice physical_device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    // GCC does not support preconditions on this deduced-return factory.
    contract_assert(instance.instance != nullptr && physical_device != VK_NULL_HANDLE);

    // ReaderIO providing advertised device extensions or their original native failure.
    auto extensions =
        reader_io_vk_enumerate_device_extension_properties<TEnvironment>(physical_device);

    // Continuation providing the owned feature query only after successful enumeration.
    auto discover_feature = std::bind_front(
        detail::reader_io_discover_swapchain_maintenance1_feature_support<TEnvironment>,
        std::move(instance),
        physical_device);

    // ReaderIO providing a complete supported path or ordinary absence of device eligibility.
    return reader_io::and_then<"discover_device_swapchain_maintenance1"_ss>(
        std::move(extensions), std::move(discover_feature));
}
}  // namespace vulkandemo::vulkan
