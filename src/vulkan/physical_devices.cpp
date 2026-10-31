// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./physical_devices.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./instance.hpp"

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct DeviceEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] physical device plans are lazy")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;

    auto plan = reader_io_vk_enumerate_physical_devices<decltype(environment)>(&instance);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // Any eager call has no matching expectation and fails.
}

TEST_CASE("[impure] physical device enumeration returns owned snapshots of borrowed handles")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T device;
    unsigned written = 1;
    SUBCASE("empty count")
    {
        written = 0;
    }
    SUBCASE("short fill")
    {
        written = 1;
    }
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_2 = written == 0 ? 0U : 2U)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (written != 0)
    {
        fill = NAMED_REQUIRE_CALL(
                   api, vk_enumerate_physical_devices(&instance, trompeloeil::_, trompeloeil::_))
                   .WITH(_3 != nullptr && *_2 == 2)
                   .LR_SIDE_EFFECT(_3[0] = &device)
                   .SIDE_EFFECT(*_2 = 1)
                   .RETURN(VK_SUCCESS)
                   .IN_SEQUENCE(calls);
    }

    auto result = reader_io_vk_enumerate_physical_devices<decltype(environment)>(&instance)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == written);
    if (written != 0)
    {
        CHECK(fnfelt::result::get_success(result)[0] == &device);
    }
}

TEST_CASE("[impure] physical device enumeration preserves failures and bounds retries")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkResult status = VK_ERROR_INITIALIZATION_FAILED;
    SUBCASE("error")
    {
        status = VK_ERROR_INITIALIZATION_FAILED;
    }
    SUBCASE("incomplete")
    {
        status = VK_INCOMPLETE;
    }
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, nullptr))
        .LR_RETURN(status)
        .RT_TIMES(status == VK_INCOMPLETE ? 3U : 1U);

    auto result = reader_io_vk_enumerate_physical_devices<decltype(environment)>(&instance)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == status);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_physical_devices);
}

TEST_CASE("[impure] physical device enumeration restarts incomplete or oversized fills")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T device;
    bool is_oversized = false;
    SUBCASE("incomplete")
    {
        is_oversized = false;
    }
    SUBCASE("oversized")
    {
        is_oversized = true;
    }
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr)
        .SIDE_EFFECT(*_2 = 2)
        .LR_RETURN(is_oversized ? VK_SUCCESS : VK_INCOMPLETE)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr)
        .LR_SIDE_EFFECT(_3[0] = &device)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    auto result = reader_io_vk_enumerate_physical_devices<decltype(environment)>(&instance)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == 1);
    CHECK(fnfelt::result::get_success(result)[0] == &device);
}

TEST_CASE("[impure] physical device enumeration preserves fill failures")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_physical_devices(&instance, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr)
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY)
        .IN_SEQUENCE(calls);

    // A failed fill must not publish a partial snapshot or start another query.
    auto result = reader_io_vk_enumerate_physical_devices<decltype(environment)>(&instance)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_physical_devices);
}

TEST_CASE("[integration][headless] physical devices can be enumerated")
{
    backend::Api api;
    DeviceEnvironment environment{std::ref(api)};
    auto instance = reader_io_vk_create_instance<decltype(environment)>()
                        .build(environment)
                        .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));

    auto devices = reader_io_vk_enumerate_physical_devices<decltype(environment)>(
                       fnfelt::result::get_success(instance).get())
                       .build(environment)
                       .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(devices));
    for (auto device : fnfelt::result::get_success(devices))
    {
        CHECK(device != VK_NULL_HANDLE);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
