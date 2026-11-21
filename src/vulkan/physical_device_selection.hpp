// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/physical_device_selection.hpp
 *
 * Physical-device ranking, discovery and selection.
 */
#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../text/string.hpp"
#include "./errors.hpp"
#include "./managed_handles_types.hpp"
#include "./physical_device_selection_types.hpp"
#include "./physical_devices.hpp"
#include "./physical_devices_types.hpp"
#include "./presentation_instance_types.hpp"
#include "./renderer_errors.hpp"
#include "detail/physical_device_selection.hpp"

namespace vulkandemo::vulkan
{
/**
 * Ranks discrete GPUs ahead of other physical-device types.
 *
 * Entries retain their original indices and snapshot order. Ranking does not establish suitability.
 *
 * @param properties Physical-device property snapshots.
 * @return Scores of zero for discrete GPUs and one for every other type.
 */
[[nodiscard]] auto score_physical_devices(
    immer::array<VkPhysicalDeviceProperties> const & properties)
    -> immer::array<PhysicalDeviceScore>;

/**
 * Selects the first entry with the lowest supplied score.
 *
 * @param scores Physical-device policy scores.
 * @return Original device index, or an empty value when no scores are supplied.
 */
[[nodiscard]] auto select_physical_device(immer::array<PhysicalDeviceScore> const & scores)
    -> std::optional<physical_device_index>;

/**
 * Discovers and selects a Vulkan 1.1 device satisfying queue and extension requirements.
 *
 * Device assessments and their independent capability queries may overlap. All assessments finish
 * before any stop takes precedence over errors; otherwise the first error in enumeration order
 * wins. The execution API must outlive outstanding queries. The instance deleter's API must outlive
 * all retained instance owners.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Nonempty owner of a live parent instance.
 * @param required_queue_flags Required queue capabilities.
 * @param required_extension_names Required complete extension names.
 * @return Reusable ReaderIO returning a selection or discovery error.
 */
template <detail::select_headless_physical_device_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_select_headless_physical_device(
    instance_ptr instance,
    VkQueueFlags required_queue_flags,
    immer::array<text::String> required_extension_names = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the physical devices belonging to the retained instance.
    auto enumerate_physical_devices =
        reader_io_vk_enumerate_physical_devices<TEnvironment>(instance.get());

    // ReaderIO providing device handles or a renderer-domain enumeration failure.
    auto enumeration = reader_io::transform_error<"physical_device_enumeration_error"_ss>(
        std::move(enumerate_physical_devices), renderer_error_from_vulkan_failure);

    // Continuation providing a suitable-device selection ReaderIO for the enumerated handles.
    auto select_headless_physical_device = std::bind_front(
        detail::reader_io_select_from_enumerated_devices<TEnvironment>,
        std::move(instance),
        required_queue_flags,
        std::move(required_extension_names));

    // ReaderIO providing the best suitable device or a discovery failure.
    return reader_io::and_then<"discover_headless_physical_device"_ss>(
        std::move(enumeration), std::move(select_headless_physical_device));
}

/**
 * Selects a Vulkan 1.1 presentation device with an enabled-instance maintenance1 path.
 *
 * The surface and instance must share a parent. The selected extension is supported but remains
 * disabled until logical-device creation explicitly enables the extension and feature. Every
 * assessment completes before cancellation takes precedence over errors; otherwise the first
 * query error in enumeration order wins. APIs and cleanup contexts must outlive retained owners.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Parent owner and the extensions actually enabled on it.
 * @param surface Nonempty surface belonging to that instance.
 * @param required_extension_names Additional required complete device extension names.
 * @return Reusable ReaderIO providing an owning required-capability selection or discovery error.
 */
template <detail::select_presentation_physical_device_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_select_presentation_physical_device(
    PresentationInstance instance,
    surface_ptr surface,
    immer::array<text::String> required_extension_names = {})
{
    // GCC currently rejects preconditions on this deduced-return factory.
    contract_assert(instance.instance != nullptr && surface != nullptr);

    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the retained instance's handles or a renderer enumeration failure.
    auto enumeration = reader_io::transform_error<"presentation_device_enumeration_error"_ss>(
        reader_io_vk_enumerate_physical_devices<TEnvironment>(instance.instance.get()),
        renderer_error_from_vulkan_failure);

    // Continuation providing assessments using the instance's canonical enabled extension names.
    auto select_device = std::bind_front(
        detail::reader_io_select_from_presentation_devices<TEnvironment>,
        std::move(instance),
        std::move(surface),
        std::move(required_extension_names));

    // ReaderIO providing the selected device, parents and required maintenance1 path.
    return reader_io::and_then<"discover_presentation_physical_device"_ss>(
        std::move(enumeration), std::move(select_device));
}
}  // namespace vulkandemo::vulkan
