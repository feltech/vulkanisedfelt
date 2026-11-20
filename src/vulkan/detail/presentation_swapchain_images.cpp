// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_swapchain_images.hpp"

#include <utility>

namespace vulkandemo::vulkan::detail
{
auto make_presentation_swapchain_images(
    PresentationSwapchain presentation_swapchain, SwapchainImages snapshot)
    -> PresentationSwapchainImages
{
    return {std::move(presentation_swapchain), std::move(snapshot.images)};
}
}  // namespace vulkandemo::vulkan::detail
