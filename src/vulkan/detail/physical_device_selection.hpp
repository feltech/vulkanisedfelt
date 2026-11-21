// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/physical_device_selection.hpp
 *
 * Implementation of headless and presentation-aware physical-device discovery.
 */
#pragma once

// IWYU pragma: private, include "src/vulkan/physical_device_selection.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"
#include "../device_extensions.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../physical_device_properties.hpp"
#include "../physical_device_selection_types.hpp"
#include "../physical_devices.hpp"
#include "../physical_devices_types.hpp"
#include "../queue_family_properties.hpp"
#include "../queue_family_selection_types.hpp"
#include "../queue_types.hpp"
#include "../renderer_errors.hpp"
#include "../surface_support.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to discover a suitable headless physical device. */
template <class TEnvironment>
concept select_headless_physical_device_environment =
    vk_enumerate_physical_devices_environment<TEnvironment> &&
    vk_get_physical_device_properties_environment<TEnvironment> &&
    vk_get_physical_device_queue_family_properties_environment<TEnvironment> &&
    vk_enumerate_device_extension_properties_environment<TEnvironment>;

/** Commands needed to discover a graphics and presentation device. */
template <class TEnvironment>
concept select_presentation_physical_device_environment =
    select_headless_physical_device_environment<TEnvironment> &&
    vk_get_physical_device_surface_support_environment<TEnvironment>;

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

/** Owned presentation suitability facts for one enumerated device. */
struct PresentationDeviceAssessment
{
    /** Properties used for ranking. */
    VkPhysicalDeviceProperties properties;
    /** Suitable graphics and presentation families, if any. */
    std::optional<PresentationQueueFamilies> queue_families;
    /** Whether swapchain and additional required extensions are available. */
    bool has_required_extensions;
};
/** Outcome of assessing one presentation device. */
using presentation_device_assessment_result = vulkan_result<PresentationDeviceAssessment>;

/**
 * Sequences completed presentation assessments into successful values in enumeration order.
 *
 * @tparam TAssessment Owned suitability facts.
 * @param assessments Outcomes in original enumeration order.
 * @return Successful values, any stop, or the first query error.
 */
template <class TAssessment>
constexpr auto sequence_presentation_device_assessments(
    immer::array<vulkan_result<TAssessment>> const & assessments)
    -> renderer_result<immer::array<TAssessment>>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<immer::array<TAssessment>>;

    // Cancellation takes precedence over every completed query failure.
    if (std::ranges::any_of(
            assessments, [](auto const & assessment) { return result::is_stopped(assessment); }))
    {
        return result::stopped<result_type>();
    }
    auto failed_assessment = std::ranges::find_if(
        assessments, [](auto const & assessment) { return result::is_error(assessment); });
    if (failed_assessment != assessments.end())
    {
        return result::error<result_type>(
            renderer_error_from_vulkan_failure(result::get_error(*failed_assessment)));
    }

    // Only an entirely successful batch needs an owned array of suitability facts.
    auto values = immer::array<TAssessment>{}.transient();
    for (auto const & assessment : assessments)
    {
        values.push_back(result::get_success(assessment));
    }
    return result::success<result_type>(std::move(values).persistent());
}

/**
 * Ranks successful presentation assessments while retaining original enumeration indices.
 *
 * @param assessments Suitability facts in enumeration order.
 * @return First minimum-scored eligible index, or no eligible device.
 */
auto select_presentation_device_index(
    immer::array<PresentationDeviceAssessment> const & assessments)
    -> std::optional<physical_device_index>;

/** Successful independent presentation capability snapshots. */
using presentation_device_query_values = std::tuple<
    VkPhysicalDeviceProperties,
    immer::array<VkExtensionProperties>,
    std::optional<PresentationQueueFamilies>>;

/**
 * Sequences presentation query outcomes into successful snapshots.
 *
 * @param properties Device ranking properties.
 * @param extensions Extension-query outcome.
 * @param queues Presentation queue-selection outcome.
 * @return Owned snapshots, any stop, or the first query error.
 */
auto sequence_presentation_device_query_results(
    VkPhysicalDeviceProperties properties,
    vulkan_result<immer::array<VkExtensionProperties>> extensions,
    vulkan_result<std::optional<PresentationQueueFamilies>> queues)
    -> vulkan_result<presentation_device_query_values>;

/**
 * Selects queue families from successful support queries in snapshot order.
 *
 * @param families Complete queue-family snapshot.
 * @param support Query outcomes in matching family order.
 * @return Selected families, any stop, or the first support-query error.
 */
