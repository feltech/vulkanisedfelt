// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_images.hpp
 *
 * Swapchain image enumeration implementation.
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
#include "../swapchain_images_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to retrieve swapchain images. */
template <class TEnvironment>
concept vk_get_swapchain_images_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkDevice device,
             VkSwapchainKHR swapchain,
             std::uint32_t * count,
             VkImage * images)
{
    {environment.api.get().vk_get_swapchain_images_khr(device, swapchain, count, images)}
        ->std::same_as<VkResult>;
};

/**
 * Retrieves a complete snapshot of swapchain-owned image handles.
 *
 * @param api Borrowed command implementation.
 * @param device Logical device owning the swapchain.
 * @param swapchain Retained image owner.
 * @return Borrowed images with ownership or the original query failure.
 */
template <class TApi>
auto get_swapchain_images(
    std::reference_wrapper<TApi> api, device_ptr device, swapchain_ptr swapchain)
    -> vulkan_result<SwapchainImages> pre(device != nullptr && swapchain != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<SwapchainImages>;
    constexpr int max_attempts = 3;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        std::uint32_t count = 0;
        VkResult status =
            api.get().vk_get_swapchain_images_khr(device.get(), swapchain.get(), &count, nullptr);
        if (status == VK_INCOMPLETE)
        {
            continue;
        }
        if (status != VK_SUCCESS)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::get_swapchain_images, status});
        }
        if (count == 0)
        {
            return result::success<result_type>(SwapchainImages{std::move(swapchain), {}});
        }

        // Writable storage for this attempt's complete image snapshot.
        auto images = immer::array<VkImage>(count).transient();
        std::uint32_t const capacity = count;
        status = api.get().vk_get_swapchain_images_khr(
            device.get(), swapchain.get(), &count, images.data_mut());
        if (status != VK_SUCCESS && status != VK_INCOMPLETE)
        {
            return result::error<result_type>(
                VulkanFailure{FallibleVulkanOperation::get_swapchain_images, status});
        }
        if (status == VK_INCOMPLETE || count > capacity)
        {
            continue;
        }
        images.take(count);
        return result::success<result_type>(
            SwapchainImages{std::move(swapchain), std::move(images).persistent()});
    }
    return result::error<result_type>(
        VulkanFailure{FallibleVulkanOperation::get_swapchain_images, VK_INCOMPLETE});
}

/**
 * Provides image retrieval from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param swapchain Retained image owner.
 * @param environment Borrowed command capabilities.
 * @return Deferred image retrieval.
 */
template <class TEnvironment>
constexpr auto io_get_swapchain_images(
    device_ptr device, swapchain_ptr swapchain, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing image handles while retaining their native parents.
    auto query_images = std::bind_front(
        get_swapchain_images<api_type>, environment.api, std::move(device), std::move(swapchain));

    // IO providing a complete image snapshot or the original Vulkan failure.
    return io::create<"vk_get_swapchain_images_khr"_ss>(std::move(query_images));
}
}  // namespace vulkandemo::vulkan::detail
