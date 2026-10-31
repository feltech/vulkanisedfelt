// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./device_extensions.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
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
using backend::MockApi;

template <class TApi>
struct ExtensionEnvironment
{
    std::reference_wrapper<TApi> api;
};

auto extension_property(char const * name, std::uint32_t version)
{
    VkExtensionProperties property{};
    std::strncpy(property.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE - 1);
    property.specVersion = version;
    return property;
}
}  // namespace

TEST_CASE("[impure] device extension enumeration is deferred and discarded plans make no calls")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};

    auto plan = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // With no expectations installed, any eager API call fails the test.
}

TEST_CASE("[impure] device extension enumeration returns empty without a fill call")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 0)
        .RETURN(VK_SUCCESS);

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).empty());
}

TEST_CASE("[impure] device extension enumeration owns exact or shortened successful snapshots")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};
    auto const expected = immer::array<VkExtensionProperties>{
        extension_property("VK_test_first", 7), extension_property("VK_test_second", 12)};
    std::uint32_t written = 0;
    SUBCASE("empty fill")
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
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 2)
        .LR_SIDE_EFFECT(std::copy_n(expected.begin(), written, _4))
        .LR_SIDE_EFFECT(*_3 = written)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == written);
    for (std::uint32_t idx = 0; idx < written; ++idx)
    {
        CHECK_EQ(
            std::strcmp(
                fnfelt::result::get_success(result)[idx].extensionName,
                expected[idx].extensionName),
            0);
        CHECK(fnfelt::result::get_success(result)[idx].specVersion == expected[idx].specVersion);
    }
}

TEST_CASE("[impure] device extension enumeration preserves count and fill errors")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};
    bool is_fill_error = false;
    SUBCASE("count error")
    {
        is_fill_error = false;
    }
    SUBCASE("fill error")
    {
        is_fill_error = true;
    }
    auto const error = VK_ERROR_OUT_OF_HOST_MEMORY;
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(is_fill_error ? VK_SUCCESS : error)
        .IN_SEQUENCE(calls);
    // Keep a conditional expectation alive until the action has completed.
    std::unique_ptr<trompeloeil::expectation> fill;
    if (is_fill_error)
    {
        fill = NAMED_REQUIRE_CALL(
                   api,
                   vk_enumerate_device_extension_properties(
                       &device, nullptr, trompeloeil::_, trompeloeil::_))
                   .WITH(_4 != nullptr)
                   .RETURN(error)
                   .IN_SEQUENCE(calls);
    }

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == error);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_device_extensions);
}

TEST_CASE("[impure] device extension enumeration retries incomplete or oversized snapshots")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};
    bool is_oversized = false;
    SUBCASE("incomplete fill")
    {
        is_oversized = false;
    }
    SUBCASE("oversized count")
    {
        is_oversized = true;
    }
    auto const expected = extension_property("VK_test_complete", 3);
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 1)
        .LR_SIDE_EFFECT(*_3 = is_oversized ? 2U : 1U)
        .LR_RETURN(is_oversized ? VK_SUCCESS : VK_INCOMPLETE)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 2)
        .LR_SIDE_EFFECT(_4[0] = expected)
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == 1);
    CHECK_EQ(
        std::strcmp(fnfelt::result::get_success(result)[0].extensionName, expected.extensionName),
        0);
}

TEST_CASE("[impure] device extension enumeration bounds incomplete count and fill retries")
{
    VkPhysicalDevice_T device;
    MockApi api;
    ExtensionEnvironment environment{std::ref(api)};
    bool is_count_incomplete = false;
    SUBCASE("incomplete fill")
    {
        is_count_incomplete = false;
    }
    SUBCASE("incomplete count")
    {
        is_count_incomplete = true;
    }
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_3 = is_count_incomplete ? 0U : 1U)
        .LR_RETURN(is_count_incomplete ? VK_INCOMPLETE : VK_SUCCESS)
        .TIMES(3);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (!is_count_incomplete)
    {
        fill = NAMED_REQUIRE_CALL(
                   api,
                   vk_enumerate_device_extension_properties(
                       &device, nullptr, trompeloeil::_, trompeloeil::_))
                   .WITH(_4 != nullptr)
                   .RETURN(VK_INCOMPLETE)
                   .TIMES(3);
    }

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(&device)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == VK_INCOMPLETE);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_device_extensions);
}

TEST_CASE("[impure] device extension enumeration reuses plans without changing earlier snapshots")
{
    VkPhysicalDevice_T device;
    MockApi first_api;
    MockApi second_api;
    ExtensionEnvironment first_environment{std::ref(first_api)};
    ExtensionEnvironment second_environment{std::ref(second_api)};
    auto plan =
        reader_io_vk_enumerate_device_extension_properties<decltype(first_environment)>(&device);
    auto const first_property = extension_property("VK_test_first", 1);
    auto const second_property = extension_property("VK_test_second", 2);
    REQUIRE_CALL(
        first_api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        first_api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(_4[0] = first_property)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        second_api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        second_api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(_4[0] = second_property)
        .RETURN(VK_SUCCESS);

    auto first = plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(second_environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    REQUIRE(fnfelt::result::get_success(first).size() == 1);
    REQUIRE(fnfelt::result::get_success(second).size() == 1);
    CHECK_EQ(
        std::strcmp(
            fnfelt::result::get_success(first)[0].extensionName, first_property.extensionName),
        0);
    CHECK_EQ(
        std::strcmp(
            fnfelt::result::get_success(second)[0].extensionName, second_property.extensionName),
        0);
}

TEST_CASE("[integration][headless] device extension enumeration retrieves real properties")
{
    backend::Api api;
    ExtensionEnvironment environment{std::ref(api)};
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

    auto result = reader_io_vk_enumerate_device_extension_properties<decltype(environment)>(
                      fnfelt::result::get_success(devices)[0])
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    for (auto const & property : fnfelt::result::get_success(result))
    {
        CHECK(std::memchr(property.extensionName, '\0', VK_MAX_EXTENSION_NAME_SIZE) != nullptr);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
