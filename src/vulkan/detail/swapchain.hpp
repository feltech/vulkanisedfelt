// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain.hpp
 *
 * Swapchain creation and replacement implementation.
 */
#pragma once

#include <array>
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
#include "../presentation_device_types.hpp"
#include "../swapchain_configuration_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to create and destroy a swapchain. */
template <class TEnvironment>
concept vk_create_swapchain_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkSwapchainCreateInfoKHR const * info,
             VkAllocationCallbacks const * allocator,
             VkSwapchainKHR * swapchain)
{
    {environment.api.get().vk_create_swapchain_khr(device, info, allocator, swapchain)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_swapchain_khr(device, *swapchain, allocator)}
        ->std::same_as<void>;
};

/**
 * Whether the mode uses ordinary per-image presentation.
 *
 * @param mode Presentation mode.
 * @return Whether the minimal swapchain supports the mode's semantics.
 */
constexpr bool is_ordinary_present_mode(VkPresentModeKHR mode)
{
    return mode == VK_PRESENT_MODE_IMMEDIATE_KHR || mode == VK_PRESENT_MODE_MAILBOX_KHR ||
        mode == VK_PRESENT_MODE_FIFO_KHR || mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
}

/**
 * Chooses opaque alpha when supported, otherwise the first supported core alpha mode.
 *
 * @param supported Nonempty supported composite-alpha flags.
 * @return Supported alpha mode.
 */
constexpr auto select_composite_alpha(VkCompositeAlphaFlagsKHR supported)
    -> VkCompositeAlphaFlagBitsKHR
    pre((supported &
         (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR |
          VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)) != 0)
{
    for (VkCompositeAlphaFlagBitsKHR mode : std::array{
             VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
             VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
             VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
             VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    {
        if ((supported & mode) != 0)
        {
            return mode;
        }
    }
    std::unreachable();
}

/**
 * Creates a swapchain from validated surface, image and optional predecessor parameters.
 *
 * @param api Borrowed command implementation.
 * @param presentation_device Retained device, surface and queue roles.
 * @param configuration Validated ordinary swapchain choices.
 * @param capabilities Current surface capabilities.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @return Acquired swapchain ownership or the original Vulkan failure.
 */
template <class TApi>
auto create_swapchain_io_action(
    std::reference_wrapper<TApi> api,
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    VkSurfaceCapabilitiesKHR capabilities,
    swapchain_ptr old_swapchain = {})
    -> vulkan_result<swapchain_ptr> pre(
        presentation_device.device != nullptr && presentation_device.surface != nullptr)
        pre(configuration.extent.width > 0 && configuration.extent.height > 0)
            pre(is_ordinary_present_mode(configuration.present_mode))
                pre((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
                    pre(capabilities.supportedCompositeAlpha != 0)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<swapchain_ptr>;
    std::array<std::uint32_t, 2> families{
        value_of(presentation_device.queue_families.graphics_family_index),
        value_of(presentation_device.queue_families.present_family_index)};
    bool const has_separate_families = families[0] != families[1];
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = presentation_device.surface.get();
    info.minImageCount = value_of(configuration.image_count);
    info.imageFormat = configuration.surface_format.format;
    info.imageColorSpace = configuration.surface_format.colorSpace;
    info.imageExtent = configuration.extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode =
        has_separate_families ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    // EXCLUSIVE sharing ignores these fields; they do not bind an image to a queue family.
    info.queueFamilyIndexCount = has_separate_families ? 2 : 0;
    info.pQueueFamilyIndices = has_separate_families ? families.data() : nullptr;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = select_composite_alpha(capabilities.supportedCompositeAlpha);
    info.presentMode = configuration.present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = old_swapchain ? old_swapchain.get() : VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult status = api.get().vk_create_swapchain_khr(
        presentation_device.device.get(), &info, nullptr, &swapchain);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_swapchain, status});
    }
    return result::success<result_type>(make_swapchain_ptr(
        swapchain,
        std::move(presentation_device.device),
        std::move(presentation_device.surface),
        api.get()));
}

/**
 * Provides swapchain acquisition from borrowed execution capabilities.
 *
 * @param presentation_device Retained device, surface and queue roles.
 * @param configuration Validated ordinary swapchain choices.
 * @param capabilities Current surface capabilities.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @param environment Borrowed command capabilities.
 * @return Deferred swapchain acquisition.
 */
template <class TEnvironment>
constexpr auto create_swapchain_reader_io_action(
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    VkSurfaceCapabilitiesKHR capabilities,
    swapchain_ptr old_swapchain,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing a swapchain while retaining parents and any predecessor through creation.
    auto acquire = std::bind_front(
        create_swapchain_io_action<api_type>,
        environment.api,
        std::move(presentation_device),
        configuration,
        capabilities,
        std::move(old_swapchain));

    // IO providing shared ownership or the original Vulkan creation failure.
    return io::create<"vk_create_swapchain_khr"_ss>(std::move(acquire));
}
}  // namespace vulkandemo::vulkan::detail
