// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/surface_capabilities.hpp
 *
 * Presentation surface capability query implementation.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../../backend/detail/api.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to inspect surface capabilities. */
template <class TEnvironment>
concept vk_get_physical_device_surface_capabilities_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment,
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        VkSurfaceCapabilitiesKHR * capabilities)
{
    {environment.api.get().vk_get_physical_device_surface_capabilities_khr(
         device, surface, capabilities)}
        ->std::same_as<VkResult>;
};

/**
 * Retrieves one presentation surface's capabilities.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Physical device supporting the surface.
 * @param surface Retained presentation surface.
 * @return Capabilities or the original query failure.
 */
template <class TApi>
auto get_physical_device_surface_capabilities(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device, surface_ptr surface)
    -> vulkan_result<VkSurfaceCapabilitiesKHR> pre(
        physical_device != VK_NULL_HANDLE && surface != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<VkSurfaceCapabilitiesKHR>;
    VkSurfaceCapabilitiesKHR capabilities{};
    VkResult status = api.get().vk_get_physical_device_surface_capabilities_khr(
        physical_device, surface.get(), &capabilities);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(VulkanFailure{
            FallibleVulkanOperation::get_physical_device_surface_capabilities, status});
    }
    return result::success<result_type>(capabilities);
}

/**
 * Provides a deferred capability query from execution capabilities.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param surface Retained presentation surface.
 * @param environment Execution capabilities.
 * @return Deferred capability query.
 */
template <class TEnvironment>
constexpr auto io_get_physical_device_surface_capabilities(
    VkPhysicalDevice physical_device, surface_ptr surface, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing capabilities while retaining the surface and its parents.
    auto query_surface_capabilities = std::bind_front(
        get_physical_device_surface_capabilities<api_type>,
        environment.api,
        physical_device,
        std::move(surface));

    // IO providing the capability snapshot or its original Vulkan failure.
    return io::create<"vk_get_physical_device_surface_capabilities_khr"_ss>(
        std::move(query_surface_capabilities));
}
}  // namespace vulkandemo::vulkan::detail