auto select_presentation_queues_from_support(
    immer::array<VkQueueFamilyProperties> const & families,
    immer::array<vulkan_result<bool>> const & support)
    -> vulkan_result<std::optional<PresentationQueueFamilies>> pre(
        families.size() == support.size());

/**
 * Assesses successful presentation capability snapshots.
 *
 * @param names Additional required extension names.
 * @param properties Device ranking properties.
 * @param extensions Available device extensions.
 * @param queues Suitable presentation queue families, if any.
 * @return Presentation suitability facts.
 */
auto assess_presentation_device_suitability(
    immer::array<text::String> const & names,
    VkPhysicalDeviceProperties const & properties,
    immer::array<VkExtensionProperties> extensions,
    std::optional<PresentationQueueFamilies> queues) -> PresentationDeviceAssessment;

/**
 * Selects a presentation device with its original enumeration index and parent owners.
 *
 * @param instance Retained instance.
 * @param surface Retained presentation surface.
 * @param handles Devices in enumeration order.
 * @param assessments Successful facts in matching enumeration order.
 * @return Owning selection or no-suitable-device error.
 */
auto select_presentation_device_from_assessments(
    instance_ptr const & instance,
    surface_ptr const & surface,
    immer::array<VkPhysicalDevice> const & handles,
    immer::array<PresentationDeviceAssessment> const & assessments)
    -> presentation_physical_device_selection_result
    pre(instance != nullptr && surface != nullptr && handles.size() == assessments.size());

/**
 * Collects surface support for every family in a complete snapshot.
 *
 * @tparam TEnvironment Execution environment type.
 * @param surface Retained presentation surface.
 * @param handle Borrowed physical device.
 * @param families Complete queue-family snapshot.
 * @return ReaderIO producing selected families or a support-query failure.
 */
template <class TEnvironment>
constexpr auto reader_io_resolve_presentation_queues(
    surface_ptr surface, VkPhysicalDevice handle, immer::array<VkQueueFamilyProperties> families)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Indices preserve alignment with the complete snapshot, including unusable families.
    auto indices = immer::array<queue_family_index>{}.transient();
    for (std::uint32_t index = 0; index < families.size(); ++index)
    {
        indices.push_back(queue_family_index{index});
    }

    // Continuation providing one independent query with an owning surface reference.
    auto query_surface_support = std::bind_back(
        reader_io_vk_get_physical_device_surface_support_khr<TEnvironment>, std::move(surface));
    auto query_support = std::bind_front(std::move(query_surface_support), handle);

    // ReaderIO providing all support outcomes in family order after every query completes.
    auto support = reader_io::traverse<"query_presentation_support"_ss, TEnvironment>(
        std::move(indices).persistent(), std::move(query_support));

    // ReaderIO providing suitable families, any stop, or the first support-query error.
    return reader_io::transform<"select_presentation_queues"_ss>(
        std::move(support),
        std::bind_front(select_presentation_queues_from_support, std::move(families)));
}

/**
 * Collects independent ranking, extension and surface-support assessments.
 *
 * @tparam TEnvironment Execution environment type.
 * @param surface Retained presentation surface.
 * @param handle Borrowed physical device.
 * @return ReaderIO providing successful capability snapshots or a query outcome.
 */
template <class TEnvironment>
constexpr auto reader_io_query_presentation_device(surface_ptr surface, VkPhysicalDevice handle)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the complete graphics and presentation family selection.
    auto queues = reader_io::and_then<"resolve_device_presentation_queues"_ss>(
        reader_io_vk_get_physical_device_queue_family_properties<TEnvironment>(handle),
        std::bind_front(
            reader_io_resolve_presentation_queues<TEnvironment>, std::move(surface), handle));

    // ReaderIO providing ranking properties and both fallible capability outcomes.
    auto capabilities =
        reader_io::sequence<"resolve_presentation_device_properties"_ss, TEnvironment>(
            reader_io_vk_get_physical_device_properties<TEnvironment>(handle),
            reader_io_vk_enumerate_device_extension_properties<TEnvironment>(handle),
            std::move(queues));

    // ReaderIO providing successful snapshots, any stop, or the first query error.
    return reader_io::transform<"sequence_presentation_device_query_results"_ss>(
        std::move(capabilities), sequence_presentation_device_query_results);
}

/**
 * Assesses presentation suitability from one complete query snapshot.
 *
 * @tparam TEnvironment Execution environment type.
 * @param surface Retained presentation surface.
 * @param names Additional required extension names.
 * @param handle Borrowed physical device.
 * @return ReaderIO producing presentation suitability facts.
 */
