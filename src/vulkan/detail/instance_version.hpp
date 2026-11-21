// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/instance_version.hpp
 *
 * Implementation of instance API version discovery.
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
#include "../api_version_types.hpp"
#include "../errors.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to discover the instance API version. */
template <class TEnvironment>
concept vk_enumerate_instance_version_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment, VkInstance instance, char const * name)
{
    {environment.api.get().vk_get_instance_proc_addr(instance, name)}
        ->std::same_as<PFN_vkVoidFunction>;
};

/**
 * Retrieves the loader's instance version, including Vulkan 1.0 implementations.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @return Instance API version or the original query failure.
 */
template <class TApi>
vulkan_result<api_version> enumerate_instance_version(std::reference_wrapper<TApi> api)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<api_version>;

    // Vulkan 1.0 loaders need not export the version query.
    auto enumerate_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        api.get().vk_get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerate_version == nullptr)
    {
        return result::success<result_type>(api_version{VK_API_VERSION_1_0});
    }

    std::uint32_t version = 0;
    VkResult status = enumerate_version(&version);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::enumerate_instance_version, status});
    }
    return result::success<result_type>(api_version{version});
}

/**
 * Supplies the API to deferred instance-version discovery.
 *
 * @tparam TEnvironment Execution environment type.
 * @param environment Execution capabilities.
 * @return Deferred instance-version query.
 */
template <class TEnvironment>
constexpr auto enumerate_instance_version_reader_io_action(TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing the implementation's instance API version.
    auto enumerate_version = std::bind_front(enumerate_instance_version<api_type>, environment.api);

    // IO providing the reported API version or query failure.
    return io::create<"vk_enumerate_instance_version"_ss>(std::move(enumerate_version));
}
}  // namespace vulkandemo::vulkan::detail
