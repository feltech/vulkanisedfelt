// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/semaphore.hpp
 *
 * Creation of owned binary semaphores.
 */
#pragma once

#include <concepts>
#include <functional>
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
/** Commands needed to create and release a semaphore. */
template <class TEnvironment>
concept vk_create_semaphore_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkSemaphoreCreateInfo const * info,
             VkSemaphore * semaphore,
             VkSemaphore handle)
{
    {environment.api.get().vk_create_semaphore(device, info, nullptr, semaphore)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_semaphore(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates a binary semaphore with a native Vulkan outcome.
 *
 * @param api Borrowed command implementation.
 * @param device Borrowed logical device.
 * @param semaphore Native output handle.
 * @return Native creation status.
 */
template <class TApi>
auto create_semaphore(std::reference_wrapper<TApi> api, VkDevice device, VkSemaphore * semaphore)
    -> VkResult pre(device != VK_NULL_HANDLE && semaphore != nullptr)
{
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    return api.get().vk_create_semaphore(device, &info, nullptr, semaphore);
}

/**
 * Executes binary semaphore creation as an IO action.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @return Owned semaphore or the original creation failure.
 */
template <class TApi>
auto create_semaphore_io_action(std::reference_wrapper<TApi> api, device_ptr device)
    -> vulkan_result<semaphore_ptr> pre(device != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<semaphore_ptr>;
    VkSemaphore handle = VK_NULL_HANDLE;
    VkResult status = create_semaphore(api, device.get(), &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_semaphore, status});
    }
    return result::success<result_type>(make_semaphore_ptr(handle, std::move(device), api.get()));
}

/**
 * Provides semaphore creation from a borrowed command implementation.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @return Deferred semaphore creation.
 */
template <class TApi>
constexpr auto io_create_semaphore(std::reference_wrapper<TApi> api, device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable providing an owned binary semaphore with its device retained.
    auto create_semaphore_with_device =
        std::bind_front(create_semaphore_io_action<TApi>, api, std::move(device));

    // IO providing semaphore ownership or the original Vulkan failure.
    return io::create<"vk_create_semaphore"_ss>(std::move(create_semaphore_with_device));
}

/**
 * Provides semaphore creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param environment Borrowed command capabilities.
 * @return Deferred semaphore creation.
 */
template <class TEnvironment>
constexpr auto create_semaphore_reader_io_action(
    device_ptr device, TEnvironment const & environment)
{
    return io_create_semaphore(environment.api, std::move(device));
}
}  // namespace vulkandemo::vulkan::detail
