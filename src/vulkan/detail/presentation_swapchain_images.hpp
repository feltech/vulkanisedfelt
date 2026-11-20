// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_swapchain_images.hpp
 *
 * Image retrieval for an acquired presentation swapchain.
 */
#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../errors.hpp"
#include "../presentation_swapchain_images_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../renderer_errors.hpp"
#include "../swapchain_images.hpp"
#include "../swapchain_images_types.hpp"
#include "./presentation_swapchain.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed to create a presentation swapchain and retrieve its images. */
template <class TEnvironment>
concept create_presentation_swapchain_images_environment =
    create_presentation_swapchain_environment<TEnvironment> &&
    vk_get_swapchain_images_environment<TEnvironment>;

/**
 * Joins presentation configuration with retrieved images and their retained owner.
 *
 * @param presentation_swapchain Acquired presentation context.
 * @param snapshot Retrieved image handles with matching swapchain ownership.
 * @return Complete presentation image context.
 */
auto make_presentation_swapchain_images(
    PresentationSwapchain presentation_swapchain, SwapchainImages snapshot)
    -> PresentationSwapchainImages;

/**
 * Retrieves images for an acquired presentation context.
 *
 * @param presentation_swapchain Acquired context retained throughout image use.
 * @return ReaderIO providing the complete image context or its query failure.
 */
template <class TEnvironment>
constexpr auto reader_io_get_presentation_swapchain_images(
    PresentationSwapchain presentation_swapchain)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing image handles with their retained native owner.
    auto query = reader_io_vk_get_swapchain_images_khr<TEnvironment>(
        presentation_swapchain.presentation_device.device, presentation_swapchain.swapchain);

    // ReaderIO preserving the original Vulkan query failure in the renderer error domain.
    auto query_with_renderer_errors = reader_io::transform_error<"swapchain_image_query_error"_ss>(
        std::move(query), renderer_error_from_vulkan_failure);

    // ReaderIO providing image handles together with configuration, queues and all parents.
    return reader_io::transform<"make_presentation_swapchain_images"_ss>(
        std::move(query_with_renderer_errors),
        std::bind_front(make_presentation_swapchain_images, std::move(presentation_swapchain)));
}
}  // namespace vulkandemo::vulkan::detail
