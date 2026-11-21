// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_framebuffers.hpp
 *
 * Preparation and composition of presentation framebuffers.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../errors.hpp"
#include "../framebuffer.hpp"
#include "../managed_handles_types.hpp"
#include "../physical_device_properties.hpp"
#include "../presentation_framebuffers_types.hpp"
#include "../presentation_image_views_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../renderer_errors.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed to validate and create presentation framebuffers. */
template <class TEnvironment>
concept create_presentation_framebuffers_environment =
    vk_create_framebuffer_environment<TEnvironment> &&
    vk_get_physical_device_properties_environment<TEnvironment>;

/**
 * Checks the selected extent against framebuffer-specific physical limits.
 *
 * @param extent Selected swapchain dimensions.
 * @param properties Physical device properties.
 * @return Supported extent or a framebuffer extent failure.
 */
auto validate_framebuffer_extent(VkExtent2D extent, VkPhysicalDeviceProperties properties)
    -> renderer_result<VkExtent2D> pre(extent.width > 0 && extent.height > 0);

/**
 * Sequences framebuffer outcomes into owners in image order.
 *
 * @param outcomes Independent creation outcomes.
 * @return All framebuffers, any stop, or the first creation error in image order.
 */
auto sequence_framebuffer_results(immer::array<vulkan_result<framebuffer_ptr>> outcomes)
    -> vulkan_result<immer::array<framebuffer_ptr>>;

/**
 * Joins framebuffers with their presentation context.
 *
 * @param snapshot Presentation views and selected configuration.
 * @param render_pass Compatible render pass.
 * @param framebuffers Owners in the corresponding image order.
 * @return Complete presentation framebuffer context.
 */
auto make_presentation_framebuffers(
    PresentationImageViews snapshot,
    render_pass_ptr render_pass,
    immer::array<framebuffer_ptr> framebuffers) -> PresentationFramebuffers;

/**
 * Provides framebuffer creation for an extent within the physical device's limits.
 *
 * @param snapshot Presentation views and selected configuration.
 * @param render_pass Compatible render pass.
 * @param extent Validated image dimensions.
 * @return ReaderIO providing a complete framebuffer context or an unsuccessful outcome.
 */
template <class TEnvironment>
constexpr auto reader_io_create_validated_framebuffers(
    PresentationImageViews snapshot, render_pass_ptr render_pass, VkExtent2D extent)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    PresentationSwapchain const & presentation_swapchain =
        snapshot.presentation_swapchain_images.presentation_swapchain;

    // Continuation providing a framebuffer for each retained color view.
    auto create_framebuffer = std::bind_front(
        std::bind_back(reader_io_vk_create_framebuffer<TEnvironment>, extent),
        presentation_swapchain.presentation_device.device,
        render_pass);

    // ReaderIO providing all creation outcomes in image order after independent effects finish.
    auto outcomes = reader_io::traverse<"create_presentation_framebuffers"_ss, TEnvironment>(
        snapshot.image_views, std::move(create_framebuffer));

    // ReaderIO providing all owners, any stop, or the first native creation error.
    auto framebuffers = reader_io::transform<"sequence_framebuffer_results"_ss>(
        std::move(outcomes), sequence_framebuffer_results);

    // ReaderIO preserving creation failures in the renderer error domain.
    auto framebuffers_with_renderer_errors =
        reader_io::transform_error<"framebuffer_creation_error"_ss>(
            std::move(framebuffers), renderer_error_from_vulkan_failure);

    // ReaderIO providing owned targets with the complete presentation context.
    return reader_io::transform<"make_presentation_framebuffers"_ss>(
        std::move(framebuffers_with_renderer_errors),
        std::bind_front(
            make_presentation_framebuffers, std::move(snapshot), std::move(render_pass)));
}
}  // namespace vulkandemo::vulkan::detail
