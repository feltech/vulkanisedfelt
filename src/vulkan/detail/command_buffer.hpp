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
/** Commands needed to finish recording a command buffer. */
template <class TEnvironment>
concept vk_end_command_buffer_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment, VkCommandBuffer buffer)
{
    {environment.api.get().vk_end_command_buffer(buffer)}->std::same_as<VkResult>;
};

/**
 * Executes recording completion as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param command_buffer Recording primary buffer outside a render pass.
 * @return Executable buffer or the original Vulkan failure.
 */
template <class TApi>
auto end_command_buffer_io_action(
    std::reference_wrapper<TApi> api, command_buffer_ptr command_buffer)
    -> vulkan_result<command_buffer_ptr> pre(command_buffer != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<command_buffer_ptr>;
    VkResult status = api.get().vk_end_command_buffer(command_buffer.get());
    if (status != VK_SUCCESS)
    {
        // A failed native end leaves the buffer invalid rather than executable.
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::end_command_buffer, status});
    }
    return result::success<result_type>(std::move(command_buffer));
}

/**
 * Provides recording completion from borrowed execution capabilities.
 *
 * @param command_buffer Recording primary buffer outside a render pass.
 * @param environment Borrowed command capabilities.
 * @return Deferred recording completion.
 */
template <class TEnvironment>
constexpr auto end_command_buffer_reader_io_action(
    command_buffer_ptr command_buffer, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing recording completion with the buffer and its parents retained.
    auto end_recording = std::bind_front(
        end_command_buffer_io_action<api_type>, environment.api, std::move(command_buffer));

    // IO providing an executable buffer or the original Vulkan failure.
    return io::create<"vk_end_command_buffer"_ss>(std::move(end_recording));
}

/** Commands needed to begin recording a command buffer. */
template <class TEnvironment>
concept vk_begin_command_buffer_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkCommandBuffer buffer,
             VkCommandBufferBeginInfo const * info)
{
    {environment.api.get().vk_begin_command_buffer(buffer, info)}->std::same_as<VkResult>;
};

/**
 * Begins one-time command recording with a native Vulkan outcome.
 *
 * @param api Borrowed command implementation.
 * @param command_buffer Borrowed initial-state primary buffer.
 * @return Native recording-start status.
 */
template <class TApi>
auto begin_command_buffer(std::reference_wrapper<TApi> api, VkCommandBuffer command_buffer)
    -> VkResult pre(command_buffer != VK_NULL_HANDLE)
{
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return api.get().vk_begin_command_buffer(command_buffer, &info);
}

/**
 * Executes recording start as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param command_buffer Initial-state primary buffer.
 * @return Recording buffer or the original Vulkan failure.
 */
template <class TApi>
auto begin_command_buffer_io_action(
    std::reference_wrapper<TApi> api, command_buffer_ptr command_buffer)
    -> vulkan_result<command_buffer_ptr> pre(command_buffer != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<command_buffer_ptr>;
    VkResult status = begin_command_buffer(api, command_buffer.get());
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::begin_command_buffer, status});
    }
    return result::success<result_type>(std::move(command_buffer));
}

/**
 * Provides recording start from borrowed execution capabilities.
 *
 * @param command_buffer Initial-state primary buffer.
 * @param environment Borrowed command capabilities.
 * @return Deferred recording start.
 */
template <class TEnvironment>
constexpr auto begin_command_buffer_reader_io_action(
    command_buffer_ptr command_buffer, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing recording start with the buffer and its parents retained.
    auto begin_recording = std::bind_front(
        begin_command_buffer_io_action<api_type>, environment.api, std::move(command_buffer));

    // IO providing a recording buffer or the original Vulkan failure.
    return io::create<"vk_begin_command_buffer"_ss>(std::move(begin_recording));
}

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
