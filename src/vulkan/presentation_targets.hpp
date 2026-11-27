// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_targets.hpp
 *
 * Deferred assembly of complete presentation framebuffers with compatible render-pass reuse.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_targets_types.hpp"
#include "detail/presentation_swapchain.hpp"
#include "detail/presentation_targets.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans swapchain, image views, compatible render pass and framebuffers from live surface facts.
 *
 * When a nonempty @p existing_render_pass matches the newly selected swapchain format, it is
 * reused directly without creating a new render pass; otherwise a new clear-and-present pass is
 * created for the selected format. Predecessor swapchain handoff follows
 * reader_io_create_presentation_swapchain: pre-creation failures do not invoke
 * `vkCreateSwapchainKHR` or retire @p old_swapchain, while later failures abandon only newly
 * acquired child owners after creation has already retired @p old_swapchain. Keep the borrowed
 * API and serviced main context alive through cleanup, complete GPU work and release prior
 * framebuffers and views before retiring a swapchain, and avoid replaying a plan that holds a
 * retired @p old_swapchain handle.
 *
 * @tparam TEnvironment Execution capabilities needed to query and build presentation targets.
 * @param presentation_device Retained device, surface and selected presentation queues.
 * @param window Nonempty window owner associated with that surface.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @param existing_render_pass Optional prior clear-and-present pass on @p presentation_device
 * eligible for format reuse.
 * @return Reusable ReaderIO providing complete presentation targets, stop, or renderer error.
 */
template <detail::create_presentation_targets_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_targets(
    PresentationDevice presentation_device,
    sdl::window_ptr window,
    swapchain_ptr old_swapchain = {},
    CompatibleRenderPass existing_render_pass = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing validated surface, pixel and physical image-limit choices.
    auto prepared = detail::reader_io_prepare_presentation_swapchain<TEnvironment>(
        presentation_device, std::move(window));

    // Continuation assembling swapchain, images, views, compatible pass and target framebuffers.
    auto build_targets = std::bind_front(
        detail::reader_io_build_prepared_presentation_targets<TEnvironment>,
        std::move(presentation_device),
        std::move(old_swapchain),
        std::move(existing_render_pass));

    // ReaderIO assembling complete presentation targets or returning a renderer failure.
    return reader_io::and_then<"build_presentation_targets"_ss>(
        std::move(prepared), std::move(build_targets));
}
}  // namespace vulkandemo::vulkan
