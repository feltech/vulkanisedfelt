// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_swapchain.hpp"

#include <cstdint>
#include <optional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>

#include "../present_mode_selection.hpp"
#include "../surface_format_selection.hpp"
#include "../swapchain_extent_selection.hpp"
#include "../swapchain_image_count_selection.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <limits>
#include <variant>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_swapchain_input_results(
    vulkan_result<SwapchainSupport> support, sdl::DrawableSize drawable)
    -> renderer_result<swapchain_input_values>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<swapchain_input_values>;
    if (result::is_stopped(support))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(support))
    {
        return result::error<result_type>(
            renderer_error_from_vulkan_failure(result::get_error(support)));
    }
    return result::success<result_type>(result::get_success(std::move(support)), drawable);
}

auto prepare_swapchain(SwapchainSupport facts, sdl::DrawableSize drawable)
    -> renderer_result<PreparedSwapchain>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<PreparedSwapchain>;
    VkExtent2D drawable_extent{
        static_cast<std::uint32_t>(value_of(drawable.width)),
        static_cast<std::uint32_t>(value_of(drawable.height))};

    std::optional selected_extent{select_swapchain_extent(facts.capabilities, drawable_extent)};
    if (!selected_extent)
    {
        return result::error<result_type>(
            NoSuitableSwapchainConfiguration{SwapchainConfigurationFailure::unavailable_extent});
    }
    std::optional selected_surface_format{select_surface_format(facts.formats)};
    if (!selected_surface_format)
    {
        return result::error<result_type>(NoSuitableSwapchainConfiguration{
            SwapchainConfigurationFailure::no_acceptable_surface_format});
    }
    std::optional selected_present_mode{select_present_mode(facts.present_modes)};
    if (!selected_present_mode)
    {
        return result::error<result_type>(NoSuitableSwapchainConfiguration{
            SwapchainConfigurationFailure::no_preferred_present_mode});
    }
    SwapchainConfiguration configuration{
        *selected_surface_format,
        *selected_present_mode,
        *selected_extent,
        select_swapchain_image_count(facts.capabilities)};
    return result::success<result_type>(PreparedSwapchain{configuration, facts.capabilities});
}

auto swapchain_image_query_error(VulkanFailure failure) -> renderer_error
{
    if (failure.result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        return NoSuitableSwapchainConfiguration{
            SwapchainConfigurationFailure::unsupported_image_parameters};
    }
    return renderer_error_from_vulkan_failure(failure);
}

auto validate_swapchain_image_parameters(
    PreparedSwapchain prepared, VkImageFormatProperties properties)
    -> renderer_result<PreparedSwapchain> pre(properties.maxExtent.depth >= 1)
        pre(properties.maxMipLevels >= 1) pre(properties.maxArrayLayers >= 1)
            pre((properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0)
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<PreparedSwapchain>;
    VkExtent2D extent = prepared.configuration.extent;
    // Successful queries guarantee nonzero dimensions/layers/mips and single-sample UNORM color
    // attachments. The selected surface extent must still fit this format's width and height.
    if (extent.width > properties.maxExtent.width || extent.height > properties.maxExtent.height)
    {
        return result::error<result_type>(NoSuitableSwapchainConfiguration{
            SwapchainConfigurationFailure::unsupported_image_parameters});
    }
    return result::success<result_type>(prepared);
}

