// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/fence.hpp
 *
 * Creation of owned unsignaled fences.
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
#include "../fence_types.hpp"
#include "../managed_handles.hpp"
#include "../managed_handles_types.hpp"
#include "../synchronization_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to wait for one fence. */
template <class TEnvironment>
concept vk_wait_for_fence_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkFence const * fence,
             std::uint64_t timeout)
{
    {environment.api.get().vk_wait_for_fences(device, 1, fence, VK_TRUE, timeout)}
        ->std::same_as<VkResult>;
};

/**
 * Waits for one fence and preserves its native status.
 *
 * @param api Borrowed command implementation.
 * @param device Logical device owning the fence.
 * @param fence Native fence to wait for.
 * @param timeout Maximum wait in nanoseconds.
 * @return The original Vulkan status.
 */
template <class TApi>
auto wait_for_fence(
    std::reference_wrapper<TApi> api, VkDevice device, VkFence fence, std::uint64_t timeout)
    -> VkResult pre(device != nullptr && fence != nullptr)
{
    return api.get().vk_wait_for_fences(device, 1, &fence, VK_TRUE, timeout);
}

/**
 * Interprets a fence wait without treating timeout as a Vulkan failure.
 *
 * @param status Native fence-wait status.
 * @return Signaled, timed out, or the original Vulkan failure.
 */
constexpr auto fence_wait_outcome(VkResult status) -> vulkan_result<FenceWaitStatus>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<FenceWaitStatus>;
    if (status == VK_SUCCESS)
    {
        return result::success<result_type>(FenceWaitStatus::signaled);
    }
    if (status == VK_TIMEOUT)
    {
        return result::success<result_type>(FenceWaitStatus::timed_out);
    }
    return result::error<result_type>(
        VulkanFailure{FallibleVulkanOperation::wait_for_fence, status});
}

/**
 * Executes a fence-wait IO action with retained owners.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param fence Retained fence.
 * @param timeout Maximum wait in nanoseconds.
 * @return Signaled, timed out, or the original Vulkan failure.
 */
template <class TApi>
auto wait_for_fence_io_action(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    fence_ptr fence,
    timeout_nanoseconds timeout) -> vulkan_result<FenceWaitStatus>
{
    VkResult status = wait_for_fence(api, device.get(), fence.get(), value_of(timeout));
    return fence_wait_outcome(status);
}

/**
 * Provides a fence wait from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param fence Retained fence.
 * @param timeout Maximum wait in nanoseconds.
 * @param environment Borrowed command capabilities.
 * @return Deferred fence wait.
 */
template <class TEnvironment>
constexpr auto wait_for_fence_reader_io_action(
    device_ptr device,
    fence_ptr fence,
    timeout_nanoseconds timeout,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing one fence's completion status while retaining its parents.
    auto wait = std::bind_front(
        wait_for_fence_io_action<api_type>,
        environment.api,
        std::move(device),
        std::move(fence),
        timeout);

    // IO providing completion, timeout or the original Vulkan failure.
    return io::create<"vk_wait_for_fence"_ss>(std::move(wait));
}

/** Commands needed to create and release a fence. */
template <class TEnvironment>
concept vk_create_fence_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkFenceCreateInfo const * info,
             VkFence * fence,
             VkFence handle)
{
    {environment.api.get().vk_create_fence(device, info, nullptr, fence)}->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_fence(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates an unsignaled fence with its native Vulkan status.
 *
 * @param api Borrowed command implementation.
 * @param device Borrowed logical device.
 * @param fence Native output handle.
 * @return The original creation status.
 */
template <class TApi>
auto create_fence(std::reference_wrapper<TApi> api, VkDevice device, VkFence * fence) -> VkResult
    pre(device != nullptr && fence != nullptr)
{
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    return api.get().vk_create_fence(device, &info, nullptr, fence);
}

/**
 * Executes fence creation as an IO action retaining its device.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @return Owned fence or the original creation failure.
 */
template <class TApi>
auto create_fence_io_action(std::reference_wrapper<TApi> api, device_ptr device)
    -> vulkan_result<fence_ptr> pre(device != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<fence_ptr>;
    VkFence handle = VK_NULL_HANDLE;
    VkResult status = create_fence(api, device.get(), &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_fence, status});
    }
    return result::success<result_type>(make_fence_ptr(handle, std::move(device), api.get()));
}

/**
 * Provides fence creation from a borrowed command implementation.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @return Deferred fence creation.
 */
template <class TApi>
constexpr auto io_create_fence(std::reference_wrapper<TApi> api, device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing an owned unsignaled fence with its device retained.
    auto create_fence_with_device =
        std::bind_front(create_fence_io_action<TApi>, api, std::move(device));

    // IO providing fence ownership or the original Vulkan failure.
    return io::create<"vk_create_fence"_ss>(std::move(create_fence_with_device));
}

/**
 * Provides fence creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param environment Borrowed command capabilities.
 * @return Deferred fence creation.
 */
template <class TEnvironment>
constexpr auto create_fence_reader_io_action(device_ptr device, TEnvironment const & environment)
{
    return io_create_fence(environment.api, std::move(device));
}
}  // namespace vulkandemo::vulkan::detail
