// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_image_views.hpp
 *
 * Composition of swapchain image view creation.
 */
#pragma once

#include <immer/array.hpp>

#include "../errors.hpp"
#include "../image_view.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_image_views_types.hpp"
#include "../presentation_swapchain_images_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to create all presentation image views. */
template <class TEnvironment>
concept create_presentation_image_views_environment =
    vk_create_image_view_environment<TEnvironment>;

/**
 * Sequences view outcomes into owned views in image order.
 *
 * @param outcomes Independent view creation outcomes.
 * @return All views, any stop, or the first creation error in image order.
 */
auto sequence_image_view_results(immer::array<vulkan_result<image_view_ptr>> outcomes)
    -> vulkan_result<immer::array<image_view_ptr>>;

/**
 * Joins image views with their presentation context.
 *
 * @param snapshot Presentation context with borrowed images.
 * @param image_views Owned views in the corresponding image order.
 * @return Complete presentation image view context.
 */
auto make_presentation_image_views(
    PresentationSwapchainImages snapshot, immer::array<image_view_ptr> image_views)
    -> PresentationImageViews;
}  // namespace vulkandemo::vulkan::detail
