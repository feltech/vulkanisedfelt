// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/physical_device_properties.hpp
 *
 * Implementation of Vulkan physical device properties operations.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>

#include "../../backend/detail/api.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to inspect physical-device properties. */
template <class TEnvironment>
concept vk_get_physical_device_properties_environment =
    backend::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment, VkPhysicalDevice device, VkPhysicalDeviceProperties * properties)
{
    {environment.api.get().vk_get_physical_device_properties(device, properties)}
        ->std::same_as<void>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves an owned physical device property snapshot.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Borrowed physical device.
 * @return Physical device properties.
 */
template <class TApi>
constexpr auto get_physical_device_properties(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device)
    -> VkPhysicalDeviceProperties
{
    VkPhysicalDeviceProperties properties{};
    api.get().vk_get_physical_device_properties(physical_device, &properties);
    return properties;
}

/**
 * Supplies the API to deferred property retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param environment Execution capabilities.
 * @return Deferred property query.
 */
template <class TEnvironment>
constexpr auto io_get_physical_device_properties(
    VkPhysicalDevice physical_device, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable to retrieve an owned snapshot of the device's properties.
    auto query_properties =
        std::bind_front(get_physical_device_properties<api_type>, environment.api, physical_device);

    // IO producing the property snapshot.
    return io::create<"vk_get_physical_device_properties"_ss>(std::move(query_properties));
}
}  // namespace vulkandemo::vulkan::detail
