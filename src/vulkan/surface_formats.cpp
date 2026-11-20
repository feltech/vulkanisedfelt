// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./surface_formats.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SurfaceQueryEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] surface format plans retain parents without querying")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char instance_storage{}, device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    instance_ptr instance{reinterpret_cast<VkInstance>(&instance_storage), [](auto) {}};
    std::weak_ptr<instance_ptr::element_type> instance_observer = instance;
    surface_ptr surface{
        reinterpret_cast<VkSurfaceKHR>(&surface_storage),
        [parent = instance](auto) mutable { parent.reset(); }};
    std::weak_ptr<surface_ptr::element_type> surface_observer = surface;

    {
        // Discarded operations keep their surface and instance alive without API effects.
        auto plan = reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface));
        [[maybe_unused]] auto deferred = plan.build(environment);
        instance.reset();

        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(instance_observer.expired());
    }

    CHECK(surface_observer.expired());
    CHECK(instance_observer.expired());
}

TEST_CASE("[impure] surface format queries return empty without filling")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 0)
        .RETURN(VK_SUCCESS);

    vulkan_result<immer::array<VkSurfaceFormatKHR>> outcome =
        reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome).empty());
}

TEST_CASE("[impure] surface format queries own exact or shortened snapshots")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    immer::array<VkSurfaceFormatKHR> const expected{
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    std::uint32_t written = 2;
    SUBCASE("empty fill")
    {
        written = 0;
    }
    SUBCASE("short fill")
    {
        written = 1;
    }
    SUBCASE("exact fill") {}
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 2)
        .LR_SIDE_EFFECT(std::ranges::copy(std::views::take(expected, written), _4))
        .LR_SIDE_EFFECT(*_3 = written)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    vulkan_result<immer::array<VkSurfaceFormatKHR>> outcome =
        reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    immer::array snapshot{fnfelt::result::get_success(outcome)};
    REQUIRE_EQ(snapshot.size(), written);
    for (auto const & [actual, reference] : std::views::zip(snapshot, expected))
    {
        CHECK_EQ(actual.format, reference.format);
        CHECK_EQ(actual.colorSpace, reference.colorSpace);
    }
}

TEST_CASE("[impure] surface format queries preserve count and fill errors")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    bool is_fill_error = false;
    SUBCASE("count error") {}
    SUBCASE("fill error")
    {
        is_fill_error = true;
    }
    VkResult const error = VK_ERROR_SURFACE_LOST_KHR;
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(is_fill_error ? VK_SUCCESS : error);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (is_fill_error)
    {
        fill = NAMED_REQUIRE_CALL(
                   api,
                   vk_get_physical_device_surface_formats_khr(
                       physical_device, handle, trompeloeil::_, trompeloeil::_))
                   .WITH(_4 != nullptr)
                   .RETURN(error);
    }

    vulkan_result<immer::array<VkSurfaceFormatKHR>> outcome =
        reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, error);
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::get_physical_device_surface_formats);
}

TEST_CASE("[impure] surface format queries retry incomplete or oversized snapshots")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    bool is_count_incomplete = false;
    bool is_oversized = false;
    SUBCASE("incomplete count")
    {
        is_count_incomplete = true;
    }
    SUBCASE("incomplete fill") {}
    SUBCASE("oversized fill count")
    {
        is_oversized = true;
    }
    VkSurfaceFormatKHR const expected{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(is_count_incomplete ? VK_INCOMPLETE : VK_SUCCESS)
        .IN_SEQUENCE(calls);
    std::unique_ptr<trompeloeil::expectation> first_fill;
    if (!is_count_incomplete)
    {
        first_fill = NAMED_REQUIRE_CALL(
                         api,
                         vk_get_physical_device_surface_formats_khr(
                             physical_device, handle, trompeloeil::_, trompeloeil::_))
                         .WITH(_4 != nullptr && *_3 == 1)
                         .LR_SIDE_EFFECT(*_3 = is_oversized ? 2U : 1U)
                         .LR_RETURN(is_oversized ? VK_SUCCESS : VK_INCOMPLETE)
                         .IN_SEQUENCE(calls);
    }
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 2)
        .LR_SIDE_EFFECT(_4[0] = expected)
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    vulkan_result<immer::array<VkSurfaceFormatKHR>> outcome =
        reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    REQUIRE_EQ(fnfelt::result::get_success(outcome).size(), 1);
    VkSurfaceFormatKHR const actual = fnfelt::result::get_success(outcome)[0];
    CHECK_EQ(actual.format, expected.format);
}

TEST_CASE("[impure] surface format queries bound count and fill retries")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    bool is_count_incomplete = false;
    SUBCASE("count never completes")
    {
        is_count_incomplete = true;
    }
    SUBCASE("fill never completes") {}
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(is_count_incomplete ? VK_INCOMPLETE : VK_SUCCESS)
        .TIMES(3);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (!is_count_incomplete)
    {
        fill = NAMED_REQUIRE_CALL(
                   api,
                   vk_get_physical_device_surface_formats_khr(
                       physical_device, handle, trompeloeil::_, trompeloeil::_))
                   .WITH(_4 != nullptr)
                   .RETURN(VK_INCOMPLETE)
                   .TIMES(3);
    }

    vulkan_result<immer::array<VkSurfaceFormatKHR>> outcome =
        reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_INCOMPLETE);
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::get_physical_device_surface_formats);
}

TEST_CASE("[impure] surface format plans preserve earlier results across repeated runs")
{
    backend::MockApi api;
    SurfaceQueryEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    VkSurfaceFormatKHR expected =
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS)
        .TIMES(2);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 1)
        .LR_SIDE_EFFECT(_4[0] = expected)
        .RETURN(VK_SUCCESS)
        .TIMES(2);
    auto plan = reader_io_vk_get_physical_device_surface_formats_khr<decltype(environment)>(
        physical_device, std::move(surface));

    // Both reusable and consuming execution produce independently owned immutable arrays.
    vulkan_result<immer::array<VkSurfaceFormatKHR>> first =
        plan.build(environment).sync_wait(lf::lazy_pool{2});
    expected = VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    vulkan_result<immer::array<VkSurfaceFormatKHR>> second =
        std::move(plan).build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    REQUIRE_EQ(fnfelt::result::get_success(first).size(), 1);
    REQUIRE_EQ(fnfelt::result::get_success(second).size(), 1);
    CHECK_EQ(fnfelt::result::get_success(first)[0].format, VK_FORMAT_B8G8R8A8_UNORM);
    CHECK_EQ(fnfelt::result::get_success(second)[0].format, VK_FORMAT_R8G8B8A8_UNORM);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
