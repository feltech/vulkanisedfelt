// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_swapchain.hpp
 *
 * Preparation and acquisition of presentation swapchains.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>

#include "../../sdl/detail/drawable_size.hpp"
#include "../../sdl/drawable_size.hpp"
#include "../../sdl/drawable_size_types.hpp"
#include "../../sdl/managed_handles_types.hpp"
#include "../image_format_properties.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_device_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../renderer_errors.hpp"
#include "../swapchain.hpp"
#include "../swapchain_support.hpp"
#include "../swapchain_support_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Current surface capabilities paired with renderer-selected image choices. */
struct PreparedSwapchain
{
    /** Selected ordinary image and presentation choices. */
    SwapchainConfiguration configuration;
    /** Surface facts used to validate and create the swapchain. */
    VkSurfaceCapabilitiesKHR capabilities;
};

/** Capabilities needed to query and acquire a presentation swapchain. */
template <class TEnvironment>
concept create_presentation_swapchain_environment =
    query_swapchain_support_environment<TEnvironment> &&
    sdl::detail::sdl_vulkan_get_drawable_size_environment<TEnvironment> &&
    vk_get_physical_device_image_format_properties_environment<TEnvironment> &&
    vk_create_swapchain_environment<TEnvironment>;

/** Successful surface support and live drawable snapshots. */
using swapchain_input_values = std::tuple<SwapchainSupport, sdl::DrawableSize>;

/**
 * Sequences swapchain inputs while preserving unsuccessful support-query outcomes.
 *
 * @param support Support-query outcome.
 * @param drawable Live drawable dimensions.
 * @return Successful snapshots or the unsuccessful support-query outcome.
 */
auto sequence_swapchain_input_results(
    vulkan_result<SwapchainSupport> support, sdl::DrawableSize drawable)
    -> renderer_result<swapchain_input_values>;

/**
 * Selects renderer choices or reports the first unavailable requirement.
 *
 * @param support Successful surface facts.
 * @param drawable Live drawable dimensions.
 * @return Prepared choices or a configuration failure.
 */
auto prepare_swapchain(SwapchainSupport support, sdl::DrawableSize drawable)
    -> renderer_result<PreparedSwapchain>;

/**
 * Translates image-format rejection into a renderer configuration failure.
 *
 * @param failure Original image query failure.
 * @return Configuration failure or the original operational failure.
 */
auto swapchain_image_query_error(VulkanFailure failure) -> renderer_error;

/**
 * Checks that the selected one-layer, one-mip, one-sample image fits the physical limits.
 *
 * @param prepared Selected configuration and surface facts.
 * @param properties Successful 2D optimal color-attachment limits for the selected UNORM format.
 * @return Validated preparation or an unsupported-image failure.
 */
