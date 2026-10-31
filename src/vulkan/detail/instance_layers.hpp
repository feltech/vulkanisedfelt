// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/instance_layers.hpp
 *
 * Implementation of Vulkan instance layers operations.
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
/** Commands needed to enumerate instance layers. */
template <class TEnvironment>
concept vk_enumerate_instance_layer_properties_environment =
    backend::api_environment<TEnvironment> &&
    requires(TEnvironment environment, std::uint32_t * count, VkLayerProperties * properties)
{
    {environment.api.get().vk_enumerate_instance_layer_properties(count, properties)}
        ->std::same_as<VkResult>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves a complete instance layer snapshot.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @return An owned instance layer snapshot or query failure.
 */
template <class TApi>
auto enumerate_instance_layer_properties(std::reference_wrapper<TApi> api)
    -> vulkan_result<immer::array<VkLayerProperties>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<VkLayerProperties>>;
    constexpr int max_attempts = 3;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        std::uint32_t count = 0;
        auto status = api.get().vk_enumerate_instance_layer_properties(&count, nullptr);
        if (status == VK_INCOMPLETE)
        {
            continue;
        }
        if (status != VK_SUCCESS)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_instance_layers, status});
        }
        if (count == 0)
        {
            return result::success<result_type>(immer::array<VkLayerProperties>{});
        }

        // Writable storage for this attempt's property snapshot.
        auto properties = immer::array<VkLayerProperties>(count).transient();
        auto const capacity = count;
        status = api.get().vk_enumerate_instance_layer_properties(&count, properties.data_mut());
        if (status != VK_SUCCESS && status != VK_INCOMPLETE)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::enumerate_instance_layers, status});
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
        VulkanFailure{FallibleVulkanOperation::enumerate_instance_layers, VK_INCOMPLETE});
}

/**
 * Supplies the API to deferred instance-layer retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param environment Execution capabilities.
 * @return Deferred instance-layer operation.
 */
template <class TEnvironment>
constexpr auto io_enumerate_instance_layer_properties(TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing an owned instance layer snapshot or query failure.
    auto enumerate_layers =
        std::bind_front(enumerate_instance_layer_properties<api_type>, environment.api);

    // IO producing an owned instance layer snapshot or query failure.
    return io::create<"vk_enumerate_instance_layer_properties"_ss>(std::move(enumerate_layers));
}
}  // namespace vulkandemo::vulkan::detail
