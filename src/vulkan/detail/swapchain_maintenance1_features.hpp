// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_maintenance1_features.hpp
 *
 * Owned feature observations from the Vulkan maintenance1 query chain.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>

#include "../../backend/detail/api.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to query extended physical-device features. */
template <class TEnvironment>
concept query_swapchain_maintenance1_feature_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment const & environment,
        VkPhysicalDevice device,
        VkPhysicalDeviceFeatures2 * features)
{
    {environment.api.get().vk_get_physical_device_features2(device, features)}->std::same_as<void>;
};

/**
 * Reads the feature shared by the KHR and EXT maintenance1 extensions.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Borrowed physical device advertising maintenance1.
 * @return Whether swapchain maintenance1 is supported.
 */
template <class TApi>
constexpr auto query_swapchain_maintenance1_feature(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device) -> bool
{
    // EXT and KHR spellings alias the same feature; EXT also exists in initial Vulkan 1.4 headers.
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance1{};
    maintenance1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &maintenance1;
    api.get().vk_get_physical_device_features2(physical_device, &features);
    return maintenance1.swapchainMaintenance1 != VK_FALSE;
}

/**
 * Supplies the API to the deferred maintenance1 feature query.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param physical_device Borrowed physical device.
 * @param environment Borrowed command implementation.
 * @return IO providing the owned feature flag.
 */
template <class TEnvironment>
constexpr auto query_swapchain_maintenance1_feature_reader_io_action(
    VkPhysicalDevice physical_device, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing a value copied from the synchronous native output chain.
    auto query = std::bind_front(
        query_swapchain_maintenance1_feature<api_type>, environment.api, physical_device);

    // IO providing the maintenance1 feature without a fabricated native error channel.
    return io::create<"query_swapchain_maintenance1_feature"_ss>(std::move(query));
}
}  // namespace vulkandemo::vulkan::detail
