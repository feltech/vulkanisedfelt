// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_maintenance1_support.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./instance_extensions.hpp"
#include "./managed_handles_types.hpp"
#include "./physical_devices.hpp"
#include "./presentation_instance_extensions.hpp"
#include "./renderer_errors.hpp"

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
struct MaintenanceSupportEnvironment
{
    std::reference_wrapper<TApi> api;
};

auto enabled_maintenance_instance_extensions() -> immer::array<extension_name>
{
    return {
        extension_name{VK_KHR_SURFACE_EXTENSION_NAME},
        extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
        extension_name{"VK_KHR_surface_maintenance1"},
        extension_name{"VK_EXT_surface_maintenance1"}};
}

auto advertised_maintenance_extensions(bool has_extension) -> immer::array<VkExtensionProperties>
{
    auto properties = immer::array<VkExtensionProperties>{}.transient();
    VkExtensionProperties swapchain{};
    std::ranges::copy(std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}, swapchain.extensionName);
    properties.push_back(swapchain);
    if (has_extension)
    {
        VkExtensionProperties maintenance1{};
        std::ranges::copy(
            std::string_view{"VK_EXT_swapchain_maintenance1"}, maintenance1.extensionName);
        properties.push_back(maintenance1);
    }
    return std::move(properties).persistent();
}
}  // namespace

TEST_CASE("[impure] maintenance1 support discovery is lazy and retains its executed instance")
{
    backend::MockApi api;
    MaintenanceSupportEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T device;
    int destructions = 0;
    instance_ptr instance{&instance_handle, [&](auto *) { ++destructions; }};
    std::weak_ptr observer{instance};
    PresentationInstance context{instance, enabled_maintenance_instance_extensions()};
    immer::array properties = advertised_maintenance_extensions(true);
    int feature_queries = 0;
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .TIMES(2)
        .LR_SIDE_EFFECT(CHECK_FALSE(observer.expired()))
        .LR_SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(properties.size()))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .TIMES(2)
        .LR_SIDE_EFFECT(CHECK_FALSE(observer.expired()))
        .LR_SIDE_EFFECT(std::ranges::copy(properties, _4))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_get_physical_device_features2(&device, trompeloeil::_))
        .TIMES(2)
        .LR_SIDE_EFFECT(CHECK_FALSE(observer.expired()))
        .LR_SIDE_EFFECT(
            static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *>(_2->pNext)
                ->swapchainMaintenance1 = VK_TRUE)
        .LR_SIDE_EFFECT(++feature_queries);

    // Materialize the IO from a temporary ReaderIO, then release every caller-owned instance alias.
    auto operation = reader_io_discover_swapchain_maintenance1_support<decltype(environment)>(
                         std::move(context), &device)
                         .build(environment)
                         .io();
    instance.reset();
    CHECK_EQ(feature_queries, 0);
    CHECK_EQ(destructions, 0);
    auto first = operation.build().sync_wait(lf::lazy_pool{2});
    auto second = std::move(operation).build().sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::get_success(first).has_value());
    CHECK_EQ(value_of(*fnfelt::result::get_success(first)).view(), "VK_EXT_swapchain_maintenance1");
    REQUIRE(fnfelt::result::is_success(second));
    CHECK_EQ(feature_queries, 2);
    // Returned names are independent values; only the consumed IO needed the instance owner.
    CHECK_EQ(destructions, 1);
    CHECK(observer.expired());
}

