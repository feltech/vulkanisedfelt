// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/image_view.hpp
 *
 * Creation of owned swapchain image views.
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
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to create and release an image view. */
template <class TEnvironment>
concept vk_create_image_view_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkImageViewCreateInfo const * info,
             VkImageView * view,
             VkImageView handle)
{
    {environment.api.get().vk_create_image_view(device, info, nullptr, view)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_image_view(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates a view retaining the swapchain-owned image's lifetime.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param swapchain Retained image owner.
 * @param image Borrowed image handle.
 * @param format Selected image format.
 * @return Owned view or the original creation failure.
 */
template <class TApi>
auto create_image_view(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    swapchain_ptr swapchain,
    VkImage image,
    VkFormat format)
    -> vulkan_result<image_view_ptr> pre(
        device != nullptr && swapchain != nullptr && image != VK_NULL_HANDLE)
        pre(format != VK_FORMAT_UNDEFINED)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<image_view_ptr>;
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY};
    info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView handle = VK_NULL_HANDLE;
    VkResult status = api.get().vk_create_image_view(device.get(), &info, nullptr, &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_image_view, status});
    }
    return result::success<result_type>(
        make_image_view_ptr(handle, std::move(device), std::move(swapchain), api.get()));
}

/**
 * Provides view creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param swapchain Retained image owner.
 * @param image Borrowed image handle.
 * @param format Selected image format.
 * @param environment Borrowed command capabilities.
 * @return Deferred image view creation.
 */
template <class TEnvironment>
constexpr auto create_image_view_reader_io_action(
    device_ptr device,
    swapchain_ptr swapchain,
    VkImage image,
    VkFormat format,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing one color view with its native parents retained.
    auto create_view = std::bind_front(
        create_image_view<api_type>,
        environment.api,
        std::move(device),
        std::move(swapchain),
        image,
        format);

    // IO providing owned view creation or the original Vulkan failure.
    return io::create<"vk_create_image_view"_ss>(std::move(create_view));
}
}  // namespace vulkandemo::vulkan::detail
