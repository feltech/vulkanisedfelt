// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./queue_family_selection.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "./strong_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <limits>
#include <type_traits>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
namespace
{
constexpr auto effective_queue_flags(VkQueueFlags flags) -> VkQueueFlags
{
    // Graphics and compute queues support transfer even when its bit is omitted.
    if ((flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0)
    {
        flags |= VK_QUEUE_TRANSFER_BIT;
    }
    return flags;
}
}  // namespace

auto score_queue_families(
    immer::array<VkQueueFamilyProperties> const & families, VkQueueFlags required_flags)
    -> immer::array<QueueFamilyScore>
{
    auto candidates =
        families | std::views::enumerate |
        std::views::filter(
            [required_flags](auto const & entry)
            {
                auto const & family = std::get<1>(entry);
                auto flags = effective_queue_flags(family.queueFlags);
                return family.queueCount > 0 && (flags & required_flags) == required_flags;
            }) |
        std::views::transform(
            [required_flags](auto const & entry)
            {
                auto const & [index, family] = entry;
                auto flags = effective_queue_flags(family.queueFlags);
                return QueueFamilyScore{
                    queue_family_index{static_cast<std::uint32_t>(index)},
                    queue_family_score{std::popcount(flags & ~required_flags)}};
            });
    auto scores = immer::array<QueueFamilyScore>{}.transient();
    std::ranges::copy(candidates, std::back_inserter(scores));
    return std::move(scores).persistent();
}

auto select_queue_family(immer::array<QueueFamilyScore> const & scores)
    -> std::optional<queue_family_index>
{
    auto selected = std::ranges::min_element(scores, {}, &QueueFamilyScore::score);
    if (selected == scores.end())
    {
        return std::nullopt;
    }
    return selected->family_index;
}

#ifndef DOCTEST_CONFIG_DISABLE
namespace
{
static_assert(!std::is_convertible_v<queue_family_index, queue_family_score>);
static_assert(!std::is_convertible_v<queue_family_score, queue_family_index>);

constexpr auto queue_family(VkQueueFlags flags, std::uint32_t count = 1) -> VkQueueFamilyProperties
{
    VkQueueFamilyProperties properties{};
    properties.queueFlags = flags;
    properties.queueCount = count;
    return properties;
}
}  // namespace

TEST_CASE("[pure] queue family selection reports absent capabilities")
{
    immer::array<VkQueueFamilyProperties> families;
    SUBCASE("empty snapshot")
    {
        families = {};
    }
    SUBCASE("no matching capability")
    {
        families = {queue_family(VK_QUEUE_TRANSFER_BIT)};
    }

    auto selected = select_queue_family(score_queue_families(families, VK_QUEUE_GRAPHICS_BIT));

    CHECK_FALSE(selected.has_value());
}

TEST_CASE("[pure] queue family selection requires every requested capability")
{
    // Earlier families each satisfy only part of the requirement.
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_GRAPHICS_BIT),
        queue_family(VK_QUEUE_COMPUTE_BIT),
        queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_SPARSE_BINDING_BIT)};

    auto selected = select_queue_family(
        score_queue_families(families, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{2});
}

TEST_CASE("[pure] queue family selection skips empty families and retains order")
{
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_GRAPHICS_BIT, 0),
        queue_family(VK_QUEUE_GRAPHICS_BIT),
        queue_family(VK_QUEUE_GRAPHICS_BIT)};

    auto selected = select_queue_family(score_queue_families(families, VK_QUEUE_GRAPHICS_BIT));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{1});
}

TEST_CASE("[pure] zero required flags prefers the fewest capabilities")
{
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_TRANSFER_BIT, 0),
        queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT),
        queue_family(VK_QUEUE_TRANSFER_BIT)};

    auto selected = select_queue_family(score_queue_families(families, 0));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{2});
}

TEST_CASE("[pure] queue family selection prefers specialized transfer queues")
{
    // A zero-count transfer family is unusable despite its minimal capabilities.
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT),
        queue_family(VK_QUEUE_TRANSFER_BIT, 0),
        queue_family(VK_QUEUE_COMPUTE_BIT),
        queue_family(VK_QUEUE_TRANSFER_BIT)};

    auto selected = select_queue_family(score_queue_families(families, VK_QUEUE_TRANSFER_BIT));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{3});
}

TEST_CASE("[pure] queue family selection prefers specialized compute queues")
{
    // The transfer-only family has fewer bits but cannot satisfy compute work.
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT),
        queue_family(VK_QUEUE_TRANSFER_BIT),
        queue_family(VK_QUEUE_COMPUTE_BIT)};

    auto selected = select_queue_family(score_queue_families(families, VK_QUEUE_COMPUTE_BIT));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{2});
}

TEST_CASE("[pure] queue family selection retains order for equal effective capability scores")
{
    // Explicit and implicit transfer support count equally in the score.
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT),
        queue_family(VK_QUEUE_COMPUTE_BIT)};

    auto selected = select_queue_family(score_queue_families(families, 0));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{0});
}

TEST_CASE("[pure] queue family selection recognizes implicit transfer support")
{
    VkQueueFlags supported_flags = VK_QUEUE_TRANSFER_BIT;
    SUBCASE("explicit transfer")
    {
        supported_flags = VK_QUEUE_TRANSFER_BIT;
    }
    SUBCASE("graphics implies transfer")
    {
        supported_flags = VK_QUEUE_GRAPHICS_BIT;
    }
    SUBCASE("compute implies transfer")
    {
        supported_flags = VK_QUEUE_COMPUTE_BIT;
    }
    auto families = immer::array<VkQueueFamilyProperties>{queue_family(supported_flags)};

    auto selected = select_queue_family(score_queue_families(families, VK_QUEUE_TRANSFER_BIT));

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{0});
}

TEST_CASE("[pure] transfer support does not imply other capabilities")
{
    auto families = immer::array<VkQueueFamilyProperties>{queue_family(VK_QUEUE_GRAPHICS_BIT)};

    auto selected = select_queue_family(
        score_queue_families(families, VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT));

    CHECK_FALSE(selected.has_value());
}
TEST_CASE("[pure] queue family scores preserve eligible indices and effective capabilities")
{
    // Empty and unsupported families leave gaps in the original indices.
    auto families = immer::array<VkQueueFamilyProperties>{
        queue_family(VK_QUEUE_TRANSFER_BIT, 0),
        queue_family(VK_QUEUE_GRAPHICS_BIT),
        queue_family(VK_QUEUE_SPARSE_BINDING_BIT),
        queue_family(VK_QUEUE_TRANSFER_BIT)};

    auto scores = score_queue_families(families, VK_QUEUE_TRANSFER_BIT);

    REQUIRE_EQ(scores.size(), 2);
    CHECK_EQ(scores[0].family_index, queue_family_index{1});
    CHECK_EQ(scores[0].score, queue_family_score{1});
    CHECK_EQ(scores[1].family_index, queue_family_index{3});
    CHECK_EQ(scores[1].score, queue_family_score{0});
}

TEST_CASE("[pure] queue family selection accepts independent policy scores")
{
    // An alternative policy can prefer any eligible original index.
    auto scores = immer::array<QueueFamilyScore>{
        {queue_family_index{7}, queue_family_score{4}},
        {queue_family_index{2}, queue_family_score{1}},
        {queue_family_index{9}, queue_family_score{3}}};

    auto selected = select_queue_family(scores);

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{2});
}

TEST_CASE("[pure] queue family score ties preserve input order")
{
    auto scores = immer::array<QueueFamilyScore>{
        {queue_family_index{7}, queue_family_score{1}},
        {queue_family_index{2}, queue_family_score{1}}};

    auto selected = select_queue_family(scores);

    REQUIRE(selected.has_value());
    CHECK_EQ(*selected, queue_family_index{7});
}

TEST_CASE("[pure] queue family selection handles empty and maximum policy scores")
{
    immer::array<QueueFamilyScore> scores;
    SUBCASE("empty policy result")
    {
        scores = {};
    }
    SUBCASE("maximum score is a valid candidate")
    {
        scores = {
            {queue_family_index{5}, queue_family_score{std::numeric_limits<std::uint32_t>::max()}}};
    }

    auto selected = select_queue_family(scores);

    if (scores.empty())
    {
        CHECK_FALSE(selected.has_value());
    }
    else
    {
        REQUIRE(selected.has_value());
        CHECK_EQ(*selected, queue_family_index{5});
    }
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan
