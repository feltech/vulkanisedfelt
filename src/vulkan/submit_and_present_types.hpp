// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/submit_and_present_types.hpp
 *
 * Completed bounded presentation outcomes.
 */
#pragma once

#include "./errors.hpp"
#include "./image_acquisition_types.hpp"

namespace vulkandemo::vulkan
{
/** A presented image whose acquisition, rendering and presentation uses permit release. */
struct CompletedPresentation
{
    /** Index of the presented swapchain image. */
    swapchain_image_index image_index;
    /** Whether acquisition or presentation reported a suboptimal swapchain. */
    bool is_suboptimal;
};

/** Outcome returned after all potentially queued uses permit resource release. */
using completed_presentation_result = vulkan_result<CompletedPresentation>;
}  // namespace vulkandemo::vulkan
