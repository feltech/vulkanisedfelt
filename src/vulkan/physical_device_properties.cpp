// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./physical_device_properties.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstring>
#include <functional>

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
struct PropertiesEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] physical device property queries are deferred")
{
    backend::MockApi api;
    PropertiesEnvironment environment{std::ref(api)};
    VkPhysicalDevice_T device;

    auto plan = reader_io_vk_get_physical_device_properties<decltype(environment)>(&device);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // Constructing and discarding the query must not inspect the device.
}

TEST_CASE("[impure] physical device properties are owned and read from the supplied API")
{
    backend::MockApi first_api;
    backend::MockApi second_api;
    PropertiesEnvironment first_environment{std::ref(first_api)};
    PropertiesEnvironment second_environment{std::ref(second_api)};
    VkPhysicalDevice_T device;
    VkPhysicalDeviceProperties expected{};
    std::strncpy(expected.deviceName, "Test device", VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
    expected.deviceID = 19;
    expected.vendorID = 31;
    expected.deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU;
    expected.apiVersion = VK_API_VERSION_1_0;
    expected.driverVersion = 7;
    expected.limits.maxImageDimension2D = 2048;
    expected.sparseProperties.residencyStandard2DBlockShape = VK_TRUE;
    auto second_expected = expected;
    second_expected.deviceID = 20;
    second_expected.limits.maxImageDimension2D = 4096;
    REQUIRE_CALL(first_api, vk_get_physical_device_properties(&device, trompeloeil::_))
        .WITH(_2 != nullptr)
        .LR_SIDE_EFFECT(*_2 = expected);
    REQUIRE_CALL(second_api, vk_get_physical_device_properties(&device, trompeloeil::_))
        .WITH(_2 != nullptr)
        .LR_SIDE_EFFECT(*_2 = second_expected);
    auto plan = reader_io_vk_get_physical_device_properties<decltype(first_environment)>(&device);

    auto first = plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(second_environment).sync_wait(lf::lazy_pool{2});
    expected.deviceID = 99;

    // A subsequent query and changes to mock source data cannot change the first value.
    CHECK_EQ(first.deviceID, 19);
    CHECK_EQ(second.deviceID, 20);
    CHECK_EQ(first.vendorID, 31);
    CHECK_EQ(first.deviceType, VK_PHYSICAL_DEVICE_TYPE_CPU);
    CHECK_EQ(first.apiVersion, VK_API_VERSION_1_0);
    CHECK_EQ(first.driverVersion, 7);
    CHECK_EQ(first.limits.maxImageDimension2D, 2048);
    CHECK_EQ(second.limits.maxImageDimension2D, 4096);
    CHECK_EQ(first.sparseProperties.residencyStandard2DBlockShape, VK_TRUE);
    CHECK_EQ(std::strcmp(first.deviceName, "Test device"), 0);
}

TEST_CASE("[integration][headless] physical device properties can be queried")
{
    backend::Api api;
    PropertiesEnvironment environment{std::ref(api)};
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

    auto properties = reader_io_vk_get_physical_device_properties<decltype(environment)>(
                          fnfelt::result::get_success(devices)[0])
                          .build(environment)
                          .sync_wait(lf::lazy_pool{2});

    CHECK(std::memchr(properties.deviceName, '\0', VK_MAX_PHYSICAL_DEVICE_NAME_SIZE) != nullptr);
    CHECK_GT(properties.limits.maxImageDimension2D, 0);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
