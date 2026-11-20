// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_image_count_selection.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <limits>
#include <type_traits>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>

#include "./swapchain_image_count_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
constexpr auto surface_image_counts(std::uint32_t minimum, std::uint32_t maximum)
    -> VkSurfaceCapabilitiesKHR
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = minimum;
    capabilities.maxImageCount = maximum;
    return capabilities;
}

static_assert(!std::is_convertible_v<std::uint32_t, swapchain_image_count>);
static_assert(!std::is_convertible_v<swapchain_image_count, std::uint32_t>);
static_assert(select_swapchain_image_count(surface_image_counts(1, 0)) == swapchain_image_count{2});
static_assert(select_swapchain_image_count(surface_image_counts(3, 4)) == swapchain_image_count{3});
}  // namespace

TEST_CASE("[pure] swapchain image counts prefer double buffering within supported bounds")
{
    CHECK_EQ(select_swapchain_image_count(surface_image_counts(1, 0)), swapchain_image_count{2});
    CHECK_EQ(select_swapchain_image_count(surface_image_counts(3, 5)), swapchain_image_count{3});
    CHECK_EQ(select_swapchain_image_count(surface_image_counts(1, 1)), swapchain_image_count{1});
    CHECK_EQ(select_swapchain_image_count(surface_image_counts(4, 4)), swapchain_image_count{4});
}

TEST_CASE("[pure] custom swapchain image counts clamp to finite surface bounds")
{
    VkSurfaceCapabilitiesKHR capabilities = surface_image_counts(2, 5);

    CHECK_EQ(
        select_swapchain_image_count(capabilities, swapchain_image_count{0}),
        swapchain_image_count{2});
    CHECK_EQ(
        select_swapchain_image_count(capabilities, swapchain_image_count{4}),
        swapchain_image_count{4});
    CHECK_EQ(
        select_swapchain_image_count(capabilities, swapchain_image_count{9}),
        swapchain_image_count{5});
}

TEST_CASE("[pure] unbounded swapchain image counts retain large preferences without overflow")
{
    constexpr std::uint32_t maximum_count = std::numeric_limits<std::uint32_t>::max();

    CHECK_EQ(
        select_swapchain_image_count(
            surface_image_counts(1, 0), swapchain_image_count{maximum_count}),
        swapchain_image_count{maximum_count});
    CHECK_EQ(
        select_swapchain_image_count(surface_image_counts(maximum_count, 0)),
        swapchain_image_count{maximum_count});
    CHECK_EQ(
        select_swapchain_image_count(
            surface_image_counts(1, 5), swapchain_image_count{maximum_count}),
        swapchain_image_count{5});
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
