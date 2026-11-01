// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/physical_device_selection.hpp
 *
 * Implementation of headless physical-device discovery.
 */
#pragma once

// IWYU pragma: private, include "src/vulkan/physical_device_selection.hpp"

#include <concepts>
#include <functional>
#include <optional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../../text/string.hpp"
#include "../device_extensions.hpp"
#include "../errors.hpp"
#include "../managed_handles.hpp"
#include "../physical_device_properties.hpp"
#include "../physical_device_selection_types.hpp"
#include "../physical_devices.hpp"
#include "../queue_family_properties.hpp"
#include "../strong_types.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to discover a suitable headless physical device. */
template <class TEnvironment>
concept select_headless_physical_device_environment =
    vk_enumerate_physical_devices_environment<TEnvironment> &&
    vk_get_physical_device_properties_environment<TEnvironment> &&
    vk_get_physical_device_queue_family_properties_environment<TEnvironment> &&
    vk_enumerate_device_extension_properties_environment<TEnvironment>;
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/// Owned suitability facts for one enumerated device.
struct PhysicalDeviceAssessment
{
    /// Properties used for ranking.
    VkPhysicalDeviceProperties properties;
    /// Suitable queue, if any.
    std::optional<vulkan::queue_family_index> queue_family_index;
    /// Whether all required extension names are available.
    bool has_required_extensions;
};
/// Outcome of assessing one device.
using physical_device_assessment_result = vulkan_result<PhysicalDeviceAssessment>;

/**
 * Checks queried capabilities, preserving extension-query errors and stops.
 *
 * @param required_flags Required queue capabilities.
 * @param names Required complete extension names.
 * @param properties Device properties.
 * @param queues Available queue families.
 * @param extensions Extension-query outcome.
 * @return Suitability facts or the unsuccessful query outcome.
 */
auto assess_physical_device_suitability(
    VkQueueFlags required_flags,
    immer::array<text::String> const & names,
    VkPhysicalDeviceProperties const & properties,
    immer::array<VkQueueFamilyProperties> const & queues,
    vulkan_result<immer::array<VkExtensionProperties>> extensions)
    -> physical_device_assessment_result;

/**
 * Selects a suitable candidate while preserving enumeration indices.
 *
 * @param instance Retained parent.
 * @param handles Handles in original enumeration order.
 * @param probes Probe outcomes in original enumeration order.
 * @return Selection or first unsuccessful outcome in enumeration order.
 */
auto select_physical_device_from_assessments(
    instance_ptr const & instance,
    immer::array<VkPhysicalDevice> const & handles,
    immer::array<physical_device_assessment_result> probes) -> physical_device_selection_result;

/**
 * Collects independent queries for one device and assesses its suitability.
 *
 * @tparam TEnvironment Execution environment type.
 * @param flags Required queue capabilities.
 * @param names Owned required extension names.
 * @param handle Borrowed physical device.
 * @return ReaderIO producing suitability facts or a query failure.
 */
template <class TEnvironment>
constexpr auto reader_io_assess_physical_device(
    VkQueueFlags flags, immer::array<text::String> names, VkPhysicalDevice handle)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the device's properties, queue families, and extension-query outcome.
    auto resolve_physical_device_props =
        reader_io::sequence<"resolve_physical_device_props"_ss, TEnvironment>(
            reader_io_vk_get_physical_device_properties<TEnvironment>(handle),
            reader_io_vk_get_physical_device_queue_family_properties<TEnvironment>(handle),
            reader_io_vk_enumerate_device_extension_properties<TEnvironment>(handle));

    // Transformer providing suitability facts from the collected query outcomes.
    auto assess_suitability =
        std::bind_front(assess_physical_device_suitability, flags, std::move(names));

    // ReaderIO providing suitability facts or an unsuccessful query outcome.
    return reader_io::transform<"assess_physical_device"_ss>(
        std::move(resolve_physical_device_props), std::move(assess_suitability));
}

/**
 * Collects device assessments and selects a suitable device.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Retained parent.
 * @param flags Required queue capabilities.
 * @param names Owned required extension names.
 * @param handles Enumerated physical devices.
 * @return ReaderIO producing the final selection.
 */
template <class TEnvironment>
constexpr auto reader_io_select_from_enumerated_devices(
    instance_ptr instance,
    VkQueueFlags flags,
    immer::array<text::String> names,
    immer::array<VkPhysicalDevice> handles)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Continuation providing an assessment ReaderIO for one physical device.
    auto assess_physical_device =
        std::bind_front(reader_io_assess_physical_device<TEnvironment>, flags, std::move(names));

    // Transformer providing a suitable selection from the device assessments.
    auto select_headless_physical_device =
        std::bind_front(select_physical_device_from_assessments, std::move(instance), handles);

    // ReaderIO providing all device assessments in enumeration order.
    auto assess_physical_devices = reader_io::traverse<"probe_physical_devices"_ss, TEnvironment>(
        std::move(handles), std::move(assess_physical_device));

    // ReaderIO providing the final selection or the first unsuccessful assessment.
    return reader_io::transform<"select_headless_physical_device"_ss>(
        std::move(assess_physical_devices), std::move(select_headless_physical_device));
}
}  // namespace vulkandemo::vulkan::detail
