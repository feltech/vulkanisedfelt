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
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>

#include "../../backend/detail/api.hpp"
#include "../api_version_types.hpp"
#include "../errors.hpp"
#include "../managed_handles.hpp"
#include "../renderer_errors.hpp"
#include "./instance_version.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to acquire and release an instance. */
template <class TEnvironment>
concept vk_create_instance_environment = vk_enumerate_instance_version_environment<TEnvironment> &&
    requires(TEnvironment environment,
             VkInstanceCreateInfo const * info,
             VkAllocationCallbacks const * allocator,
             VkInstance * instance)
{
    {environment.api.get().vk_create_instance(info, allocator, instance)}->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_instance(*instance, allocator)}->std::same_as<void>;
};

/**
 * Validates instance support for the renderer's API baseline.
 *
 * @param available Instance version reported by the implementation.
 * @return Successful validation or the required and available versions.
 */
renderer_result<void> require_instance_api_version(api_version available);
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

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.apiVersion = value_of(minimum_api_version);
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult const status = api.get().vk_create_instance(&create_info, nullptr, &instance);
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

/**
 * Plans native instance creation in the renderer error domain.
 *
 * @tparam TEnvironment Execution environment type.
 * @return ReaderIO producing an instance owner or renderer failure.
 */
template <class TEnvironment>
constexpr auto reader_io_create_instance()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing native instance-acquisition IO.
    auto io_acquire_instance = io_create_instance<TEnvironment>;

    // ReaderIO providing the native owner or original Vulkan status.
    auto acquire_instance =
        reader_io::create<"vk_create_instance"_ss, TEnvironment>(std::move(io_acquire_instance));

    // ReaderIO providing the same owner with renderer-domain failures.
    return reader_io::transform_error<"instance_creation_error"_ss>(
        std::move(acquire_instance), renderer_error_from_vulkan_failure);
}
}  // namespace vulkandemo::vulkan::detail
