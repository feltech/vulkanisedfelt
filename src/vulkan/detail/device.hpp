// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/device.hpp
 *
 * Implementation of Vulkan device operations.
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
#include "../strong_types.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to acquire and release a logical device. */
template <class TEnvironment>
concept vk_create_device_environment = backend::api_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkPhysicalDevice physical_device,
             VkDeviceCreateInfo const * info,
             VkAllocationCallbacks const * allocator,
             VkDevice * device)
{
    {environment.api.get().vk_create_device(physical_device, info, allocator, device)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_device(*device, allocator)}->std::same_as<void>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Acquires a minimal logical device.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param parent Retained parent instance.
 * @param physical_device Borrowed physical device belonging to parent.
 * @param queue_family_index Selected queue family.
 * @return A device owner or acquisition failure.
 */
template <class TApi>
auto create_device(
    std::reference_wrapper<TApi> api,
    instance_ptr const & parent,
    VkPhysicalDevice physical_device,
    queue_family_index queue_family_index) -> vulkan_result<device_ptr>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<device_ptr>;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = value_of(queue_family_index);
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    VkDevice device = VK_NULL_HANDLE;
    auto const status = api.get().vk_create_device(physical_device, &create_info, nullptr, &device);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_device, status});
    }
    return result::success<result_type>(make_device_ptr(device, parent, api.get()));
}

/**
 * Supplies the API to deferred device acquisition.
 *
 * @tparam TEnvironment Execution environment type.
 * @param parent Retained parent instance.
 * @param physical_device Borrowed physical device belonging to parent.
 * @param queue_family_index Selected queue family.
 * @param environment Execution capabilities.
 * @return Deferred device-acquisition operation.
 */
template <class TEnvironment>
constexpr auto io_create_device(
    instance_ptr const & parent,
    VkPhysicalDevice physical_device,
    queue_family_index queue_family_index,
    TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing a device owner or acquisition failure.
    auto acquire_device = std::bind_front(
        create_device<api_type>, environment.api, parent, physical_device, queue_family_index);

    // IO producing a device owner or acquisition failure.
    return io::create<"vk_create_device"_ss>(std::move(acquire_device));
}
}  // namespace vulkandemo::vulkan::detail
