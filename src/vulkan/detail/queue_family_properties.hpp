// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/queue_family_properties.hpp
 *
 * Implementation of Vulkan queue family properties operations.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../backend/detail/api.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to inspect queue families. */
template <class TEnvironment>
concept vk_get_physical_device_queue_family_properties_environment =
    backend::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment,
        VkPhysicalDevice device,
        std::uint32_t * count,
        VkQueueFamilyProperties * properties)
{
    {environment.api.get().vk_get_physical_device_queue_family_properties(
         device, count, properties)}
        ->std::same_as<void>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves an owned queue family property snapshot.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Borrowed physical device.
 * @return An immutable queue family property snapshot.
 */
template <class TApi>
auto get_physical_device_queue_family_properties(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device)
    -> immer::array<VkQueueFamilyProperties>
{
    std::uint32_t count = 0;
    api.get().vk_get_physical_device_queue_family_properties(physical_device, &count, nullptr);
    if (count == 0)
    {
        return {};
    }

    auto properties = immer::array<VkQueueFamilyProperties>(count).transient();
    auto const capacity = count;
    api.get().vk_get_physical_device_queue_family_properties(
        physical_device, &count, properties.data_mut());
    // Only the reported prefix is a valid snapshot; the void API has no incomplete status.
    contract_assert(count <= capacity);
    properties.take(count);
    return std::move(properties).persistent();
}

/**
 * Supplies the API to deferred queue-family retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param environment Execution capabilities.
 * @return Deferred queue-family operation.
 */
template <class TEnvironment>
constexpr auto io_get_physical_device_queue_family_properties(
    VkPhysicalDevice physical_device, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing an immutable queue family property snapshot.
    auto query_queue_families = std::bind_front(
        get_physical_device_queue_family_properties<api_type>, environment.api, physical_device);

    // IO producing an immutable queue family property snapshot.
    return io::create<"vk_get_physical_device_queue_family_properties"_ss>(
        std::move(query_queue_families));
}
}  // namespace vulkandemo::vulkan::detail
