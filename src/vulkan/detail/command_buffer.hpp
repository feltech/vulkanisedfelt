// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/command_buffer.hpp
 *
 * Allocation of owned primary command buffers.
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
#include "../managed_handles.hpp"
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to allocate and release a command buffer. */
template <class TEnvironment>
concept vk_allocate_command_buffer_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkCommandPool command_pool,
             VkCommandBufferAllocateInfo const * info,
             VkCommandBuffer * command_buffers,
             VkCommandBuffer const * handles,
             std::uint32_t count)
{
    {environment.api.get().vk_allocate_command_buffers(device, info, command_buffers)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_free_command_buffers(device, command_pool, count, handles)}
        ->std::same_as<void>;
};

/**
 * Allocates a primary command buffer with a native Vulkan outcome.
 *
 * @param api Borrowed command implementation.
 * @param device Borrowed logical device.
 * @param command_pool Borrowed allocation pool.
 * @param command_buffer Native output handle.
 * @return Native allocation status.
 */
template <class TApi>
auto allocate_command_buffer(
    std::reference_wrapper<TApi> api,
    VkDevice device,
    VkCommandPool command_pool,
    VkCommandBuffer * command_buffer) -> VkResult
    pre(device != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE && command_buffer != nullptr)
{
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    return api.get().vk_allocate_command_buffers(device, &info, command_buffer);
}

/**
 * Executes primary-buffer allocation as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param command_pool Pool owning the allocation.
 * @return Owned buffer or the original allocation failure.
 */
template <class TApi>
auto allocate_command_buffer_io_action(
    std::reference_wrapper<TApi> api, device_ptr device, command_pool_ptr command_pool)
    -> vulkan_result<command_buffer_ptr> pre(device != nullptr && command_pool != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<command_buffer_ptr>;
    VkCommandBuffer handle = VK_NULL_HANDLE;
    VkResult status = allocate_command_buffer(api, device.get(), command_pool.get(), &handle);
    if (status != VK_SUCCESS)
    {
        // Vulkan frees any allocations from this failed call; no owner is published.
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::allocate_command_buffer, status});
    }
    return result::success<result_type>(
        make_command_buffer_ptr(handle, std::move(device), std::move(command_pool), api.get()));
}

/**
 * Provides primary-buffer allocation from a borrowed command implementation.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param command_pool Pool owning the allocation.
 * @return Deferred primary-buffer allocation.
 */
template <class TApi>
constexpr auto io_allocate_command_buffer(
    std::reference_wrapper<TApi> api, device_ptr device, command_pool_ptr command_pool)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing one primary buffer with its native parents retained.
    auto allocate_buffer = std::bind_front(
        allocate_command_buffer_io_action<TApi>, api, std::move(device), std::move(command_pool));

    // IO providing buffer ownership or the original Vulkan failure.
    return io::create<"vk_allocate_command_buffer"_ss>(std::move(allocate_buffer));
}

/**
 * Provides primary-buffer allocation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param command_pool Pool owning the allocation.
 * @param environment Borrowed command capabilities.
 * @return Deferred primary-buffer allocation.
 */
template <class TEnvironment>
constexpr auto allocate_command_buffer_reader_io_action(
    device_ptr device, command_pool_ptr command_pool, TEnvironment const & environment)
{
    return io_allocate_command_buffer(environment.api, std::move(device), std::move(command_pool));
}
}  // namespace vulkandemo::vulkan::detail
