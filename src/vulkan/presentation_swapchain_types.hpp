// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_swapchain_types.hpp
 *
 * An owned swapchain and the context needed for presentation work.
 */
#pragma once

#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./renderer_errors.hpp"
#include "./swapchain_configuration_types.hpp"

namespace vulkandemo::vulkan
{
/** Initial swapchain with its selected configuration, queues and retained parents. */
struct PresentationSwapchain
{
    /** Device, surface and queue roles used for this swapchain. */
    PresentationDevice presentation_device;
    /** Selected image and presentation choices. */
    SwapchainConfiguration configuration;
    /** Shared swapchain ownership. */
    swapchain_ptr swapchain;
};

/** Initial presentation-swapchain outcome. */
using presentation_swapchain_result = renderer_result<PresentationSwapchain>;
}  // namespace vulkandemo::vulkan
