// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_maintenance1_support.hpp
 *
 * Owned-instance composition of maintenance1 capability observations.
 */
#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../errors.hpp"
#include "../extension_types.hpp"
#include "../presentation_instance_types.hpp"
#include "../swapchain_maintenance1_selection.hpp"
#include "./device_extensions.hpp"
#include "./swapchain_maintenance1_features.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to discover device maintenance1 support. */
template <class TEnvironment>
concept discover_swapchain_maintenance1_support_environment =
    vk_enumerate_device_extension_properties_environment<TEnvironment> &&
    query_swapchain_maintenance1_feature_environment<TEnvironment>;

/**
 * Resolves an extension candidate against its device feature support.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param instance Parent instance and its enabled extension names.
 * @param physical_device Borrowed device belonging to the instance.
 * @param device_extensions Advertised device extension properties.
 * @return Supported device-extension name, or successful absence of support.
 */
template <class TApi>
auto discover_swapchain_maintenance1_support(
    std::reference_wrapper<TApi> api,
    PresentationInstance instance,
    VkPhysicalDevice physical_device,
    immer::array<VkExtensionProperties> device_extensions)
    -> vulkan_result<std::optional<extension_name>>
{
    using result_type = vulkan_result<std::optional<extension_name>>;
    std::optional candidate = select_swapchain_maintenance1_extension(
        std::move(instance.enabled_extensions), std::move(device_extensions));
    if (!candidate.has_value())
    {
        return fnfelt::result::success<result_type>(std::nullopt);
    }

    // The by-value instance keeps the borrowed device alive through the synchronous native query.
    bool const has_feature = query_swapchain_maintenance1_feature(api, physical_device);
    return fnfelt::result::success<result_type>(has_feature ? std::move(candidate) : std::nullopt);
}

/**
 * Supplies execution capabilities to an owned-instance feature observation.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param instance Parent instance and enabled names.
 * @param physical_device Borrowed device belonging to the instance.
 * @param device_extensions Advertised device extension properties.
 * @param environment Borrowed command implementation.
 * @return IO providing supported extension eligibility or successful absence.
 */
template <class TEnvironment>
constexpr auto discover_swapchain_maintenance1_support_reader_io_action(
    PresentationInstance instance,
    VkPhysicalDevice physical_device,
    immer::array<VkExtensionProperties> device_extensions,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable retaining the instance while selecting and querying an eligible device path.
    auto query = std::bind_front(
        discover_swapchain_maintenance1_support<api_type>,
        environment.api,
        std::move(instance),
        physical_device,
        std::move(device_extensions));

    // IO providing a supported path without retaining pointers into the local feature chain.
    return io::create<"discover_swapchain_maintenance1_support"_ss>(std::move(query));
}

/**
 * Plans the owned-instance feature observation following extension enumeration.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param instance Parent instance and enabled names.
 * @param physical_device Borrowed device belonging to the instance.
 * @param device_extensions Advertised device extension properties.
 * @return ReaderIO providing device eligibility and retaining its parent through execution.
 */
template <class TEnvironment>
constexpr auto reader_io_discover_swapchain_maintenance1_feature_support(
    PresentationInstance instance,
    VkPhysicalDevice physical_device,
    immer::array<VkExtensionProperties> device_extensions)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing an owned-instance IO after the execution API is supplied.
    auto query = std::bind_front(
        discover_swapchain_maintenance1_support_reader_io_action<TEnvironment>,
        std::move(instance),
        physical_device,
        std::move(device_extensions));

    // ReaderIO providing a supported extension name or an ordinary unsupported outcome.
    return reader_io::create<"discover_swapchain_maintenance1_feature_support"_ss, TEnvironment>(
        std::move(query));
}
}  // namespace vulkandemo::vulkan::detail
