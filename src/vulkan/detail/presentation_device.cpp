// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_device.hpp"

#include <algorithm>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <string_view>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../../backend/mock_api.hpp"
#include "../errors.hpp"
#include "../extension_types.hpp"
#include "../managed_handles.hpp"
#include "../physical_device_selection_types.hpp"
#include "../queue_types.hpp"

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};
struct VkDevice_T
{
};

#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto presentation_device_extension_names(immer::array<text::String> required_extension_names)
    -> immer::array<text::String>
{
    auto extension_names = immer::array{text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}.transient();
    for (text::String const & name : required_extension_names)
    {
        if (std::ranges::find(extension_names, name) == extension_names.end())
        {
            extension_names.push_back(name);
        }
    }
    return std::move(extension_names).persistent();
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
TEST_CASE("[pure] presentation device extensions contain swapchain and distinct caller names")
{
    immer::array requested{
        text::String{"VK_extra"},
        text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
        text::String{"VK_extra"}};

    immer::array names{detail::presentation_device_extension_names(requested)};

    REQUIRE_EQ(names.size(), 2);
    CHECK_EQ(names[0].view(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    CHECK_EQ(names[1].view(), "VK_extra");
    CHECK_EQ(requested.size(), 3);
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan::detail
{
namespace
{
struct PresentationDeviceEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] maintenance1 device acquisition retains its selection lazily")
{
    backend::MockApi api;
    PresentationDeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T physical_device;
    char surface_storage{};
    bool is_surface_destroyed = false;
    REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr));
    instance_ptr instance = make_instance_ptr(&instance_handle, api);
    surface_ptr surface{
        reinterpret_cast<VkSurfaceKHR>(&surface_storage),
        [&](auto) { is_surface_destroyed = true; }};
    std::weak_ptr<VkInstance_T> instance_observer = instance;
    SelectedPresentationPhysicalDevice selected{
        std::move(instance),
        std::move(surface),
        &physical_device,
        {queue_family_index{2}, queue_family_index{2}},
        extension_name{"VK_EXT_swapchain_maintenance1"}};

    {
        auto plan = create_presentation_device_reader_io_action(
            std::move(selected), immer::array<text::String>{}, environment);
        [[maybe_unused]] auto deferred = plan.build();

        // No creation expectation exists: constructing IO must only retain its inputs.
        CHECK_FALSE(instance_observer.expired());
        CHECK_FALSE(is_surface_destroyed);
    }

    CHECK(instance_observer.expired());
    CHECK(is_surface_destroyed);
}

TEST_CASE("[impure] maintenance1 devices enable the selected path and retain native parents")
{
    bool has_shared_family = true;
    std::string_view maintenance1_name = "VK_KHR_swapchain_maintenance1";
    SUBCASE("KHR feature with one shared queue family") {}
    SUBCASE("EXT feature with separate queue families")
    {
        has_shared_family = false;
        maintenance1_name = "VK_EXT_swapchain_maintenance1";
    }
    backend::MockApi api;
    PresentationDeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T physical_device;
    VkDevice_T device_handle;
    char surface_storage{};
    bool is_device_destroyed = false;
    bool is_instance_destroyed = false;
    REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK(is_device_destroyed))
        .LR_SIDE_EFFECT(is_instance_destroyed = true);
    REQUIRE_CALL(api, vk_destroy_device(&device_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_FALSE(is_instance_destroyed))
        .LR_SIDE_EFFECT(is_device_destroyed = true);
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(_2 != nullptr && _2->pNext != nullptr && _4 != nullptr)
        .LR_SIDE_EFFECT({
            // Inspect the call-local chain synchronously, while all native pointers are valid.
            auto const * feature =
                static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT const *>(_2->pNext);
            CHECK_EQ(_2->sType, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
            CHECK_EQ(
                feature->sType,
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
            CHECK(feature->pNext == nullptr);
            CHECK_EQ(feature->swapchainMaintenance1, VK_TRUE);
            CHECK(_2->pEnabledFeatures == nullptr);
            CHECK_EQ(_2->queueCreateInfoCount, has_shared_family ? 1U : 2U);
            CHECK_EQ(_2->pQueueCreateInfos[0].queueFamilyIndex, 7U);
            CHECK_EQ(_2->pQueueCreateInfos[0].queueCount, 1U);
            CHECK_EQ(_2->pQueueCreateInfos[0].pQueuePriorities[0], 1.0f);
            if (!has_shared_family)
            {
                CHECK_EQ(_2->pQueueCreateInfos[1].queueFamilyIndex, 2U);
                CHECK_EQ(_2->pQueueCreateInfos[1].queueCount, 1U);
            }
            CHECK_EQ(_2->enabledExtensionCount, 3U);
            CHECK_EQ(
                std::string_view{_2->ppEnabledExtensionNames[0]}, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            CHECK_EQ(
                std::string_view{_2->ppEnabledExtensionNames[1]},
                has_shared_family ? maintenance1_name : std::string_view{"VK_extra"});
            CHECK_EQ(
                std::string_view{_2->ppEnabledExtensionNames[2]},
                has_shared_family ? std::string_view{"VK_extra"} : maintenance1_name);
            *_4 = &device_handle;
        })
        .RETURN(VK_SUCCESS);
    instance_ptr instance = make_instance_ptr(&instance_handle, api);
    surface_ptr surface{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}};
    SelectedPresentationPhysicalDevice selected{
        std::move(instance),
        std::move(surface),
        &physical_device,
        {queue_family_index{7}, queue_family_index{has_shared_family ? 7U : 2U}},
        extension_name{text::String{maintenance1_name}}};
    device_ptr owner;
    std::weak_ptr<VkDevice_T> device_observer;

    immer::array requested_names{
        text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME}, text::String{"VK_extra"}};
    if (has_shared_family)
    {
        requested_names = immer::array{
            text::String{maintenance1_name},
            text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
            text::String{"VK_extra"},
            text::String{maintenance1_name}};
    }
    {
        auto plan = create_presentation_device_reader_io_action(
            std::move(selected), std::move(requested_names), environment);
        vulkan_result<device_ptr> outcome = plan.build().sync_wait(lf::lazy_pool{2});

        REQUIRE(fnfelt::result::is_success(outcome));
        owner = fnfelt::result::get_success(outcome);
        device_observer = owner;
    }

    // The last returned owner, rather than weak observers, governs both native cleanup calls.
    CHECK_FALSE(is_device_destroyed);
    CHECK_FALSE(is_instance_destroyed);
    device_ptr last_owner = owner;
    owner.reset();
    CHECK_FALSE(is_device_destroyed);
    last_owner.reset();
    CHECK(device_observer.expired());
    CHECK(is_device_destroyed);
    CHECK(is_instance_destroyed);
}

TEST_CASE("[impure] maintenance1 device creation preserves native failures")
{
    backend::MockApi api;
    PresentationDeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance_handle;
    VkPhysicalDevice_T physical_device;
    REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr));
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_FEATURE_NOT_PRESENT);
    SelectedPresentationPhysicalDevice selected{
        make_instance_ptr(&instance_handle, api),
        {},
        &physical_device,
        {queue_family_index{1}, queue_family_index{1}},
        extension_name{"VK_EXT_swapchain_maintenance1"}};

    vulkan_result<device_ptr> outcome =
        create_presentation_device_reader_io_action(
            std::move(selected), immer::array<text::String>{}, environment)
            .build()
            .sync_wait(lf::lazy_pool{2});

    // No device was acquired, so no device destruction is expected.
    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(fnfelt::result::get_error(outcome).operation == FallibleVulkanOperation::create_device);
}
}  // namespace vulkandemo::vulkan::detail
#endif  // DOCTEST_CONFIG_DISABLE
