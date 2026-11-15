// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/surface_support.hpp
 *
 * Presentation-support query implementation.
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

#include "../../backend/detail/api.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../queue_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to inspect presentation support. */
template <class TEnvironment>
concept vk_get_physical_device_surface_support_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment const & environment,
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::uint32_t index,
        VkBool32 * supported)
{
    {environment.api.get().vk_get_physical_device_surface_support_khr(
         device, index, surface, supported)}
        ->std::same_as<VkResult>;
};

/**
 * Retrieves one queue family's presentation support.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Physical device from the surface's instance.
 * @param family_index Queue family to inspect.
 * @param surface Retained presentation surface.
 * @return Support flag or original query failure.
 */
template <class TApi>
auto get_physical_device_surface_support(
    std::reference_wrapper<TApi> api,
    VkPhysicalDevice physical_device,
    queue_family_index family_index,
    surface_ptr surface)
    -> vulkan_result<bool> pre(physical_device != VK_NULL_HANDLE && surface != nullptr)
{
    namespace result = fnfelt::result;
    VkBool32 is_supported = VK_FALSE;
    VkResult status = api.get().vk_get_physical_device_surface_support_khr(
        physical_device, value_of(family_index), surface.get(), &is_supported);
    if (status != VK_SUCCESS)
    {
        return result::error<vulkan_result<bool>>(
            VulkanFailure{FallibleVulkanOperation::get_physical_device_surface_support, status});
    }
    return result::success<vulkan_result<bool>>(is_supported != VK_FALSE);
}

/**
 * Provides a deferred query from execution capabilities.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param family_index Queue family to inspect.
 * @param surface Retained presentation surface.
 * @param environment Execution capabilities.
 * @return Deferred support query.
 */
template <class TEnvironment>
constexpr auto get_physical_device_surface_support_reader_io_action(
    VkPhysicalDevice physical_device,
    queue_family_index family_index,
    surface_ptr surface,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing support while keeping the surface and its parents alive.
    auto query_surface_support = std::bind_front(
        get_physical_device_surface_support<api_type>,
        environment.api,
        physical_device,
        family_index,
        std::move(surface));

    // IO providing the queue family's presentation-support outcome.
    return io::create<"vk_get_physical_device_surface_support_khr"_ss>(
        std::move(query_surface_support));
}
}  // namespace vulkandemo::vulkan::detail
