// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_framebuffers_types.hpp
 *
 * Presentation attachments with owned framebuffers.
 */
#pragma once

#include <immer/array.hpp>

#include "./managed_handles_types.hpp"
#include "./presentation_image_views_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/** Presentation context and framebuffers in swapchain image order. */
struct PresentationFramebuffers
{
    /** Selected presentation context and color views. */
    PresentationImageViews presentation_image_views;
    /** Render pass compatible with every framebuffer. */
    render_pass_ptr render_pass;
    /** Framebuffers, each retaining its attachment and render pass independently. */
    immer::array<framebuffer_ptr> framebuffers;
};

/** Outcome of presentation framebuffer creation. */
using presentation_framebuffers_result = renderer_result<PresentationFramebuffers>;
}  // namespace vulkandemo::vulkan
