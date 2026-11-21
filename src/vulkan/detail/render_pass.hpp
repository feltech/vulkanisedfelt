// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/render_pass.hpp
 *
 * Creation of owned clear-and-present render passes.
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
/** Commands needed to create and release a render pass. */
template <class TEnvironment>
concept vk_create_render_pass_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkDevice device,
             VkRenderPassCreateInfo const * info,
             VkRenderPass * render_pass,
             VkRenderPass handle)
{
    {environment.api.get().vk_create_render_pass(device, info, nullptr, render_pass)}
        ->std::same_as<VkResult>;
    {environment.api.get().vk_destroy_render_pass(device, handle, nullptr)}->std::same_as<void>;
};

/**
 * Creates a single-subpass render pass for clearing a presentation image.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param format Selected image format.
 * @return Owned render pass or the original creation failure.
 */
template <class TApi>
auto create_render_pass(std::reference_wrapper<TApi> api, device_ptr device, VkFormat format)
    -> vulkan_result<render_pass_ptr> pre(device != nullptr) pre(format != VK_FORMAT_UNDEFINED)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<render_pass_ptr>;
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    constexpr VkAttachmentReference color_attachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    // Scope the initial layout transition to a future acquire wait at color-attachment output.
    // Submission must supply that wait; this dependency alone does not acquire the image.
    constexpr VkSubpassDependency dependency{
        VK_SUBPASS_EXTERNAL,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0};
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    VkRenderPass handle = VK_NULL_HANDLE;
    VkResult status = api.get().vk_create_render_pass(device.get(), &info, nullptr, &handle);
    if (status != VK_SUCCESS)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::create_render_pass, status});
    }
    return result::success<result_type>(make_render_pass_ptr(handle, std::move(device), api.get()));
}

/**
 * Provides render pass creation from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param format Selected image format.
 * @param environment Borrowed command capabilities.
 * @return Deferred render pass creation.
 */
template <class TEnvironment>
constexpr auto create_render_pass_reader_io_action(
    device_ptr device, VkFormat format, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing one render pass with its logical device retained.
    auto create_pass =
        std::bind_front(create_render_pass<api_type>, environment.api, std::move(device), format);

    // IO providing owned render pass creation or the original Vulkan failure.
    return io::create<"vk_create_render_pass"_ss>(std::move(create_pass));
}
}  // namespace vulkandemo::vulkan::detail
