// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_image_views_types.hpp
 *
 * Presentation resources with owned views of their swapchain images.
 */
#pragma once

#include <immer/array.hpp>

#include "./managed_handles_types.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/** Presentation context and views in the corresponding swapchain image order. */
struct PresentationImageViews
{
    /** Borrowed images with their configuration, queues and ownership. */
    PresentationSwapchainImages presentation_swapchain_images;
    /** Color views, each retaining the swapchain independently. */
    immer::array<image_view_ptr> image_views;
};

/** Outcome of presentation image view creation. */
using presentation_image_views_result = renderer_result<PresentationImageViews>;
}  // namespace vulkandemo::vulkan
