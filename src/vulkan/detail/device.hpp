// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/device.hpp
 *
 * Implementation of Vulkan device operations.
 */
#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../../backend/detail/api.hpp"
#include "../../text/string.hpp"
#include "../errors.hpp"
#include "../managed_handles.hpp"
#include "../queue_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to acquire and release a logical device. */
template <class TEnvironment>
concept vk_create_device_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkPhysicalDevice physical_device,
             VkDeviceCreateInfo const * info,
             VkAllocationCallbacks const * allocator,
             VkDevice * device)
{
    {environment.api.get().vk_create_device(physical_device, info, allocator, device)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_device(*device, allocator)}->std::same_as<void>;
};
/**
 * Acquires a logical device with requested queues and extensions.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param parent Retained parent instance.
 * @param physical_device Borrowed physical device belonging to parent.
 * @param queue_families Requested queue families.
 * @param extension_names Owned extension names.
 * @return A device owner or acquisition failure.
 */
template <class TApi>
auto create_device(
    std::reference_wrapper<TApi> api,
    instance_ptr const & parent,
    VkPhysicalDevice physical_device,
    immer::array<queue_family_index> const & queue_families,
    immer::array<text::String> const & extension_names)
    -> vulkan_result<device_ptr> pre(!queue_families.empty())
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<device_ptr>;
    float priority = 1.0f;
    // Each distinct family contributes one queue, sharing the synchronous priority storage.
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(queue_families.size());
    for (queue_family_index family : queue_families)
    {
        if (std::ranges::find(
                queue_infos, value_of(family), &VkDeviceQueueCreateInfo::queueFamilyIndex) !=
            queue_infos.end())
        {
            continue;
        }
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = value_of(family);
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        queue_infos.push_back(queue_info);
    }

    // Stable C strings borrow the owned immutable names for this API call.
    std::vector<char const *> name_pointers;
    name_pointers.reserve(extension_names.size());
    for (text::String const & name : extension_names)
    {
        name_pointers.push_back(name.c_str());
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(name_pointers.size());
    create_info.ppEnabledExtensionNames = name_pointers.empty() ? nullptr : name_pointers.data();
    VkDevice device = VK_NULL_HANDLE;
    VkResult const status =
        api.get().vk_create_device(physical_device, &create_info, nullptr, &device);
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
 * @param queue_families Requested queue families.
 * @param extension_names Owned extension names.
 * @param environment Execution capabilities.
 * @return Deferred device-acquisition operation.
 */
template <class TEnvironment>
constexpr auto create_device_reader_io_action(
    instance_ptr const & parent,
    VkPhysicalDevice physical_device,
    immer::array<queue_family_index> const & queue_families,
    immer::array<text::String> const & extension_names,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing a device owner or acquisition failure.
    auto acquire_device = std::bind_front(
        create_device<api_type>,
        environment.api,
        parent,
        physical_device,
        queue_families,
        extension_names);

    // IO producing a device owner or acquisition failure.
    return io::create<"vk_create_device"_ss>(std::move(acquire_device));
}
}  // namespace vulkandemo::vulkan::detail
