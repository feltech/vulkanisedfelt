// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_maintenance1_features.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"

struct VkPhysicalDevice_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct MaintenanceFeaturesEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] maintenance1 feature queries are deferred")
{
    backend::MockApi api;
    MaintenanceFeaturesEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;

    auto plan = reader_io_query_swapchain_maintenance1_feature<decltype(environment)>(&device);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // Discarding the plan or its IO must not query the native output chain.
}

TEST_CASE("[impure] maintenance1 feature queries return owned observations of initialized chains")
{
    backend::MockApi api;
    MaintenanceFeaturesEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;
    bool has_feature = true;
    int queries = 0;
    auto inspect_chain = [&](VkPhysicalDeviceFeatures2 * features)
    {
        REQUIRE(features != nullptr);
        CHECK_EQ(features->sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        CHECK_EQ(features->features.robustBufferAccess, VK_FALSE);
        auto * maintenance1 =
            static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *>(features->pNext);
        REQUIRE(maintenance1 != nullptr);
        CHECK_EQ(
            maintenance1->sType,
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
        CHECK(maintenance1->pNext == nullptr);
        CHECK_EQ(maintenance1->swapchainMaintenance1, VK_FALSE);
        maintenance1->swapchainMaintenance1 = has_feature ? VK_TRUE : VK_FALSE;
        ++queries;
    };
    REQUIRE_CALL(api, vk_get_physical_device_features2(&device, trompeloeil::_))
        .TIMES(2)
        .LR_SIDE_EFFECT(inspect_chain(_2));
    auto plan = reader_io_query_swapchain_maintenance1_feature<decltype(environment)>(&device);

    bool const first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    has_feature = false;
    bool const second = plan.build(environment).sync_wait(lf::lazy_pool{2});

    // A later query cannot change the value copied from the first native output chain.
    CHECK(first);
    CHECK_FALSE(second);
    CHECK_EQ(queries, 2);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