template <class TEnvironment>
constexpr auto reader_io_assess_presentation_device(
    surface_ptr surface, immer::array<text::String> names, VkPhysicalDevice handle)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the common ranking, extension and queue snapshots.
    auto query_values =
        reader_io_query_presentation_device<TEnvironment>(std::move(surface), handle);

    // ReaderIO providing suitability facts only for successful capability snapshots.
    return reader_io::transform<"assess_presentation_device"_ss>(
        std::move(query_values),
        std::bind_front(assess_presentation_device_suitability, std::move(names)));
}

/**
 * Collects presentation assessments and selects a suitable device.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Retained instance.
 * @param surface Retained presentation surface.
 * @param names Additional required extension names.
 * @param handles Enumerated physical devices.
 * @return ReaderIO producing an owning presentation device selection.
 */
template <class TEnvironment>
constexpr auto reader_io_select_from_presentation_devices(
    instance_ptr instance,
    surface_ptr surface,
    immer::array<text::String> names,
    immer::array<VkPhysicalDevice> handles)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Continuation providing a surface-aware assessment for each enumerated device.
    auto assess_device = std::bind_front(
        reader_io_assess_presentation_device<TEnvironment>, surface, std::move(names));

    // Transformer providing an owning selection from assessments in enumeration order.
    auto select_device = std::bind_front(
        select_presentation_device_from_assessments,
        std::move(instance),
        std::move(surface),
        handles);

    // ReaderIO providing every device assessment after all probes have finished.
    auto assessments = reader_io::traverse<"probe_presentation_devices"_ss, TEnvironment>(
        std::move(handles), std::move(assess_device));

    // ReaderIO providing plain facts only after every assessment succeeded.
    auto successful_assessments = reader_io::transform<"sequence_presentation_assessments"_ss>(
        std::move(assessments),
        sequence_presentation_device_assessments<PresentationDeviceAssessment>);

    // ReaderIO providing a suitable device or preserving unsuccessful discovery.
    return reader_io::transform<"select_presentation_physical_device"_ss>(
        std::move(successful_assessments), std::move(select_device));
}

/** Successful independent headless capability snapshots. */
using physical_device_query_values = std::tuple<
    VkPhysicalDeviceProperties,
    immer::array<VkQueueFamilyProperties>,
    immer::array<VkExtensionProperties>>;

/**
 * Sequences headless query outcomes into successful snapshots.
 *
 * @param properties Device ranking properties.
 * @param queues Available queue families.
 * @param extensions Extension-query outcome.
 * @return Owned snapshots or the unsuccessful extension-query outcome.
 */
auto sequence_physical_device_query_results(
    VkPhysicalDeviceProperties properties,
    immer::array<VkQueueFamilyProperties> queues,
    vulkan_result<immer::array<VkExtensionProperties>> extensions)
    -> vulkan_result<physical_device_query_values>;

/**
 * Checks successful headless capability snapshots.
 *
 * @param required_flags Required queue capabilities.
 * @param names Required complete extension names.
 * @param properties Device properties.
 * @param queues Available queue families.
 * @param extensions Available device extensions.
 * @return Headless suitability facts.
 */
auto assess_physical_device_suitability(
    VkQueueFlags required_flags,
    immer::array<text::String> const & names,
    VkPhysicalDeviceProperties const & properties,
    immer::array<VkQueueFamilyProperties> const & queues,
    immer::array<VkExtensionProperties> extensions) -> PhysicalDeviceAssessment;

/**
 * Selects a suitable candidate while preserving enumeration indices.
 *
 * @param instance Retained parent.
 * @param handles Handles in original enumeration order.
 * @param assessments Outcomes in original enumeration order.
 * @return Selection, any stop, or the first error in enumeration order.
 */
auto select_physical_device_from_assessments(
    instance_ptr const & instance,
    immer::array<VkPhysicalDevice> const & handles,
    immer::array<physical_device_assessment_result> assessments)
    -> physical_device_selection_result;

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

    // ReaderIO providing successful snapshots or the unsuccessful extension-query outcome.
    auto query_values = reader_io::transform<"sequence_physical_device_query_results"_ss>(
        std::move(resolve_physical_device_props), sequence_physical_device_query_results);

    // Transformer providing suitability facts from the collected query outcomes.
    auto assess_suitability =
        std::bind_front(assess_physical_device_suitability, flags, std::move(names));

    // ReaderIO providing suitability facts or an unsuccessful query outcome.
    return reader_io::transform<"assess_physical_device"_ss>(
        std::move(query_values), std::move(assess_suitability));
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

    // ReaderIO providing the final selection, any stop, or the first assessment error.
    return reader_io::transform<"select_headless_physical_device"_ss>(
        std::move(assess_physical_devices), std::move(select_headless_physical_device));
}
}  // namespace vulkandemo::vulkan::detail
