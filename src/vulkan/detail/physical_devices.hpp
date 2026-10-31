// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/physical_devices.hpp
 *
 * Implementation of Vulkan physical devices operations.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../backend/detail/api.hpp"
#include "../errors.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to enumerate physical devices. */
template <class TEnvironment>
concept vk_enumerate_physical_devices_environment = backend::api_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkInstance instance,
             std::uint32_t * count,
             VkPhysicalDevice * devices)
{
    {environment.api.get().vk_enumerate_physical_devices(instance, count, devices)}
        ->std::same_as<VkResult>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves a complete snapshot of instance physical devices.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param instance Borrowed instance to query.
 * @return Immutable snapshot of borrowed physical-device handles or query failure.
 */
template <class TApi>
auto enumerate_physical_devices(std::reference_wrapper<TApi> api, VkInstance instance)
    -> vulkan_result<immer::array<VkPhysicalDevice>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<VkPhysicalDevice>>;
    constexpr int max_attempts = 3;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        std::uint32_t count = 0;
        auto status = api.get().vk_enumerate_physical_devices(instance, &count, nullptr);
        if (status == VK_INCOMPLETE)
        {
            continue;
        }
        if (status != VK_SUCCESS)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_physical_devices, status});
        }
        if (count == 0)
        {
            return result::success<result_type>(immer::array<VkPhysicalDevice>{});
        }

        // Writable storage for this attempt's property snapshot.
        auto properties = immer::array<VkPhysicalDevice>(count).transient();
        auto const capacity = count;
        status = api.get().vk_enumerate_physical_devices(instance, &count, properties.data_mut());
        if (status != VK_SUCCESS && status != VK_INCOMPLETE)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_physical_devices, status});
        }
        // Only complete snapshots within the allocated capacity are usable.
        if (status == VK_INCOMPLETE || count > capacity)
        {
            continue;
        }
        properties.take(count);
        return result::success<result_type>(std::move(properties).persistent());
    }
    return result::error<result_type>(
        VulkanFailure{FallibleVulkanOperation::enumerate_physical_devices, VK_INCOMPLETE});
}

/**
 * Supplies the API to deferred physical-device retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Borrowed instance to query.
 * @param environment Execution capabilities.
 * @return Deferred physical-device operation.
 */
template <class TEnvironment>
constexpr auto io_enumerate_physical_devices(VkInstance instance, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing an immutable snapshot of borrowed physical-device handles or query
    // failure.
    auto enumerate_devices =
        std::bind_front(enumerate_physical_devices<api_type>, environment.api, instance);

    // IO producing an immutable snapshot of borrowed physical-device handles or query failure.
    return io::create<"vk_enumerate_physical_devices"_ss>(std::move(enumerate_devices));
}
}  // namespace vulkandemo::vulkan::detail
