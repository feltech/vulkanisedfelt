// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/device_extensions.hpp
 *
 * Implementation of Vulkan device extensions operations.
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
/** Commands needed to enumerate device extensions. */
template <class TEnvironment>
concept vk_enumerate_device_extension_properties_environment =
    backend::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment,
        VkPhysicalDevice device,
        char const * layer,
        std::uint32_t * count,
        VkExtensionProperties * properties)
{
    {environment.api.get().vk_enumerate_device_extension_properties(
         device, layer, count, properties)}
        ->std::same_as<VkResult>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves a complete device extension snapshot.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Borrowed physical device.
 * @return An owned device extension snapshot or query failure.
 */
template <class TApi>
auto enumerate_device_extension_properties(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device)
    -> vulkan_result<immer::array<VkExtensionProperties>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<VkExtensionProperties>>;
    constexpr int max_attempts = 3;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        std::uint32_t count = 0;
        auto status = api.get().vk_enumerate_device_extension_properties(
            physical_device, nullptr, &count, nullptr);
        if (status == VK_INCOMPLETE)
        {
            continue;
        }
        if (status != VK_SUCCESS)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_device_extensions, status});
        }
        if (count == 0)
        {
            return result::success<result_type>(immer::array<VkExtensionProperties>{});
        }

        // Writable storage for this attempt's property snapshot.
        auto properties = immer::array<VkExtensionProperties>(count).transient();
        auto const capacity = count;
        status = api.get().vk_enumerate_device_extension_properties(
            physical_device, nullptr, &count, properties.data_mut());
        if (status != VK_SUCCESS && status != VK_INCOMPLETE)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_device_extensions, status});
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
        VulkanFailure{FallibleVulkanOperation::enumerate_device_extensions, VK_INCOMPLETE});
}

/**
 * Supplies the API to deferred device-extension retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param environment Execution capabilities.
 * @return Deferred device-extension operation.
 */
template <class TEnvironment>
constexpr auto io_enumerate_device_extension_properties(
    VkPhysicalDevice physical_device, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing an owned device extension snapshot or query failure.
    auto enumerate_extensions = std::bind_front(
        enumerate_device_extension_properties<api_type>, environment.api, physical_device);

    // IO producing an owned device extension snapshot or query failure.
    return io::create<"vk_enumerate_device_extension_properties"_ss>(
        std::move(enumerate_extensions));
}
}  // namespace vulkandemo::vulkan::detail
