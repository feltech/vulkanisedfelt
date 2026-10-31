// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/instance.hpp
 *
 * Implementation of Vulkan instance operations.
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

namespace vulkandemo::vulkan
{
/** Commands needed to acquire and release an instance. */
template <class TEnvironment>
concept vk_create_instance_environment = backend::api_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkInstanceCreateInfo const * info,
             VkAllocationCallbacks const * allocator,
             VkInstance * instance)
{
    {environment.api.get().vk_create_instance(info, allocator, instance)}->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_instance(*instance, allocator)}->std::same_as<void>;
};
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Acquires a minimal Vulkan instance.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @return An instance owner or acquisition failure.
 */
template <class TApi>
auto create_instance(std::reference_wrapper<TApi> api) -> vulkan_result<instance_ptr>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<instance_ptr>;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance = VK_NULL_HANDLE;
    auto const status = api.get().vk_create_instance(&create_info, nullptr, &instance);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_instance, status});
    }
    return result::success<result_type>(make_instance_ptr(instance, api.get()));
}

/**
 * Supplies the API to deferred instance acquisition.
 *
 * @tparam TEnvironment Execution environment type.
 * @param environment Execution capabilities.
 * @return Deferred instance-acquisition operation.
 */
template <class TEnvironment>
constexpr auto io_create_instance(TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable producing an instance owner or acquisition failure.
    auto acquire_instance = std::bind_front(create_instance<api_type>, environment.api);

    // IO producing an instance owner or acquisition failure.
    return io::create<"vk_create_instance"_ss>(std::move(acquire_instance));
}
}  // namespace vulkandemo::vulkan::detail
