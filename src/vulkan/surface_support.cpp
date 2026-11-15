// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./surface_support.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SurfaceSupportEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] surface support plans retain their surface without querying")
{
    backend::MockApi api;
    SurfaceSupportEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    surface_ptr surface{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}};
    std::weak_ptr<surface_ptr::element_type> observer = surface;

    {
        // Building and discarding a plan performs no API calls but retains the input lifetime.
        auto plan = reader_io_vk_get_physical_device_surface_support_khr<decltype(environment)>(
            device, queue_family_index{3}, std::move(surface));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(observer.expired());
    }

    CHECK(observer.expired());
}

TEST_CASE("[impure] surface support preserves true false and Vulkan errors across repeated runs")
{
    backend::MockApi api;
    SurfaceSupportEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    VkPhysicalDevice device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    surface_ptr surface{handle, [](auto) {}};
    VkBool32 is_supported = VK_TRUE;
    VkResult status = VK_SUCCESS;
    SUBCASE("supported") {}
    SUBCASE("unsupported is successful false")
    {
        is_supported = VK_FALSE;
    }
    SUBCASE("surface lost")
    {
        status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("allocation failure")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    REQUIRE_CALL(api, vk_get_physical_device_surface_support_khr(device, 3, handle, trompeloeil::_))
        .TIMES(2)
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(*_4 = is_supported)
        .LR_RETURN(status);
    auto plan = reader_io_vk_get_physical_device_surface_support_khr<decltype(environment)>(
        device, queue_family_index{3}, std::move(surface));

    // Both reusable and consuming executions invoke the injected API with the retained handle.
    vulkan_result<bool> first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    vulkan_result<bool> second = std::move(plan).build(environment).sync_wait(lf::lazy_pool{2});

    for (vulkan_result<bool> const * outcome : {&first, &second})
    {
        if (status == VK_SUCCESS)
        {
            REQUIRE(fnfelt::result::is_success(*outcome));
            CHECK_EQ(fnfelt::result::get_success(*outcome), is_supported == VK_TRUE);
        }
        else
        {
            REQUIRE(fnfelt::result::is_error(*outcome));
            CHECK_EQ(fnfelt::result::get_error(*outcome).result, status);
            CHECK(
                fnfelt::result::get_error(*outcome).operation ==
                FallibleVulkanOperation::get_physical_device_surface_support);
        }
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
