// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_framebuffers.hpp
 *
 * Deferred creation of framebuffers for presentation views.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./physical_device_properties.hpp"
#include "./presentation_image_views_types.hpp"
#include "./presentation_swapchain_types.hpp"
#include "detail/presentation_framebuffers.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans framebuffer creation for an existing presentation view snapshot.
 *
 * The render pass must belong to the snapshot's device and match its single-sample color format.
 * The snapshot must retain its matching parents and nonzero selected extent. Every framebuffer
 * independently retains its view and render pass. Keep the borrowed API alive through cleanup,
 * finish GPU work and synchronize host access before final release. Failed creation releases all
 * framebuffers acquired by that execution without publishing a partial set.
 *
 * @param snapshot Existing presentation context with owned color views.
 * @param render_pass Compatible clear-and-present render pass.
 * @return Reusable ReaderIO providing all framebuffers, stop, or renderer error.
 */
template <detail::create_presentation_framebuffers_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_framebuffers(
    PresentationImageViews snapshot, render_pass_ptr render_pass)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    PresentationSwapchain const & presentation_swapchain =
        snapshot.presentation_swapchain_images.presentation_swapchain;

    // ReaderIO providing physical limits beyond the image-format and surface extent limits.
    auto properties = reader_io_vk_get_physical_device_properties<TEnvironment>(
        presentation_swapchain.presentation_device.physical_device);

    // ReaderIO providing a supported extent or a diagnostic before any native framebuffer create.
    auto extent = reader_io::transform<"validate_framebuffer_extent"_ss>(
        std::move(properties),
        std::bind_front(
            detail::validate_framebuffer_extent, presentation_swapchain.configuration.extent));

    // ReaderIO providing owned targets only after framebuffer dimensions are known to fit.
    return reader_io::and_then<"create_validated_framebuffers"_ss>(
        std::move(extent),
        std::bind_front(
            detail::reader_io_create_validated_framebuffers<TEnvironment>,
            std::move(snapshot),
            std::move(render_pass)));
}
}  // namespace vulkandemo::vulkan
