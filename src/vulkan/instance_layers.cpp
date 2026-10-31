// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./instance_layers.hpp"

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
#include "./instance_extensions.hpp"

namespace vulkandemo::vulkan
{
namespace
{
using backend::MockApi;

template <class TApi>
struct LayerEnvironment
{
    std::reference_wrapper<TApi> api;
};

auto layer_property(char const * name, std::uint32_t version)
{
    VkLayerProperties property{};
    std::strncpy(property.layerName, name, VK_MAX_EXTENSION_NAME_SIZE - 1);
    std::strncpy(property.description, "A test layer", VK_MAX_DESCRIPTION_SIZE - 1);
    property.specVersion = version;
    property.implementationVersion = version + 1;
    return property;
}
}  // namespace

TEST_CASE("[impure] layer enumeration is deferred and discarded plans make no calls")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};

    auto plan = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>();
    [[maybe_unused]] auto deferred = plan.build(environment);

    // With no expectations installed, any eager API call fails the test.
}

TEST_CASE("[impure] Vulkan queries compose directly and preserve the failing operation")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
    bool should_fail_extensions = false;
    SUBCASE("extension failure bypasses layer query")
    {
        should_fail_extensions = true;
    }
    SUBCASE("layer failure follows successful extension query") {}
    REQUIRE_CALL(api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 0)
        .RETURN(should_fail_extensions ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS);
    std::unique_ptr<trompeloeil::expectation> layer_expectation;
    if (!should_fail_extensions)
    {
        layer_expectation =
            NAMED_REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
                .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    }

    // Both queries share VulkanFailure, so dependent chaining needs no error conversion.
    auto plan =
        reader_io_vk_enumerate_instance_extension_properties<decltype(environment)>().and_then(
            [](immer::array<VkExtensionProperties>)
            { return reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>(); });
    auto result = plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    auto const failure = fnfelt::result::get_error(result);
    CHECK_EQ(failure.result, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(
        failure.operation ==
        (should_fail_extensions ? FallibleVulkanOperation::enumerate_instance_extensions
                                : FallibleVulkanOperation::enumerate_instance_layers));
}

TEST_CASE("[impure] layer enumeration returns empty without a fill call")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 0)
        .RETURN(VK_SUCCESS);

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).empty());
}

TEST_CASE("[impure] layer enumeration owns exact or shortened successful snapshots")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
    auto const expected = immer::array<VkLayerProperties>{
        layer_property("VK_test_first", 7), layer_property("VK_test_second", 12)};
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
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
        .WITH(_2 != nullptr && *_1 == 2)
        .LR_SIDE_EFFECT(std::copy_n(expected.begin(), written, _2))
        .LR_SIDE_EFFECT(*_1 = written)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == written);
    for (std::uint32_t idx = 0; idx < written; ++idx)
    {
        CHECK_EQ(
            std::strcmp(
                fnfelt::result::get_success(result)[idx].layerName, expected[idx].layerName),
            0);
        CHECK_EQ(
            std::strcmp(
                fnfelt::result::get_success(result)[idx].description, expected[idx].description),
            0);
        CHECK(fnfelt::result::get_success(result)[idx].specVersion == expected[idx].specVersion);
        CHECK(
            fnfelt::result::get_success(result)[idx].implementationVersion ==
            expected[idx].implementationVersion);
    }
}

TEST_CASE("[impure] layer enumeration preserves count and fill errors")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
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
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 1)
        .LR_RETURN(is_fill_error ? VK_SUCCESS : error)
        .IN_SEQUENCE(calls);
    // Keep a conditional expectation alive until the action has completed.
    std::unique_ptr<trompeloeil::expectation> fill;
    if (is_fill_error)
    {
        fill = NAMED_REQUIRE_CALL(
                   api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
                   .WITH(_2 != nullptr)
                   .RETURN(error)
                   .IN_SEQUENCE(calls);
    }

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == error);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_instance_layers);
}

