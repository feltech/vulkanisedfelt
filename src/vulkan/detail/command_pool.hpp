// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/command_pool.hpp
 *
 * Creation of owned command pools.
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
#include "../managed_handles.hpp"
#include "../managed_handles_types.hpp"
#include "../queue_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to create and release a command pool. */
template <class TEnvironment>
concept vk_create_command_pool_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkCommandPoolCreateInfo const * info,
             VkCommandPool * command_pool,
             VkCommandPool handle)
{
    {environment.api.get().vk_create_command_pool(device, info, nullptr, command_pool)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_command_pool(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates a command pool with a native Vulkan outcome.
 *
 * @param api Borrowed command implementation.
 * @param device Borrowed logical device.
 * @param queue_family Requested queue family.
 * @param command_pool Native output handle.
 * @return Native creation status.
 */
template <class TApi>
auto create_command_pool(
    std::reference_wrapper<TApi> api,
    VkDevice device,
    queue_family_index queue_family,
    VkCommandPool * command_pool) -> VkResult
    pre(device != VK_NULL_HANDLE && command_pool != nullptr)
{
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = value_of(queue_family);
    return api.get().vk_create_command_pool(device, &info, nullptr, command_pool);
}

/**
 * Executes command pool creation as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param queue_family Requested queue family.
 * @return Owned pool or the original creation failure.
 */
template <class TApi>
auto create_command_pool_io_action(
    std::reference_wrapper<TApi> api, device_ptr device, queue_family_index queue_family)
    -> vulkan_result<command_pool_ptr> pre(device != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<command_pool_ptr>;
    VkCommandPool handle = VK_NULL_HANDLE;
    VkResult status = create_command_pool(api, device.get(), queue_family, &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_command_pool, status});
    }
    return result::success<result_type>(
        make_command_pool_ptr(handle, std::move(device), api.get()));
}

/**
 * Provides command pool creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param queue_family Requested queue family.
 * @param environment Borrowed command capabilities.
 * @return Deferred command pool creation.
 */
template <class TEnvironment>
constexpr auto create_command_pool_reader_io_action(
    device_ptr device, queue_family_index queue_family, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing one pool with its logical device retained.
    auto create_pool = std::bind_front(
        create_command_pool_io_action<api_type>, environment.api, std::move(device), queue_family);

    // IO providing pool ownership or the original Vulkan failure.
    return io::create<"vk_create_command_pool"_ss>(std::move(create_pool));
}
}  // namespace vulkandemo::vulkan::detail