auto validate_swapchain_image_parameters(
    PreparedSwapchain prepared, VkImageFormatProperties properties)
    -> renderer_result<PreparedSwapchain> pre(properties.maxExtent.depth >= 1)
        pre(properties.maxMipLevels >= 1) pre(properties.maxArrayLayers >= 1)
            pre((properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0);

/**
 * Validates prepared image parameters while keeping physical device parents alive.
 *
 * @param retained_device Device retaining the instance during physical queries.
 * @param prepared Selected configuration and surface facts.
 * @param properties Successful 2D optimal color-attachment limits for the selected UNORM format.
 * @return Validated preparation or an unsupported-image failure.
 */
auto validate_prepared_swapchain_image_parameters(
    device_ptr retained_device, PreparedSwapchain prepared, VkImageFormatProperties properties)
    -> renderer_result<PreparedSwapchain> pre(retained_device != nullptr)
        pre(properties.maxExtent.depth >= 1) pre(properties.maxMipLevels >= 1)
            pre(properties.maxArrayLayers >= 1)
                pre((properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0);

/**
 * Joins acquired ownership with the selected context.
 *
 * @param presentation_device Retained device, surface and queues.
 * @param configuration Acquired swapchain configuration.
 * @param swapchain Acquired swapchain ownership.
 * @return Owning presentation context.
 */
auto make_presentation_swapchain(
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    swapchain_ptr swapchain) -> PresentationSwapchain;

/**
 * Provides validated image parameters for a prepared swapchain.
 *
 * @param device Retained logical device keeping instance and physical device alive.
 * @param physical_device Borrowed physical device.
 * @param prepared Selected configuration and surface facts.
 * @return ReaderIO providing validated image choices or an unsuccessful query/validation outcome.
 */
template <class TEnvironment>
constexpr auto reader_io_validate_swapchain_image_parameters(
    device_ptr device, VkPhysicalDevice physical_device, PreparedSwapchain prepared)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Surface capabilities bound the drawable area; VUID-VkSwapchainCreateInfoKHR-imageFormat-01778
    // additionally requires the chosen image description to fit physical image-format limits.
    auto query = reader_io_vk_get_physical_device_image_format_properties<TEnvironment>(
        physical_device,
        prepared.configuration.surface_format.format,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0);

    // ReaderIO distinguishing unsupported images from operational query failures.
    auto query_with_renderer_errors = reader_io::transform_error<"swapchain_image_query_error"_ss>(
        std::move(query), swapchain_image_query_error);

    // ReaderIO providing validated choices while retaining the parent device through validation.
    return reader_io::transform<"validate_swapchain_image_parameters"_ss>(
        std::move(query_with_renderer_errors),
        std::bind_front(validate_prepared_swapchain_image_parameters, std::move(device), prepared));
}

/**
 * Plans support discovery, configuration selection and image-limit validation.
 *
 * @param presentation_device Retained device, surface and physical device handles.
 * @param window Nonempty window owner associated with that surface.
 * @return ReaderIO providing validated swapchain preparation or a renderer error.
 */
template <class TEnvironment>
constexpr auto reader_io_prepare_presentation_swapchain(
    PresentationDevice presentation_device, sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIOs providing surface support and live pixels while retaining their respective owners.
    auto support = reader_io_query_swapchain_support<TEnvironment>(
        presentation_device.physical_device, std::move(presentation_device.surface));
    auto drawable = sdl::reader_io_sdl_vulkan_get_drawable_size<TEnvironment>(std::move(window));
    auto snapshots = reader_io::sequence<"query_swapchain_inputs"_ss, TEnvironment>(
        std::move(support), std::move(drawable));

    // ReaderIO providing successful snapshots or the original support-query failure.
    auto inputs = reader_io::transform<"sequence_swapchain_input_results"_ss>(
        std::move(snapshots), sequence_swapchain_input_results);

    // ReaderIO providing selected choices or the preparation's configuration failure.
    auto preparation =
        reader_io::transform<"prepare_swapchain"_ss>(std::move(inputs), prepare_swapchain);

    // ReaderIO providing a configuration whose image parameters satisfy physical limits.
    return reader_io::and_then<"query_swapchain_image_limits"_ss>(
        std::move(preparation),
        std::bind_front(
            reader_io_validate_swapchain_image_parameters<TEnvironment>,
            std::move(presentation_device.device),
            presentation_device.physical_device));
}

/**
 * Provides owned presentation context from a validated preparation.
 *
 * @param presentation_device Retained device, surface and queues.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @param prepared Validated image choices and surface facts.
 * @return ReaderIO providing owned presentation resources or the original creation failure.
 */
template <class TEnvironment>
constexpr auto reader_io_create_prepared_swapchain(
    PresentationDevice presentation_device, swapchain_ptr old_swapchain, PreparedSwapchain prepared)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the native swapchain owner with parents and optional predecessor handoff.
    auto create = reader_io_vk_create_swapchain_khr<TEnvironment>(
        presentation_device,
        prepared.configuration,
        prepared.capabilities,
        std::move(old_swapchain));

    // ReaderIO providing creation failures in the shared renderer error domain.
    auto create_with_renderer_errors = reader_io::transform_error<"swapchain_creation_error"_ss>(
        std::move(create), renderer_error_from_vulkan_failure);

    // ReaderIO providing the selected configuration and queue context with successful ownership.
    return reader_io::transform<"make_presentation_swapchain"_ss>(
        std::move(create_with_renderer_errors),
        std::bind_front(
            make_presentation_swapchain, std::move(presentation_device), prepared.configuration));
}
}  // namespace vulkandemo::vulkan::detail
