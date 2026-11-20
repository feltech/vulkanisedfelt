// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/surface_present_modes.hpp
 *
 * Presentation surface present-mode query implementation.
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
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to inspect surface present-modes. */
template <class TEnvironment>
concept vk_get_physical_device_surface_present_modes_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment,
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::uint32_t * count,
        VkPresentModeKHR * modes)
{
    {environment.api.get().vk_get_physical_device_surface_present_modes_khr(
         device, surface, count, modes)}
        ->std::same_as<VkResult>;
};

/**
 * Retrieves a complete surface present-mode snapshot.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param physical_device Physical device supporting the surface.
 * @param surface Retained presentation surface.
 * @return Owned surface present-modes or the original query failure.
 */
template <class TApi>
auto get_physical_device_surface_present_modes(
    std::reference_wrapper<TApi> api, VkPhysicalDevice physical_device, surface_ptr surface)
    -> vulkan_result<immer::array<VkPresentModeKHR>> pre(
        physical_device != VK_NULL_HANDLE && surface != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<VkPresentModeKHR>>;
    constexpr int max_attempts = 3;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        std::uint32_t count = 0;
        VkResult status = api.get().vk_get_physical_device_surface_present_modes_khr(
            physical_device, surface.get(), &count, nullptr);
        if (status == VK_INCOMPLETE)
        {
            continue;
        }
        if (status != VK_SUCCESS)
        {
            return result::error<result_type>(VulkanFailure{
                FallibleVulkanOperation::get_physical_device_surface_present_modes, status});
        }
        if (count == 0)
        {
            return result::success<result_type>(immer::array<VkPresentModeKHR>{});
        }

        // Writable storage for this attempt's complete snapshot.
        auto modes = immer::array<VkPresentModeKHR>(count).transient();
        std::uint32_t const capacity = count;
        status = api.get().vk_get_physical_device_surface_present_modes_khr(
            physical_device, surface.get(), &count, modes.data_mut());
        if (status != VK_SUCCESS && status != VK_INCOMPLETE)
        {
            return result::error<result_type>(VulkanFailure{
                FallibleVulkanOperation::get_physical_device_surface_present_modes, status});
        }
        // Only complete snapshots within the allocated capacity are usable.
        if (status == VK_INCOMPLETE || count > capacity)
        {
            continue;
        }
        modes.take(count);
        return result::success<result_type>(std::move(modes).persistent());
    }
    return result::error<result_type>(VulkanFailure{
        FallibleVulkanOperation::get_physical_device_surface_present_modes, VK_INCOMPLETE});
}

/**
 * Provides deferred surface present-mode retrieval from execution capabilities.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Borrowed physical device.
 * @param surface Retained presentation surface.
 * @param environment Execution capabilities.
 * @return Deferred surface present-mode query.
 */
template <class TEnvironment>
constexpr auto io_get_physical_device_surface_present_modes(
    VkPhysicalDevice physical_device, surface_ptr surface, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing an owned snapshot while retaining the surface and its parents.
    auto query_surface_present_modes = std::bind_front(
        get_physical_device_surface_present_modes<api_type>,
        environment.api,
        physical_device,
        std::move(surface));

    // IO providing a complete snapshot or the original Vulkan failure.
    return io::create<"vk_get_physical_device_surface_present_modes_khr"_ss>(
        std::move(query_surface_present_modes));
}
}  // namespace vulkandemo::vulkan::detail
