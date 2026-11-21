// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_device.hpp
 *
 * Composition of presentation device acquisition and queue retrieval.
 */
#pragma once

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"
#include "../device_queue.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../physical_device_selection.hpp"
#include "../physical_device_selection_types.hpp"
#include "../presentation_device_types.hpp"
#include "../queue_family_selection_types.hpp"
#include "../queue_types.hpp"
#include "../renderer_errors.hpp"
#include "./device.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to discover and create a presentation device with both queues. */
template <class TEnvironment>
concept create_presentation_device_environment =
    select_presentation_physical_device_environment<TEnvironment> &&
    vk_create_device_environment<TEnvironment> && vk_get_device_queue_environment<TEnvironment>;

/**
 * Provides unique device extension names including swapchain support.
 *
 * @param required_extension_names Additional required extension names.
 * @return Swapchain followed by distinct additional names in their supplied order.
 */
auto presentation_device_extension_names(immer::array<text::String> required_extension_names)
    -> immer::array<text::String>;

/**
 * Acquires a presentation device with its selected extension and feature enabled.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param selected Suitable physical device and retained parents.
 * @param required_extension_names Additional required device extensions.
 * @return A device owner or native creation failure.
 */
template <class TApi>
auto create_presentation_device(
    std::reference_wrapper<TApi> api,
    SelectedPresentationPhysicalDevice const & selected,
    immer::array<text::String> const & required_extension_names) -> vulkan_result<device_ptr>
{
    // Distinct swapchain, caller and selected maintenance names provide the native requests.
    auto extension_names =
        presentation_device_extension_names(required_extension_names).transient();
    text::String const & maintenance1_name = value_of(selected.maintenance1_extension);
    if (std::ranges::find(extension_names, maintenance1_name) == extension_names.end())
    {
        extension_names.push_back(maintenance1_name);
    }

    // The EXT spelling aliases the KHR feature, and its chain lives through the native call.
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance1_features{};
    maintenance1_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    maintenance1_features.swapchainMaintenance1 = VK_TRUE;
    return create_device_with_features(
        api,
        selected.instance,
        selected.physical_device,
        immer::array{
            selected.queue_families.graphics_family_index,
            selected.queue_families.present_family_index},
        std::move(extension_names).persistent(),
        &maintenance1_features);
}

/**
 * Supplies the API to deferred presentation-device acquisition.
 *
 * @tparam TEnvironment Execution environment type.
 * @param selected Suitable physical device and retained parents.
 * @param required_extension_names Additional required device extensions.
 * @param environment Execution capabilities.
 * @return IO providing an acquired device or native failure.
 */
template <class TEnvironment>
constexpr auto create_presentation_device_reader_io_action(
    SelectedPresentationPhysicalDevice selected,
    immer::array<text::String> required_extension_names,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing device acquisition with the selected feature and retained parents.
    auto create_device = std::bind_front(
        create_presentation_device<api_type>,
        environment.api,
        std::move(selected),
        std::move(required_extension_names));

    // IO providing native device acquisition with a call-local maintenance1 feature chain.
    return io::create<"create_presentation_device"_ss>(std::move(create_device));
}

/**
 * Retains the device and surface alongside their borrowed queue handles.
 *
 * @param surface Presentation surface owner.
 * @param physical_device Selected physical device.
 * @param queue_families Selected graphics and presentation families.
 * @param device Acquired device owner.
 * @param graphics_queue Borrowed graphics queue.
 * @param present_queue Borrowed presentation queue.
 * @return Owned presentation device context.
 */
constexpr auto make_presentation_device(
    surface_ptr surface,
    VkPhysicalDevice physical_device,
    PresentationQueueFamilies queue_families,
    device_ptr device,
    VkQueue graphics_queue,
    VkQueue present_queue) -> PresentationDevice
{
    return {
        std::move(surface),
        std::move(device),
        physical_device,
        queue_families,
        graphics_queue,
        present_queue};
}

/**
 * Retrieves the created device's graphics and presentation queues.
 *
 * @tparam TEnvironment Execution environment type.
 * @param surface Presentation surface owner.
 * @param physical_device Selected physical device.
 * @param queue_families Selected graphics and presentation families.
 * @param device Acquired device owner.
 * @return ReaderIO providing both queues and their retained owners.
 */
template <class TEnvironment>
constexpr auto reader_io_get_presentation_device_queues(
    surface_ptr surface,
    VkPhysicalDevice physical_device,
    PresentationQueueFamilies queue_families,
    device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing queue zero for both roles, collected in graphics/presentation order.
    auto retrieve_queues = reader_io::sequence<"get_presentation_device_queues"_ss, TEnvironment>(
        reader_io_vk_get_device_queue<TEnvironment>(
            device, queue_families.graphics_family_index, queue_index{0}),
        reader_io_vk_get_device_queue<TEnvironment>(
            device, queue_families.present_family_index, queue_index{0}));

    // Transformer providing the device and surface owners alongside both queue roles.
    auto retain_queues = std::bind_front(
        make_presentation_device,
        std::move(surface),
        physical_device,
        queue_families,
        std::move(device));

    // ReaderIO providing both queue roles with their retained owners.
    return reader_io::transform<"retain_presentation_device_queues"_ss>(
        std::move(retrieve_queues), std::move(retain_queues));
}

/**
 * Creates a maintenance1-enabled presentation device and retrieves both requested queues.
 *
 * @tparam TEnvironment Execution environment type.
 * @param extension_names Complete device extension names.
 * @param selected Suitable physical device with retained instance and surface.
 * @return ReaderIO providing the device context or creation failure.
 */
template <class TEnvironment>
constexpr auto reader_io_create_selected_presentation_device(
    immer::array<text::String> extension_names, SelectedPresentationPhysicalDevice selected)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Continuation providing both queue roles while retaining the selected surface and metadata.
    auto retrieve_queues = std::bind_front(
        reader_io_get_presentation_device_queues<TEnvironment>,
        selected.surface,
        selected.physical_device,
        selected.queue_families);

    // Action providing device acquisition with one queue per selected family and maintenance1
    // enabled.
    auto acquire_selected_device = std::bind_front(
        create_presentation_device_reader_io_action<TEnvironment>,
        std::move(selected),
        std::move(extension_names));
    auto create_device = reader_io::create<"create_selected_presentation_device"_ss, TEnvironment>(
        std::move(acquire_selected_device));

    // ReaderIO providing the device owner or a renderer-domain creation failure.
    auto acquire_device = reader_io::transform_error<"presentation_device_creation_error"_ss>(
        std::move(create_device), renderer_error_from_vulkan_failure);

    // ReaderIO providing a complete context, bypassing queue retrieval after creation failure.
    return reader_io::and_then<"get_created_presentation_device_queues"_ss>(
        std::move(acquire_device), std::move(retrieve_queues));
}
}  // namespace vulkandemo::vulkan::detail
