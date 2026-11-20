// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_extent_selection.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <limits>
#include <optional>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>

namespace vulkandemo::vulkan
{
namespace
{
constexpr auto variable_surface_extent() -> VkSurfaceCapabilitiesKHR
{
    VkSurfaceCapabilitiesKHR capabilities{};
    constexpr std::uint32_t variable_extent = std::numeric_limits<std::uint32_t>::max();
    capabilities.currentExtent = {variable_extent, variable_extent};
    capabilities.minImageExtent = {64, 32};
    capabilities.maxImageExtent = {1024, 768};
    return capabilities;
}

static_assert(select_swapchain_extent(variable_surface_extent(), {1, 2000})->width == 64);
static_assert(select_swapchain_extent(variable_surface_extent(), {1, 2000})->height == 768);
static_assert(!select_swapchain_extent(variable_surface_extent(), {0, 2000}).has_value());
}  // namespace

TEST_CASE("[pure] swapchain extent selection preserves the fixed surface extent")
{
    VkSurfaceCapabilitiesKHR capabilities = variable_surface_extent();
    capabilities.currentExtent = {800, 600};

    std::optional selected_extent{select_swapchain_extent(capabilities, {0, 0})};

    REQUIRE(selected_extent.has_value());
    CHECK_EQ(selected_extent->width, 800U);
    CHECK_EQ(selected_extent->height, 600U);
}

TEST_CASE("[pure] variable swapchain extents clamp each requested pixel dimension")
{
    VkSurfaceCapabilitiesKHR capabilities = variable_surface_extent();

    std::optional selected_extent{select_swapchain_extent(capabilities, {1, 2000})};

    REQUIRE(selected_extent.has_value());
    CHECK_EQ(selected_extent->width, 64U);
    CHECK_EQ(selected_extent->height, 768U);
}

TEST_CASE("[pure] variable swapchain extents preserve in-range and boundary sizes")
{
    VkExtent2D drawable_extent{512, 384};
    SUBCASE("in-range size") {}
    SUBCASE("minimum boundary")
    {
        drawable_extent = {64, 32};
    }
    SUBCASE("maximum boundary")
    {
        drawable_extent = {1024, 768};
    }

    std::optional selected_extent{
        select_swapchain_extent(variable_surface_extent(), drawable_extent)};

    REQUIRE(selected_extent.has_value());
    CHECK_EQ(selected_extent->width, drawable_extent.width);
    CHECK_EQ(selected_extent->height, drawable_extent.height);
}

TEST_CASE("[pure] zero swapchain extents remain unavailable rather than being enlarged")
{
    VkSurfaceCapabilitiesKHR capabilities = variable_surface_extent();
    VkExtent2D drawable_extent{0, 480};
    SUBCASE("zero requested height")
    {
        drawable_extent = {640, 0};
    }
    SUBCASE("zero fixed width")
    {
        capabilities.currentExtent = {0, 480};
    }
    SUBCASE("zero fixed height")
    {
        capabilities.currentExtent = {640, 0};
    }
    SUBCASE("zero supported area")
    {
        capabilities.minImageExtent = {0, 0};
        capabilities.maxImageExtent = {0, 0};
        drawable_extent = {640, 480};
    }

    CHECK_FALSE(select_swapchain_extent(capabilities, drawable_extent).has_value());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
