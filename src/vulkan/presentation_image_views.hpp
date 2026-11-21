// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_image_views.hpp
 *
 * Deferred creation of color views for presentation images.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./image_view.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./presentation_swapchain_types.hpp"
#include "./renderer_errors.hpp"
#include "detail/presentation_image_views.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of color views for an existing presentation image snapshot.
 *
 * The snapshot must retain its matching device and swapchain, and use the selected image format.
 * Every returned view independently retains its swapchain. The borrowed API must outlive all
 * plans and owners. Finish GPU work and synchronize host access before final release.
 * Failed creation releases all views acquired by that execution without publishing a partial set.
 *
 * @param snapshot Existing presentation context with swapchain-owned images.
 * @return Reusable ReaderIO providing all views, stop, or renderer error.
 */
template <detail::create_presentation_image_views_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_image_views(
    PresentationSwapchainImages snapshot)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    PresentationSwapchain const & presentation_swapchain = snapshot.presentation_swapchain;

    // Continuation providing a view for each image with matching owners and selected format.
    auto create_view = std::bind_front(
        std::bind_back(
            reader_io_vk_create_image_view<TEnvironment>,
            presentation_swapchain.configuration.surface_format.format),
        presentation_swapchain.presentation_device.device,
        presentation_swapchain.swapchain);

    // ReaderIO providing every creation outcome in image order after all effects finish.
    auto outcomes = reader_io::traverse<"create_presentation_image_views"_ss, TEnvironment>(
        snapshot.images, std::move(create_view));

    // ReaderIO providing a complete set of views, any stop, or the first creation failure.
    auto image_views = reader_io::transform<"sequence_image_view_results"_ss>(
        std::move(outcomes), detail::sequence_image_view_results);

    // ReaderIO preserving native creation failures in the renderer error domain.
    auto views_with_renderer_errors = reader_io::transform_error<"image_view_creation_error"_ss>(
        std::move(image_views), renderer_error_from_vulkan_failure);

    // ReaderIO providing owned views alongside the complete presentation context.
    return reader_io::transform<"make_presentation_image_views"_ss>(
        std::move(views_with_renderer_errors),
        std::bind_front(detail::make_presentation_image_views, std::move(snapshot)));
}
}  // namespace vulkandemo::vulkan