TEST_CASE("[impure] maintenance1 support requires both an extension candidate and its feature")
{
    backend::MockApi api;
    MaintenanceSupportEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T device;
    instance_ptr instance{&instance_handle, [](auto *) {}};
    PresentationInstance context{instance, enabled_maintenance_instance_extensions()};
    bool has_extension = true;
    bool has_feature = true;
    bool has_matching_instance = true;
    SUBCASE("supported extension and feature") {}
    SUBCASE("false feature produces successful absence")
    {
        has_feature = false;
    }
    SUBCASE("no device maintenance extension skips the feature call")
    {
        has_extension = false;
    }
    SUBCASE("no matching instance path skips the feature call")
    {
        has_matching_instance = false;
        context.enabled_extensions = {
            extension_name{VK_KHR_SURFACE_EXTENSION_NAME},
            extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
            extension_name{"VK_KHR_surface_maintenance1"}};
    }
    immer::array properties = advertised_maintenance_extensions(has_extension);
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(properties.size()))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(std::ranges::copy(properties, _4))
        .RETURN(VK_SUCCESS);
    std::unique_ptr<trompeloeil::expectation> feature_call;
    if (has_extension && has_matching_instance)
    {
        feature_call =
            NAMED_REQUIRE_CALL(api, vk_get_physical_device_features2(&device, trompeloeil::_))
                .LR_SIDE_EFFECT(
                    static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *>(_2->pNext)
                        ->swapchainMaintenance1 = has_feature ? VK_TRUE : VK_FALSE);
    }

    auto outcome =
        reader_io_discover_swapchain_maintenance1_support<decltype(environment)>(context, &device)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(
        fnfelt::result::get_success(outcome).has_value(),
        has_extension && has_matching_instance && has_feature);
}

TEST_CASE("[impure] maintenance1 discovery preserves extension errors without querying features")
{
    backend::MockApi api;
    MaintenanceSupportEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T device;
    instance_ptr instance{&instance_handle, [](auto *) {}};
    PresentationInstance context{instance, enabled_maintenance_instance_extensions()};
    REQUIRE_CALL(
        api, vk_enumerate_device_extension_properties(&device, nullptr, trompeloeil::_, nullptr))
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);

    auto outcome =
        reader_io_discover_swapchain_maintenance1_support<decltype(environment)>(context, &device)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation,
        FallibleVulkanOperation::enumerate_device_extensions);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
}

TEST_CASE(
    "[integration][headless] device maintenance1 support can be discovered without enabling it")
{
    backend::Api api;
    MaintenanceSupportEnvironment environment{std::ref(api)};
    auto properties = reader_io_vk_enumerate_instance_extension_properties<decltype(environment)>()
                          .build(environment)
                          .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(properties));
    auto names = select_presentation_instance_extensions(
        {}, fnfelt::result::get_success(std::move(properties)));
    if (fnfelt::result::is_error(names))
    {
        // An implementation without a maintenance1 instance path cannot offer this capability.
        CHECK(
            std::holds_alternative<PresentationInstanceFailure>(fnfelt::result::get_error(names)));
        return;
    }
    immer::array enabled_extensions = fnfelt::result::get_success(std::move(names));
    renderer_result<instance_ptr> instance =
        reader_io_vk_create_instance<decltype(environment)>(enabled_extensions)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));
    PresentationInstance context{
        fnfelt::result::get_success(std::move(instance)), enabled_extensions};
    auto devices =
        reader_io_vk_enumerate_physical_devices<decltype(environment)>(context.instance.get())
            .build(environment)
            .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(devices));
    REQUIRE_FALSE(fnfelt::result::get_success(devices).empty());

    // Each native query uses only advertised device paths and leaves device features disabled.
    for (VkPhysicalDevice device : fnfelt::result::get_success(devices))
    {
        auto outcome = reader_io_discover_swapchain_maintenance1_support<decltype(environment)>(
                           context, device)
                           .build(environment)
                           .sync_wait(lf::lazy_pool{2});

        REQUIRE(fnfelt::result::is_success(outcome));
        std::optional supported_extension = fnfelt::result::get_success(std::move(outcome));
        if (supported_extension.has_value())
        {
            std::string_view name = value_of(*supported_extension).view();
            CHECK(
                (name == "VK_KHR_swapchain_maintenance1" ||
                 name == "VK_EXT_swapchain_maintenance1"));
        }
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
