// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/framebuffer.hpp
 *
 * Creation of owned presentation framebuffers.
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
/** Commands needed to create and release a framebuffer. */
template <class TEnvironment>
concept vk_create_framebuffer_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkFramebufferCreateInfo const * info,
             VkFramebuffer * framebuffer,
             VkFramebuffer handle)
{
    {environment.api.get().vk_create_framebuffer(device, info, nullptr, framebuffer)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_framebuffer(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates a single-attachment framebuffer retaining its native parents.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param render_pass Compatible render pass.
 * @param image_view Color attachment view.
 * @param extent Selected image extent.
 * @return Owned framebuffer or the original creation failure.
 */
template <class TApi>
auto create_framebuffer(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    render_pass_ptr render_pass,
    image_view_ptr image_view,
    VkExtent2D extent)
    -> vulkan_result<framebuffer_ptr> pre(
        device != nullptr && render_pass != nullptr && image_view != nullptr)
        pre(extent.width > 0 && extent.height > 0)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<framebuffer_ptr>;
    VkImageView attachment = image_view.get();
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = render_pass.get();
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;
    VkFramebuffer handle = VK_NULL_HANDLE;
    VkResult status = api.get().vk_create_framebuffer(device.get(), &info, nullptr, &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_framebuffer, status});
    }
    return result::success<result_type>(make_framebuffer_ptr(
        handle, std::move(device), std::move(render_pass), std::move(image_view), api.get()));
}

/**
 * Provides framebuffer creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param render_pass Compatible render pass.
 * @param image_view Color attachment view.
 * @param extent Selected image extent.
 * @param environment Borrowed command capabilities.
 * @return Deferred framebuffer creation.
 */
template <class TEnvironment>
constexpr auto create_framebuffer_reader_io_action(
    device_ptr device,
    render_pass_ptr render_pass,
    image_view_ptr image_view,
    VkExtent2D extent,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing one framebuffer with its native parents retained.
    auto create_target = std::bind_front(
        create_framebuffer<api_type>,
        environment.api,
        std::move(device),
        std::move(render_pass),
        std::move(image_view),
        extent);

    // IO providing framebuffer ownership or the original Vulkan failure.
    return io::create<"vk_create_framebuffer"_ss>(std::move(create_target));
}
}  // namespace vulkandemo::vulkan::detail
