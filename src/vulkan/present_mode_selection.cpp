// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./present_mode_selection.hpp"

#include <algorithm>
#include <optional>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
auto select_present_mode(
    immer::array<VkPresentModeKHR> const & supported_modes,
    immer::array<VkPresentModeKHR> const & preferred_modes) -> std::optional<VkPresentModeKHR>
{
    auto selected_present_mode = std::ranges::find_if(
        preferred_modes,
        [&](VkPresentModeKHR present_mode)
        { return std::ranges::contains(supported_modes, present_mode); });
    if (selected_present_mode == preferred_modes.end())
    {
        return std::nullopt;
    }
    return *selected_present_mode;
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[pure] present mode selection prefers mailbox over fifo regardless of advertised order")
{
    immer::array<VkPresentModeKHR> supported_modes{
        VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR};

    std::optional selected_present_mode{select_present_mode(supported_modes)};

    REQUIRE(selected_present_mode.has_value());
    CHECK_EQ(*selected_present_mode, VK_PRESENT_MODE_MAILBOX_KHR);
}

TEST_CASE("[pure] present mode selection falls back to advertised fifo")
{
    std::optional selected_present_mode{
        select_present_mode({VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR})};

    REQUIRE(selected_present_mode.has_value());
    CHECK_EQ(*selected_present_mode, VK_PRESENT_MODE_FIFO_KHR);
}

TEST_CASE("[pure] present mode selection follows custom preference order")
{
    immer::array<VkPresentModeKHR> supported_modes{
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};

    std::optional selected_present_mode{select_present_mode(
        supported_modes, {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR})};

    REQUIRE(selected_present_mode.has_value());
    CHECK_EQ(*selected_present_mode, VK_PRESENT_MODE_IMMEDIATE_KHR);
}

TEST_CASE("[pure] present mode selection does not invent unavailable preferences")
{
    CHECK_FALSE(select_present_mode({}).has_value());
    CHECK_FALSE(select_present_mode({VK_PRESENT_MODE_IMMEDIATE_KHR}).has_value());
    CHECK_FALSE(select_present_mode({VK_PRESENT_MODE_FIFO_KHR}, {}).has_value());
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan
