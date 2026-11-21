// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/image_acquisition.hpp
 *
 * Native acquisition of a presentable image.
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
#include "../image_acquisition_types.hpp"
#include "../managed_handles_types.hpp"
#include "../synchronization_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to acquire a swapchain image. */
template <class TEnvironment>
concept vk_acquire_next_image_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkSwapchainKHR swapchain,
             std::uint64_t timeout,
             VkSemaphore semaphore,
             VkFence fence,
             std::uint32_t * image_index)
{
    {environment.api.get().vk_acquire_next_image_khr(
         device, swapchain, timeout, semaphore, fence, image_index)}
        ->std::same_as<VkResult>;
};

/**
 * Executes acquisition as an IO action retaining its image and signal owners.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param swapchain Retained image owner.
 * @param timeout Maximum wait in nanoseconds.
 * @param semaphore Optional retained acquisition semaphore.
 * @param fence Optional retained acquisition fence.
 * @return Acquired image or the original non-acquisition Vulkan status.
 */
template <class TApi>
auto acquire_next_image_io_action(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    swapchain_ptr swapchain,
    timeout_nanoseconds timeout,
    semaphore_ptr semaphore,
    fence_ptr fence)
    -> vulkan_result<AcquiredSwapchainImage> pre(device != nullptr && swapchain != nullptr)
        pre(semaphore != nullptr || fence != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<AcquiredSwapchainImage>;
    std::uint32_t image_index = 0;
    VkResult status = api.get().vk_acquire_next_image_khr(
        device.get(),
        swapchain.get(),
        value_of(timeout),
        semaphore.get(),
        fence.get(),
        &image_index);
    if (status == VK_SUCCESS || status == VK_SUBOPTIMAL_KHR)
    {
        return result::success<result_type>(AcquiredSwapchainImage{
            std::move(swapchain),
            std::move(semaphore),
            std::move(fence),
            swapchain_image_index{image_index},
            status == VK_SUBOPTIMAL_KHR});
    }
    return result::error<result_type>(
        VulkanFailure{FallibleVulkanOperation::acquire_next_image, status});
}

/**
 * Provides image acquisition from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param swapchain Retained image owner.
 * @param timeout Maximum wait in nanoseconds.
 * @param semaphore Optional retained binary semaphore.
 * @param fence Optional retained fence.
 * @param environment Borrowed command capabilities.
 * @return Deferred image acquisition.
 */
template <class TEnvironment>
constexpr auto acquire_next_image_reader_io_action(
    device_ptr device,
    swapchain_ptr swapchain,
    timeout_nanoseconds timeout,
    semaphore_ptr semaphore,
    fence_ptr fence,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing an image index with its swapchain and signal owners retained.
    auto acquire = std::bind_front(
        acquire_next_image_io_action<api_type>,
        environment.api,
        std::move(device),
        std::move(swapchain),
        timeout,
        std::move(semaphore),
        std::move(fence));

    // IO providing an acquired image or a native status for recovery by the frame layer.
    return io::create<"vk_acquire_next_image_khr"_ss>(std::move(acquire));
}
}  // namespace vulkandemo::vulkan::detail
