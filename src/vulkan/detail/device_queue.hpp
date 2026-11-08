// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/device_queue.hpp
 *
 * Implementation of deferred device queue retrieval.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>

#include "../../backend/detail/api.hpp"
#include "../managed_handles.hpp"
#include "../strong_types.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to retrieve a device queue. */
template <class TEnvironment>
concept vk_get_device_queue_environment = backend::api_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkDevice device,
             std::uint32_t family,
             std::uint32_t index,
             VkQueue * queue)
{
    {environment.api.get().vk_get_device_queue(device, family, index, queue)}->std::same_as<void>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retrieves a queue belonging to a retained device.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param device Retained queue owner.
 * @param queue_family_index Requested queue family.
 * @param queue_index Requested queue index.
 * @return Borrowed queue handle.
 */
template <class TApi>
constexpr auto get_device_queue(
    std::reference_wrapper<TApi> api,
    device_ptr const & device,
    queue_family_index queue_family_index,
    queue_index queue_index) -> VkQueue
{
    VkQueue queue = VK_NULL_HANDLE;
    api.get().vk_get_device_queue(
        device.get(), value_of(queue_family_index), value_of(queue_index), &queue);
    return queue;
}

/**
 * Supplies the API to deferred queue retrieval.
 *
 * @tparam TEnvironment Execution environment type.
 * @param device Retained queue owner.
 * @param queue_family_index Requested queue family.
 * @param queue_index Requested queue index.
 * @param environment Execution capabilities.
 * @return Deferred queue query.
 */
template <class TEnvironment>
constexpr auto io_get_device_queue(
    device_ptr const & device,
    queue_family_index queue_family_index,
    queue_index queue_index,
    TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing a queue from the retained device and supplied API.
    auto query_queue = std::bind_front(
        get_device_queue<api_type>, environment.api, device, queue_family_index, queue_index);

    // IO providing the borrowed queue handle.
    return io::create<"vk_get_device_queue"_ss>(std::move(query_queue));
}
}  // namespace vulkandemo::vulkan::detail