TEST_CASE("[impure] layer enumeration retries incomplete or oversized snapshots")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
    bool is_oversized = false;
    SUBCASE("incomplete fill")
    {
        is_oversized = false;
    }
    SUBCASE("oversized count")
    {
        is_oversized = true;
    }
    auto const expected = layer_property("VK_test_complete", 3);
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
        .WITH(_2 != nullptr && *_1 == 1)
        .LR_SIDE_EFFECT(*_1 = is_oversized ? 2U : 1U)
        .LR_RETURN(is_oversized ? VK_SUCCESS : VK_INCOMPLETE)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
        .WITH(_2 != nullptr && *_1 == 2)
        .LR_SIDE_EFFECT(_2[0] = expected)
        .SIDE_EFFECT(*_1 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::get_success(result).size() == 1);
    CHECK_EQ(std::strcmp(fnfelt::result::get_success(result)[0].layerName, expected.layerName), 0);
}

TEST_CASE("[impure] layer enumeration bounds incomplete count and fill retries")
{
    MockApi api;
    LayerEnvironment environment{std::ref(api)};
    bool is_count_incomplete = false;
    SUBCASE("incomplete fill")
    {
        is_count_incomplete = false;
    }
    SUBCASE("incomplete count")
    {
        is_count_incomplete = true;
    }
    REQUIRE_CALL(api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_1 = is_count_incomplete ? 0U : 1U)
        .LR_RETURN(is_count_incomplete ? VK_INCOMPLETE : VK_SUCCESS)
        .TIMES(3);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (!is_count_incomplete)
    {
        fill = NAMED_REQUIRE_CALL(
                   api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
                   .WITH(_2 != nullptr)
                   .RETURN(VK_INCOMPLETE)
                   .TIMES(3);
    }

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == VK_INCOMPLETE);
    CHECK(
        fnfelt::result::get_error(result).operation ==
        FallibleVulkanOperation::enumerate_instance_layers);
}

TEST_CASE("[impure] layer enumeration reuses plans without changing earlier snapshots")
{
    MockApi first_api;
    MockApi second_api;
    LayerEnvironment first_environment{std::ref(first_api)};
    LayerEnvironment second_environment{std::ref(second_api)};
    auto plan = reader_io_vk_enumerate_instance_layer_properties<decltype(first_environment)>();
    auto const first_property = layer_property("VK_test_first", 1);
    auto const second_property = layer_property("VK_test_second", 2);
    REQUIRE_CALL(first_api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(first_api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
        .WITH(_2 != nullptr)
        .LR_SIDE_EFFECT(_2[0] = first_property)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(second_api, vk_enumerate_instance_layer_properties(trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_1 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(second_api, vk_enumerate_instance_layer_properties(trompeloeil::_, trompeloeil::_))
        .WITH(_2 != nullptr)
        .LR_SIDE_EFFECT(_2[0] = second_property)
        .RETURN(VK_SUCCESS);

    auto first = plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(second_environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    REQUIRE(fnfelt::result::get_success(first).size() == 1);
    REQUIRE(fnfelt::result::get_success(second).size() == 1);
    CHECK_EQ(
        std::strcmp(fnfelt::result::get_success(first)[0].layerName, first_property.layerName), 0);
    CHECK_EQ(
        std::strcmp(fnfelt::result::get_success(second)[0].layerName, second_property.layerName),
        0);
}

TEST_CASE("[integration][headless] layer enumeration retrieves real global properties")
{
    backend::Api api;
    LayerEnvironment environment{std::ref(api)};

    auto result = reader_io_vk_enumerate_instance_layer_properties<decltype(environment)>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    // An implementation may expose no layers; returned strings must still be bounded.
    for (auto const & property : fnfelt::result::get_success(result))
    {
        CHECK(std::memchr(property.layerName, '\0', VK_MAX_EXTENSION_NAME_SIZE) != nullptr);
        CHECK(std::memchr(property.description, '\0', VK_MAX_DESCRIPTION_SIZE) != nullptr);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
