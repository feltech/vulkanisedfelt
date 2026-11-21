// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_device.hpp
 *
 * Deferred presentation device and queue setup.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../text/string.hpp"
#include "./managed_handles_types.hpp"
#include "./physical_device_selection.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_instance_types.hpp"
#include "detail/presentation_device.hpp"

namespace vulkandemo::vulkan
{
/**
 * Selects a presentation physical device and creates its logical device and queues.
 *
 * Requests queue zero at priority 1 in each selected family, the swapchain extension and all
 * additional required extensions, without optional features. Shared families provide the same
 * queue for both roles. Each execution creates an independent logical device. The returned
 * context retains its instance and surface. The borrowed API and serviced main-thread cleanup
 * context must outlive plans, operations and owners. Finish GPU work and release device children
 * before releasing the final device owner.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Parent instance and its actual enabled extension names.
 * @param surface Nonempty surface owner from that instance.
 * @param required_extension_names Additional required device extension names.
 * @return Reusable ReaderIO providing an owned device and both queues or renderer error.
 */
template <detail::create_presentation_device_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_device(
    PresentationInstance instance,
    surface_ptr surface,
    immer::array<text::String> required_extension_names = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Owned extension names required by both suitability checks and device creation.
    immer::array extension_names{
        detail::presentation_device_extension_names(std::move(required_extension_names))};

    // ReaderIO providing a suitable physical device and both selected queue families.
    auto select_physical_device = reader_io_select_presentation_physical_device<TEnvironment>(
        std::move(instance), std::move(surface), extension_names);

    // Continuation providing device creation with the same extension requirements.
    auto create_selected_device = std::bind_front(
        detail::reader_io_create_selected_presentation_device<TEnvironment>,
        std::move(extension_names));

    // ReaderIO providing a complete presentation context or the first unsuccessful outcome.
    return reader_io::and_then<"create_presentation_device"_ss>(
        std::move(select_physical_device), std::move(create_selected_device));
}
}  // namespace vulkandemo::vulkan