auto make_presentation_swapchain(
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    swapchain_ptr swapchain) -> PresentationSwapchain
{
    return PresentationSwapchain{
        std::move(presentation_device), configuration, std::move(swapchain)};
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
namespace
{
auto drawable_support() -> SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 1;
    capabilities.maxImageCount = 4;
    capabilities.currentExtent = {
        std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()};
    capabilities.minImageExtent = {1, 1};
    capabilities.maxImageExtent = {4096, 4096};
    capabilities.maxImageArrayLayers = 1;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return SwapchainSupport{
        capabilities,
        {{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
        {VK_PRESENT_MODE_FIFO_KHR}};
}

auto image_limits() -> VkImageFormatProperties
{
    return VkImageFormatProperties{{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1048576};
}
}  // namespace

TEST_CASE("[pure] swapchain input sequencing preserves unsuccessful support outcomes")
{
    using support_result = vulkan_result<SwapchainSupport>;
    namespace result = fnfelt::result;
    sdl::DrawableSize drawable{sdl::drawable_width{800}, sdl::drawable_height{600}};
    auto stopped =
        detail::sequence_swapchain_input_results(result::stopped<support_result>(), drawable);
    auto errored = detail::sequence_swapchain_input_results(
        result::error<support_result>(VulkanFailure{
            FallibleVulkanOperation::get_physical_device_surface_capabilities,
            VK_ERROR_SURFACE_LOST_KHR}),
        drawable);

    CHECK(result::is_stopped(stopped));
    REQUIRE(result::is_error(errored));
    CHECK_EQ(
        std::get<VulkanOperationFailure>(result::get_error(errored)).cause.result,
        VK_ERROR_SURFACE_LOST_KHR);
}

TEST_CASE("[pure] swapchain preparation distinguishes unavailable extents from missing choices")
{
    SwapchainSupport support = drawable_support();
    sdl::DrawableSize drawable{sdl::drawable_width{800}, sdl::drawable_height{600}};
    bool should_succeed = false;
    SwapchainConfigurationFailure reason =
        SwapchainConfigurationFailure::no_acceptable_surface_format;
    SUBCASE("minimized surface takes precedence over missing format and mode")
    {
        drawable.width = sdl::drawable_width{0};
        support.formats = {};
        support.present_modes = {};
        reason = SwapchainConfigurationFailure::unavailable_extent;
    }
    SUBCASE("zero fixed extent")
    {
        support.capabilities.currentExtent = {640, 0};
        reason = SwapchainConfigurationFailure::unavailable_extent;
    }
    SUBCASE("no acceptable format")
    {
        support.formats = {};
    }
    SUBCASE("no preferred mode")
    {
        support.present_modes = {VK_PRESENT_MODE_IMMEDIATE_KHR};
        reason = SwapchainConfigurationFailure::no_preferred_present_mode;
    }
    SUBCASE("fixed extent remains authoritative for zero drawable")
    {
        drawable.width = sdl::drawable_width{0};
        support.capabilities.currentExtent = {640, 480};
        should_succeed = true;
    }

    renderer_result<detail::PreparedSwapchain> outcome =
        detail::prepare_swapchain(support, drawable);

    if (should_succeed)
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        CHECK_EQ(fnfelt::result::get_success(outcome).configuration.extent.width, 640U);
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK(
            std::get<NoSuitableSwapchainConfiguration>(fnfelt::result::get_error(outcome)).reason ==
            reason);
    }
}

TEST_CASE("[pure] swapchain image validation rejects extents beyond physical format limits")
{
    auto preparation = detail::prepare_swapchain(
        drawable_support(), sdl::DrawableSize{sdl::drawable_width{800}, sdl::drawable_height{600}});
    REQUIRE(fnfelt::result::is_success(preparation));
    VkImageFormatProperties properties = image_limits();
    bool should_succeed = false;
    SUBCASE("supported")
    {
        should_succeed = true;
    }
    SUBCASE("width exceeds limit")
    {
        properties.maxExtent.width = 799;
    }
    SUBCASE("height exceeds limit")
    {
        properties.maxExtent.height = 599;
    }
    auto outcome = detail::validate_swapchain_image_parameters(
        fnfelt::result::get_success(preparation), properties);

    CHECK_EQ(fnfelt::result::is_success(outcome), should_succeed);
    if (!should_succeed)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK(
            std::get<NoSuitableSwapchainConfiguration>(fnfelt::result::get_error(outcome)).reason ==
            SwapchainConfigurationFailure::unsupported_image_parameters);
    }
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
