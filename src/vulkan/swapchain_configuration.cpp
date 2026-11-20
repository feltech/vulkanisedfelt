// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_configuration.hpp"

#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#include "./present_mode_selection.hpp"
#include "./surface_format_selection.hpp"
#include "./swapchain_configuration_types.hpp"
#include "./swapchain_extent_selection.hpp"
#include "./swapchain_image_count_selection.hpp"
#include "./swapchain_image_count_types.hpp"
#include "./swapchain_support_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <limits>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
auto select_swapchain_configuration(
    SwapchainSupport const & support,
    VkExtent2D drawable_extent,
    immer::array<VkFormat> const & acceptable_formats,
    immer::array<VkPresentModeKHR> const & preferred_present_modes,
    swapchain_image_count preferred_image_count) -> std::optional<SwapchainConfiguration>
{
    std::optional selected_surface_format{
        select_surface_format(support.formats, acceptable_formats)};
    std::optional selected_present_mode{
        select_present_mode(support.present_modes, preferred_present_modes)};
    std::optional selected_extent{select_swapchain_extent(support.capabilities, drawable_extent)};
    if (!selected_surface_format || !selected_present_mode || !selected_extent)
    {
        return std::nullopt;
    }
    return SwapchainConfiguration{
        *selected_surface_format,
        *selected_present_mode,
        *selected_extent,
        select_swapchain_image_count(support.capabilities, preferred_image_count)};
}

#ifndef DOCTEST_CONFIG_DISABLE
namespace
{
auto supported_swapchain() -> SwapchainSupport
{
    constexpr std::uint32_t variable_extent = std::numeric_limits<std::uint32_t>::max();
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.currentExtent = {variable_extent, variable_extent};
    capabilities.minImageExtent = {64, 64};
    capabilities.maxImageExtent = {1280, 720};
    capabilities.minImageCount = 1;
    capabilities.maxImageCount = 5;
    return {
        capabilities,
        {VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT},
         VkSurfaceFormatKHR{
             VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT}},
        {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR}};
}
}  // namespace

TEST_CASE("[pure] swapchain configuration joins supported default policy choices")
{
    SwapchainSupport support = supported_swapchain();

    std::optional selected_configuration{select_swapchain_configuration(support, {1920, 1080})};

    REQUIRE(selected_configuration.has_value());
    CHECK_EQ(selected_configuration->surface_format.format, VK_FORMAT_B8G8R8A8_UNORM);
    CHECK_EQ(
        selected_configuration->surface_format.colorSpace, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
    CHECK_EQ(selected_configuration->present_mode, VK_PRESENT_MODE_MAILBOX_KHR);
    CHECK_EQ(selected_configuration->extent.width, 1280U);
    CHECK_EQ(selected_configuration->extent.height, 720U);
    CHECK_EQ(selected_configuration->image_count, swapchain_image_count{2});
}

TEST_CASE("[pure] swapchain configuration passes custom choices through every policy")
{
    SwapchainSupport support = supported_swapchain();
    support.capabilities.currentExtent = {800, 600};

    std::optional selected_configuration{select_swapchain_configuration(
        support,
        {0, 0},
        {VK_FORMAT_R16G16B16A16_SFLOAT},
        {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR},
        swapchain_image_count{9})};

    REQUIRE(selected_configuration.has_value());
    CHECK_EQ(selected_configuration->surface_format.format, VK_FORMAT_R16G16B16A16_SFLOAT);
    CHECK_EQ(
        selected_configuration->surface_format.colorSpace, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
    CHECK_EQ(selected_configuration->present_mode, VK_PRESENT_MODE_IMMEDIATE_KHR);
    CHECK_EQ(selected_configuration->extent.width, 800U);
    CHECK_EQ(selected_configuration->extent.height, 600U);
    CHECK_EQ(selected_configuration->image_count, swapchain_image_count{5});
}

TEST_CASE("[pure] swapchain configuration remains unavailable when any required choice is missing")
{
    SwapchainSupport support = supported_swapchain();
    VkExtent2D drawable_extent{640, 480};
    SUBCASE("no surface formats")
    {
        support.formats = {};
    }
    SUBCASE("no acceptable format")
    {
        support.formats = {
            VkSurfaceFormatKHR{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    }
    SUBCASE("no presentation modes")
    {
        support.present_modes = {};
    }
    SUBCASE("no preferred presentation mode")
    {
        support.present_modes = {VK_PRESENT_MODE_IMMEDIATE_KHR};
    }
    SUBCASE("zero requested drawable size")
    {
        drawable_extent = {0, 480};
    }
    SUBCASE("zero fixed surface size")
    {
        support.capabilities.currentExtent = {640, 0};
    }

    CHECK_FALSE(select_swapchain_configuration(support, drawable_extent).has_value());
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan
