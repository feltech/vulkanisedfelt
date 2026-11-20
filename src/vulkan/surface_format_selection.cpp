// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./surface_format_selection.hpp"

#include <algorithm>
#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
auto select_surface_format(
    immer::array<VkSurfaceFormatKHR> const & supported_formats,
    immer::array<VkFormat> const & acceptable_formats) -> std::optional<VkSurfaceFormatKHR>
{
    auto selected_surface_format = std::ranges::find_if(
        supported_formats,
        [&](VkSurfaceFormatKHR surface_format)
        {
            return surface_format.format != VK_FORMAT_UNDEFINED &&
                std::ranges::contains(acceptable_formats, surface_format.format);
        });
    if (selected_surface_format == supported_formats.end())
    {
        return std::nullopt;
    }
    return *selected_surface_format;
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[pure] surface format selection preserves advertised order and color space")
{
    immer::array<VkSurfaceFormatKHR> supported_formats{
        VkSurfaceFormatKHR{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT},
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT},
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

    std::optional selected_surface_format{select_surface_format(supported_formats)};

    REQUIRE(selected_surface_format.has_value());
    CHECK_EQ(selected_surface_format->format, VK_FORMAT_B8G8R8A8_UNORM);
    CHECK_EQ(selected_surface_format->colorSpace, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
}

TEST_CASE("[pure] surface format selection accepts a custom format allowlist")
{
    immer::array<VkSurfaceFormatKHR> supported_formats{
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT}};

    std::optional selected_surface_format{
        select_surface_format(supported_formats, {VK_FORMAT_R16G16B16A16_SFLOAT})};

    REQUIRE(selected_surface_format.has_value());
    CHECK_EQ(selected_surface_format->format, VK_FORMAT_R16G16B16A16_SFLOAT);
    CHECK_EQ(selected_surface_format->colorSpace, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
}

TEST_CASE("[pure] surface format selection reports no acceptable advertised pair")
{
    immer::array<VkSurfaceFormatKHR> supported_formats{
        VkSurfaceFormatKHR{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

    CHECK_FALSE(select_surface_format({}).has_value());
    CHECK_FALSE(select_surface_format(supported_formats).has_value());
    CHECK_FALSE(select_surface_format(supported_formats, {}).has_value());
    CHECK_FALSE(select_surface_format(
                    {VkSurfaceFormatKHR{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}})
                    .has_value());
    CHECK_FALSE(select_surface_format(
                    {VkSurfaceFormatKHR{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
                    {VK_FORMAT_UNDEFINED})
                    .has_value());
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan
