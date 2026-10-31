// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./queue_family_properties.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./instance.hpp"
#include "./physical_devices.hpp"

struct VkPhysicalDevice_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct QueueEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] queue family plans are deferred")
{
    backend::MockApi api;
    QueueEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;

    auto plan =
        reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(&device);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // Any eager query would lack a matching expectation.
}

TEST_CASE("[impure] queue family count queries can return empty")
{
    backend::MockApi api;
    QueueEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;
    REQUIRE_CALL(
        api, vk_get_physical_device_queue_family_properties(&device, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 0);

    auto properties =
        reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(&device)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    CHECK(properties.empty());
}

TEST_CASE("[impure] queue family queries publish only the written prefix")
{
    backend::MockApi api;
    QueueEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;
    std::uint32_t written = 1;
    SUBCASE("zero fill")
    {
        written = 0;
    }
    SUBCASE("short fill")
    {
        written = 1;
    }
    SUBCASE("exact fill")
    {
        written = 2;
    }
    VkQueueFamilyProperties expected{};
    expected.queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
    expected.queueCount = 3;
    expected.timestampValidBits = 48;
    expected.minImageTransferGranularity = {2, 3, 4};
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api, vk_get_physical_device_queue_family_properties(&device, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 2)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_queue_family_properties(&device, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr && *_2 == 2)
        .LR_SIDE_EFFECT(std::fill_n(_3, written, expected))
        .LR_SIDE_EFFECT(*_2 = written)
        .IN_SEQUENCE(calls);

    auto properties =
        reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(&device)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(properties.size() == written);
    for (auto const & property : properties)
    {
        CHECK_EQ(property.queueFlags, expected.queueFlags);
        CHECK_EQ(property.queueCount, expected.queueCount);
        CHECK_EQ(property.timestampValidBits, expected.timestampValidBits);
        CHECK_EQ(property.minImageTransferGranularity.width, 2);
        CHECK_EQ(property.minImageTransferGranularity.height, 3);
        CHECK_EQ(property.minImageTransferGranularity.depth, 4);
    }
}

TEST_CASE("[integration][headless] queue family properties can be queried")
{
    backend::Api api;
    QueueEnvironment environment{std::ref(api)};
    auto instance = reader_io_vk_create_instance<decltype(environment)>()
                        .build(environment)
                        .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));
    auto devices = reader_io_vk_enumerate_physical_devices<decltype(environment)>(
                       fnfelt::result::get_success(instance).get())
                       .build(environment)
                       .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(devices));
    REQUIRE_FALSE(fnfelt::result::get_success(devices).empty());

    auto properties =
        reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(
            fnfelt::result::get_success(devices)[0])
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE_FALSE(properties.empty());
    for (auto const & property : properties)
    {
        CHECK_GT(property.queueCount, 0);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
