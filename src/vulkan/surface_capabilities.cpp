// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./surface_capabilities.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SurfaceCapabilitiesEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] surface capability plans retain parents without querying")
{
    backend::MockApi api;
    SurfaceCapabilitiesEnvironment environment{std::ref(api)};
    char instance_storage{}, device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    instance_ptr instance{reinterpret_cast<VkInstance>(&instance_storage), [](auto) {}};
    std::weak_ptr<instance_ptr::element_type> instance_observer = instance;
    surface_ptr surface{
        reinterpret_cast<VkSurfaceKHR>(&surface_storage),
        [parent = instance](auto) mutable { parent.reset(); }};
    std::weak_ptr<surface_ptr::element_type> surface_observer = surface;

    {
        // A discarded deferred operation retains both owners without touching the command API.
        auto plan =
            reader_io_vk_get_physical_device_surface_capabilities_khr<decltype(environment)>(
                physical_device, std::move(surface));
        [[maybe_unused]] auto deferred = plan.build(environment);
        instance.reset();

        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(instance_observer.expired());
    }

    CHECK(surface_observer.expired());
    CHECK(instance_observer.expired());
}

TEST_CASE("[impure] surface capability plans return independent snapshots across repeated runs")
{
    backend::MockApi api;
    SurfaceCapabilitiesEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    VkSurfaceCapabilitiesKHR expected{};
    expected.minImageCount = 2;
    expected.maxImageCount = 4;
    expected.currentExtent = {640, 480};
    expected.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_capabilities_khr(physical_device, handle, trompeloeil::_))
        .WITH(_3 != nullptr)
        .LR_SIDE_EFFECT(*_3 = expected)
        .LR_SIDE_EFFECT(++expected.minImageCount)
        .RETURN(VK_SUCCESS)
        .TIMES(2);
    auto plan = reader_io_vk_get_physical_device_surface_capabilities_khr<decltype(environment)>(
        physical_device, std::move(surface));

    // Each execution owns its result even when a later query changes the returned capabilities.
    vulkan_result<VkSurfaceCapabilitiesKHR> first =
        plan.build(environment).sync_wait(lf::lazy_pool{2});
    vulkan_result<VkSurfaceCapabilitiesKHR> second =
        std::move(plan).build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    CHECK_EQ(fnfelt::result::get_success(first).minImageCount, 2U);
    CHECK_EQ(fnfelt::result::get_success(second).minImageCount, 3U);
    CHECK_EQ(fnfelt::result::get_success(first).maxImageCount, 4U);
    CHECK_EQ(fnfelt::result::get_success(first).currentExtent.width, 640U);
    CHECK_EQ(fnfelt::result::get_success(first).currentExtent.height, 480U);
    CHECK_EQ(
        fnfelt::result::get_success(first).supportedUsageFlags,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

TEST_CASE("[impure] surface capability queries preserve Vulkan failures")
{
    backend::MockApi api;
    SurfaceCapabilitiesEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    VkResult status = VK_ERROR_SURFACE_LOST_KHR;
    SUBCASE("lost surface") {}
    SUBCASE("allocation failure")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_capabilities_khr(physical_device, handle, trompeloeil::_))
        .WITH(_3 != nullptr)
        .LR_RETURN(status);

    vulkan_result<VkSurfaceCapabilitiesKHR> outcome =
        reader_io_vk_get_physical_device_surface_capabilities_khr<decltype(environment)>(
            physical_device, std::move(surface))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::get_physical_device_surface_capabilities);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
