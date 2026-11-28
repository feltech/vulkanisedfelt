// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_framebuffers.hpp
 *
 * Composition of swapchains, image views, compatible render passes and framebuffers.
 */
#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>

#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_device_types.hpp"
#include "../presentation_framebuffers.hpp"
#include "../presentation_image_views.hpp"
#include "../presentation_image_views_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../renderer_errors.hpp"
#include "../swapchain_framebuffers_types.hpp"
#include "./presentation_framebuffers.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_swapchain.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./render_pass.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed to assemble complete presentation framebuffers from live surface facts. */
template <class TEnvironment>
concept create_swapchain_framebuffers_environment =
    create_presentation_swapchain_images_environment<TEnvironment> &&
    create_presentation_image_views_environment<TEnvironment> &&
    vk_create_render_pass_environment<TEnvironment> &&
    create_presentation_framebuffers_environment<TEnvironment>;

/**
 * Reuses a compatible clear-and-present render pass or creates one for a changed format.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param format Selected swapchain image format.
 * @param existing Optional prior render pass and its format.
 * @return Compatible render pass or the original creation failure.
 */
template <class TApi>
auto acquire_compatible_render_pass_io_action(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    VkFormat format,
    CompatibleRenderPass existing) -> vulkan_result<render_pass_ptr> pre(device != nullptr)
    pre(format != VK_FORMAT_UNDEFINED)
{
    namespace result = fnfelt::result;
    if (existing.render_pass != nullptr && existing.format == format)
    {
        // Format matches the existing compatible pass, so reuse it without a driver call.
        return result::success<vulkan_result<render_pass_ptr>>(std::move(existing.render_pass));
    }
    // Either no pass existed yet or the swapchain format changed; create a matching pass.
    return create_render_pass(api, std::move(device), format);
}

/**
 * Provides a compatible render pass from borrowed execution capabilities.
 *
 * @param device Retained logical device.
 * @param format Selected swapchain image format.
 * @param existing Optional prior render pass and its format.
 * @param environment Borrowed command capabilities.
 * @return Deferred compatible render-pass acquisition.
 */
template <class TEnvironment>
constexpr auto acquire_compatible_render_pass_reader_io_action(
    device_ptr device,
    VkFormat format,
    CompatibleRenderPass existing,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable reusing a matching pass without driver calls or creating one for a new format.
    auto acquire_pass = std::bind_front(
        acquire_compatible_render_pass_io_action<api_type>,
        environment.api,
        std::move(device),
        format,
        std::move(existing));

    // IO providing a compatible render pass or the original Vulkan creation failure.
    return io::create<"acquire_compatible_render_pass"_ss>(std::move(acquire_pass));
}

/**
 * Plans compatible render-pass acquisition and framebuffer creation for presentation views.
 *
 * @param existing_render_pass Optional prior render pass eligible for format-matched reuse.
 * @param views Acquired swapchain, borrowed images and owned color views.
 * @return ReaderIO providing complete presentation framebuffers or a renderer failure.
 */
template <class TEnvironment>
constexpr auto reader_io_build_swapchain_framebuffers(
    CompatibleRenderPass existing_render_pass, PresentationImageViews views)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    PresentationSwapchain const & presentation_swapchain =
        views.presentation_swapchain_images.presentation_swapchain;

    // Action providing a clear-and-present render pass compatible with the selected format.
    auto acquire_pass = std::bind_front(
        acquire_compatible_render_pass_reader_io_action<TEnvironment>,
        presentation_swapchain.presentation_device.device,
        presentation_swapchain.configuration.surface_format.format,
        std::move(existing_render_pass));
    auto render_pass = reader_io::create<"acquire_compatible_render_pass"_ss, TEnvironment>(
        std::move(acquire_pass));

    // ReaderIO preserving native render-pass creation failures in the renderer error domain.
    auto pass_with_renderer_errors = reader_io::transform_error<"render_pass_creation_error"_ss>(
        std::move(render_pass), renderer_error_from_vulkan_failure);

    // Continuation building framebuffers matching each view and the compatible render pass.
    auto create_framebuffers =
        std::bind_front(reader_io_create_presentation_framebuffers<TEnvironment>, std::move(views));

    // ReaderIO providing framebuffers matching each view and the compatible render pass.
    return reader_io::and_then<"create_swapchain_framebuffers"_ss>(
        std::move(pass_with_renderer_errors), std::move(create_framebuffers));
}

/**
 * Plans swapchain creation, image retrieval, views, compatible pass and framebuffers from prepared
 * surface choices.
 *
 * @param presentation_device Retained device, surface and presentation queues.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @param existing_render_pass Optional prior render pass reused when the selected format matches.
 * @param prepared Validated surface, extent and image-format configuration.
 * @return ReaderIO providing complete presentation framebuffers or a renderer failure.
 */
template <class TEnvironment>
constexpr auto reader_io_build_prepared_swapchain_framebuffers(
    PresentationDevice presentation_device,
    swapchain_ptr old_swapchain,
    CompatibleRenderPass existing_render_pass,
    PreparedSwapchain prepared)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the acquired swapchain, optionally handing off a prior handle.
    auto swapchain = reader_io_create_prepared_swapchain<TEnvironment>(
        std::move(presentation_device), std::move(old_swapchain), prepared);

    // ReaderIO providing presentable images owned through the acquired swapchain.
    auto images = reader_io::and_then<"get_swapchain_framebuffers_images"_ss>(
        std::move(swapchain), reader_io_get_presentation_swapchain_images<TEnvironment>);

    // ReaderIO providing color views for each retrieved swapchain image.
    auto views = reader_io::and_then<"create_swapchain_framebuffers_views"_ss>(
        std::move(images), reader_io_create_presentation_image_views<TEnvironment>);

    // Continuation selecting or creating a compatible pass and building presentation framebuffers.
    auto build_framebuffers = std::bind_front(
        reader_io_build_swapchain_framebuffers<TEnvironment>, std::move(existing_render_pass));

    // ReaderIO providing the compatible render pass and complete presentation framebuffers.
    return reader_io::and_then<"build_swapchain_framebuffers"_ss>(
        std::move(views), std::move(build_framebuffers));
}
}  // namespace vulkandemo::vulkan::detail
