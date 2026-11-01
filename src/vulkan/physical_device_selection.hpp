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
#include "./managed_handles.hpp"
#include "./physical_device_selection_types.hpp"
#include "./physical_devices.hpp"
#include "./renderer_errors.hpp"
#include "./strong_types.hpp"
#include "detail/physical_device_selection.hpp"

namespace vulkandemo::vulkan
{
/** A physical device's original index and lower-is-better policy score. */
struct PhysicalDeviceScore
{
    /** Index in the original physical-device snapshot. */
    physical_device_index device_index;
    /** Policy cost; lower values are preferred. */
    physical_device_score score;
};

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
 * Discovers and selects a headless device satisfying queue and extension requirements.
 *
 * Device probes and their independent capability queries may overlap. All probes finish before
 * the first unsuccessful outcome in enumeration order is returned.
 * The execution API must outlive outstanding queries. The instance deleter's API must outlive
 * all retained instance owners.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Nonempty owner of a live parent instance.
 * @param required_queue_flags Required queue capabilities.
 * @param required_extension_names Required complete extension names.
 * @return Reusable ReaderIO returning a selection or discovery error.
 */
template <select_headless_physical_device_environment TEnvironment>
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

    // Error transformer providing renderer-domain context for enumeration failures.
    auto map_enumeration_error = [](VulkanFailure failure) -> renderer_error
    { return VulkanOperationFailure{failure, std::nullopt}; };

    // ReaderIO providing device handles or a renderer-domain enumeration failure.
    auto enumeration = reader_io::transform_error<"physical_device_enumeration_error"_ss>(
        std::move(enumerate_physical_devices), std::move(map_enumeration_error));

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
}  // namespace vulkandemo::vulkan
