// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/image_format_properties.hpp
 *
 * Image-format support query implementation.
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

namespace vulkandemo::vulkan::detail
{
/** Commands needed to inspect image-format support. */
template <class TEnvironment>
concept vk_get_physical_device_image_format_properties_environment =
    backend::detail::api_environment<TEnvironment> &&
    requires(
        TEnvironment environment,
        VkPhysicalDevice device,
        VkFormat format,
        VkImageType type,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkImageCreateFlags flags,
        VkImageFormatProperties * properties)
{
    {environment.api.get().vk_get_physical_device_image_format_properties(
         device, format, type, tiling, usage, flags, properties)}
        ->std::same_as<VkResult>;
};

/**
 * Retrieves supported limits for one image description.
 *
 * @param api Borrowed command implementation.
 * @param device Borrowed physical device.
 * @param format Image format.
 * @param type Image dimensionality.
 * @param tiling Image memory layout.
 * @param usage Required image usages.
 * @param flags Image creation flags.
 * @return Owned properties or the original Vulkan failure.
 */
template <class TApi>
auto get_image_format_properties(
    std::reference_wrapper<TApi> api,
    VkPhysicalDevice device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags)
    -> vulkan_result<VkImageFormatProperties> pre(device != VK_NULL_HANDLE)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<VkImageFormatProperties>;
    VkImageFormatProperties properties{};
    VkResult status = api.get().vk_get_physical_device_image_format_properties(
        device, format, type, tiling, usage, flags, &properties);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(VulkanFailure{
            FallibleVulkanOperation::get_physical_device_image_format_properties, status});
    }
    return result::success<result_type>(properties);
}

/**
 * Provides an image-format query from execution capabilities.
 *
 * @param device Borrowed physical device.
 * @param format Image format.
 * @param type Image dimensionality.
 * @param tiling Image memory layout.
 * @param usage Required image usages.
 * @param flags Image creation flags.
 * @param environment Borrowed command capabilities.
 * @return Deferred support query.
 */
template <class TEnvironment>
constexpr auto io_get_image_format_properties(
    VkPhysicalDevice device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing support limits for the exact requested image parameters.
    auto query_properties = std::bind_front(
        get_image_format_properties<api_type>,
        environment.api,
        device,
        format,
        type,
        tiling,
        usage,
        flags);

    // IO providing an independent property snapshot or the original Vulkan failure.
    return io::create<"vk_get_physical_device_image_format_properties"_ss>(
        std::move(query_properties));
}
}  // namespace vulkandemo::vulkan::detail
