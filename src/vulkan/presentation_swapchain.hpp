// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_swapchain.hpp
 *
 * Live initial swapchain setup for a presentation device.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/drawable_size.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./swapchain_support.hpp"
#include "detail/presentation_swapchain.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a first swapchain using live drawable pixels and current support facts.
 *
 * The window must be the Vulkan-capable window associated with the device's surface. The native
 * window must have no existing swapchain; release earlier successful owners before replaying.
 * Default format, presentation and image-count policies select ordinary per-image presentation.
 * Unavailable extents, unsupported policy and image limits report a renderer error. Cancellation
 * remains distinct. Surface changes can still cause creation to fail with its original status.
 * Synchronize host access to the surface throughout execution. The borrowed API and serviced
 * main context must outlive plans and owners; complete GPU work and child cleanup before release.
 *
 * @param presentation_device Retained device, surface and selected presentation queues.
 * @param window Nonempty window owner associated with that surface.
 * @return Reusable ReaderIO providing an owning presentation swapchain, stop, or renderer error.
 */
template <detail::create_presentation_swapchain_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_swapchain(
    PresentationDevice presentation_device, sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIOs providing surface support and live pixels while retaining their respective owners.
    auto support = reader_io_query_swapchain_support<TEnvironment>(
        presentation_device.physical_device, presentation_device.surface);
    auto drawable = sdl::reader_io_sdl_vulkan_get_drawable_size<TEnvironment>(std::move(window));
    auto snapshots = reader_io::sequence<"query_swapchain_inputs"_ss, TEnvironment>(
        std::move(support), std::move(drawable));

    // ReaderIO providing successful snapshots or the original support-query failure.
    auto inputs = reader_io::transform<"sequence_swapchain_input_results"_ss>(
        std::move(snapshots), detail::sequence_swapchain_input_results);

    // ReaderIO providing selected choices or the preparation's configuration failure.
    auto preparation =
        reader_io::transform<"prepare_swapchain"_ss>(std::move(inputs), detail::prepare_swapchain);

    // ReaderIO providing a configuration whose image parameters satisfy physical limits.
    auto validated = reader_io::and_then<"query_swapchain_image_limits"_ss>(
        std::move(preparation),
        std::bind_front(
            detail::reader_io_validate_swapchain_image_parameters<TEnvironment>,
            presentation_device.physical_device));

    // ReaderIO providing owned initial presentation resources from a fully validated preparation.
    return reader_io::and_then<"create_presentation_swapchain"_ss>(
        std::move(validated),
        std::bind_front(
            detail::reader_io_create_prepared_swapchain<TEnvironment>,
            std::move(presentation_device)));
}
}  // namespace vulkandemo::vulkan
