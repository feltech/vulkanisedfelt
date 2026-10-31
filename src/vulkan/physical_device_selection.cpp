// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./physical_device_selection.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "./strong_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <type_traits>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
auto score_physical_devices(immer::array<VkPhysicalDeviceProperties> const & properties)
    -> immer::array<PhysicalDeviceScore>
{
    auto candidates =
        properties | std::views::enumerate |
        std::views::transform(
            [](auto const & entry)
            {
                auto const & [index, device] = entry;
                return PhysicalDeviceScore{
                    physical_device_index{static_cast<std::uint32_t>(index)},
                    physical_device_score{
                        device.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 0U : 1U}};
            });
    auto scores = immer::array<PhysicalDeviceScore>{}.transient();
    std::ranges::copy(candidates, std::back_inserter(scores));
    return std::move(scores).persistent();
}

auto select_physical_device(immer::array<PhysicalDeviceScore> const & scores)
    -> std::optional<physical_device_index>
{
    auto selected = std::ranges::min_element(scores, {}, &PhysicalDeviceScore::score);
    if (selected == scores.end())
    {
        return std::nullopt;
    }
    return selected->device_index;
}

#ifndef DOCTEST_CONFIG_DISABLE
namespace
{
static_assert(!std::is_convertible_v<std::uint32_t, physical_device_index>);
static_assert(!std::is_convertible_v<physical_device_index, std::uint32_t>);
static_assert(!std::is_convertible_v<physical_device_index, physical_device_score>);
static_assert(!std::is_convertible_v<physical_device_score, physical_device_index>);

constexpr auto device_properties(VkPhysicalDeviceType device_type) -> VkPhysicalDeviceProperties
{
    VkPhysicalDeviceProperties properties{};
    properties.deviceType = device_type;
    return properties;
}
}  // namespace

TEST_CASE("[pure] physical device scoring accepts an empty snapshot")
{
    immer::array<VkPhysicalDeviceProperties> properties;

    auto scores = score_physical_devices(properties);

    CHECK(scores.empty());
}

TEST_CASE("[pure] physical device scoring prefers discrete GPUs and preserves indices")
{
    // Device order is retained so scores continue to identify the original snapshots.
    auto properties = immer::array<VkPhysicalDeviceProperties>{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_OTHER),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)};

    auto scores = score_physical_devices(properties);

    REQUIRE_EQ(scores.size(), properties.size());
    for (std::uint32_t index = 0; index < scores.size(); ++index)
    {
        CHECK_EQ(scores[index].device_index, physical_device_index{index});
    }
    CHECK_EQ(scores[0].score, physical_device_score{1});
    CHECK_EQ(scores[1].score, physical_device_score{0});
    CHECK_EQ(scores[2].score, physical_device_score{1});
    CHECK_EQ(scores[3].score, physical_device_score{1});
    CHECK_EQ(scores[4].score, physical_device_score{1});
    CHECK_EQ(scores[5].score, physical_device_score{0});
}

TEST_CASE("[pure] physical device scoring treats non-discrete types equally")
{
    // Keep a shared snapshot live while producing an independent score array.
    auto properties = immer::array<VkPhysicalDeviceProperties>{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)};
    auto retained = properties;

    auto scores = score_physical_devices(properties);

    REQUIRE_EQ(scores.size(), 2U);
    CHECK_EQ(scores[0].score, scores[1].score);
    CHECK_EQ(scores[0].score, physical_device_score{1});
    CHECK_EQ(retained[0].deviceType, VK_PHYSICAL_DEVICE_TYPE_CPU);
    CHECK_EQ(retained[1].deviceType, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
}

TEST_CASE("[pure] physical device selection accepts empty scores")
{
    immer::array<PhysicalDeviceScore> scores;

    auto selected = select_physical_device(scores);

    CHECK_FALSE(selected.has_value());
}

TEST_CASE("[pure] physical device selection uses supplied scores and original indices")
{
    // A custom policy can rank candidates independently of their array positions.
    auto scores = immer::array<PhysicalDeviceScore>{
        {physical_device_index{9}, physical_device_score{4}},
        {physical_device_index{7}, physical_device_score{1}},
        {physical_device_index{2}, physical_device_score{1}}};

    auto selected = select_physical_device(scores);

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, physical_device_index{7});
}

TEST_CASE("[pure] physical device selection accepts maximum scores")
{
    // No numeric score is reserved as an ineligible-candidate sentinel.
    auto scores = immer::array<PhysicalDeviceScore>{
        {physical_device_index{5}, physical_device_score{UINT32_MAX}},
        {physical_device_index{3}, physical_device_score{UINT32_MAX}}};

    auto selected = select_physical_device(scores);

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, physical_device_index{5});
}

TEST_CASE("[pure] physical device scoring and selection preserve policy and ties")
{
    auto properties = immer::array<VkPhysicalDeviceProperties>{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)};
    physical_device_index expected_index{0};
    SUBCASE("discrete GPUs are preferred")
    {
        properties = properties.push_back(device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU));
        expected_index = physical_device_index{2};
    }
    SUBCASE("equally ranked non-discrete GPUs retain snapshot order") {}

    auto selected = select_physical_device(score_physical_devices(properties));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, expected_index);
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan
