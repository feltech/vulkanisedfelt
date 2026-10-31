// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./device.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./instance.hpp"
#include "./physical_devices.hpp"
#include "./queue_family_properties.hpp"
#include "./strong_types.hpp"

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};
struct VkDevice_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
static_assert(!std::is_convertible_v<std::uint32_t, queue_family_index>);
static_assert(!std::is_convertible_v<queue_family_index, std::uint32_t>);

template <class TApi>
struct DeviceEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] device plans retain their parent without eagerly creating a device")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T physical_device;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr));
    auto parent = make_instance_ptr(&instance, api);

    {
        auto plan = reader_io_vk_create_device<decltype(environment)>(
            parent, &physical_device, queue_family_index{2});
        [[maybe_unused]] auto deferred = plan.build(environment);
        parent.reset();

        // No create expectation exists: building or discarding the plan must not create a device.
    }
    // Releasing the last plan performs the expected cleanup of its retained parent.
}

TEST_CASE("[impure] device creation preserves failure without destroying a device")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T physical_device;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction"));
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr));
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_INITIALIZATION_FAILED);
    auto parent = make_instance_ptr(&instance, api);

    auto result = reader_io_vk_create_device<decltype(environment)>(
                      parent, &physical_device, queue_family_index{1})
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK_EQ(fnfelt::result::get_error(result).result, VK_ERROR_INITIALIZATION_FAILED);
    CHECK(fnfelt::result::get_error(result).operation == FallibleVulkanOperation::create_device);
    CHECK_EQ(parent.use_count(), 1);
}

TEST_CASE("[impure] device owners release the parent before weak owners expire")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T physical_device;
    VkDevice_T device;
    bool is_device_destroyed = false;
    bool is_instance_destroyed = false;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction"));
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(_2 != nullptr && _4 != nullptr)
        .WITH(
            _2->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO && _2->pNext == nullptr &&
            _2->flags == 0)
        .WITH(_2->queueCreateInfoCount == 1 && _2->pQueueCreateInfos != nullptr)
        .WITH(_2->enabledLayerCount == 0 && _2->ppEnabledLayerNames == nullptr)
        .WITH(_2->enabledExtensionCount == 0 && _2->ppEnabledExtensionNames == nullptr)
        .WITH(_2->pEnabledFeatures == nullptr)
        .WITH(_2->pQueueCreateInfos[0].sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
        .WITH(_2->pQueueCreateInfos[0].pNext == nullptr && _2->pQueueCreateInfos[0].flags == 0)
        .WITH(
            _2->pQueueCreateInfos[0].queueFamilyIndex == 7 &&
            _2->pQueueCreateInfos[0].queueCount == 1)
        .WITH(
            _2->pQueueCreateInfos[0].pQueuePriorities != nullptr &&
            _2->pQueueCreateInfos[0].pQueuePriorities[0] == 1.0f)
        .LR_SIDE_EFFECT(*_4 = &device)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_device(&device, nullptr))
        .LR_SIDE_EFFECT(CHECK_FALSE(is_instance_destroyed))
        .LR_SIDE_EFFECT(is_device_destroyed = true);
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr))
        .LR_SIDE_EFFECT(CHECK(is_device_destroyed))
        .LR_SIDE_EFFECT(is_instance_destroyed = true);
    device_ptr owner;
    std::weak_ptr<VkDevice_T> weak;

    {
        auto parent = make_instance_ptr(&instance, api);
        auto plan = reader_io_vk_create_device<decltype(environment)>(
            std::move(parent), &physical_device, queue_family_index{7});
        auto result = plan.build(environment).sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(result));
        owner = fnfelt::result::get_success(result);
        weak = owner;
    }

    CHECK_FALSE(is_device_destroyed);
    CHECK_FALSE(is_instance_destroyed);
    CHECK(owner.get() == &device);
    auto copy = owner;
    owner.reset();

    CHECK_FALSE(is_device_destroyed);

    copy.reset();

    // Weak ownership preserves the control block, but must not defer either API cleanup call.
    CHECK(weak.expired());
    CHECK(is_device_destroyed);
    CHECK(is_instance_destroyed);
    weak.reset();
}

TEST_CASE("[impure] repeated device creation acquires independent owners")
{
    backend::MockApi api;
    DeviceEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkPhysicalDevice_T physical_device;
    VkDevice_T first_device;
    VkDevice_T second_device;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction"));
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr));
    REQUIRE_CALL(api, vk_destroy_device(&first_device, nullptr));
    REQUIRE_CALL(api, vk_destroy_device(&second_device, nullptr));
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_4 = &first_device)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_4 = &second_device)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    auto parent = make_instance_ptr(&instance, api);
    auto plan = reader_io_vk_create_device<decltype(environment)>(
        parent, &physical_device, queue_family_index{0});

    auto first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    CHECK(fnfelt::result::get_success(first).get() == &first_device);
    CHECK(fnfelt::result::get_success(second).get() == &second_device);
}

TEST_CASE("[integration][headless] a device can be created and destroyed")
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
    REQUIRE_FALSE(fnfelt::result::get_success(devices).empty());
    auto families = reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(
                        fnfelt::result::get_success(devices)[0])
                        .build(environment)
                        .sync_wait(lf::lazy_pool{2});
    std::uint32_t family_index = 0;
    while (family_index < families.size() && families[family_index].queueCount == 0)
    {
        ++family_index;
    }
    REQUIRE_LT(family_index, families.size());

    auto device = reader_io_vk_create_device<decltype(environment)>(
                      fnfelt::result::get_success(instance),
                      fnfelt::result::get_success(devices)[0],
                      queue_family_index{family_index})
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(device));
    CHECK(fnfelt::result::get_success(device).get() != VK_NULL_HANDLE);
    // No work or device children were created; final scope release can destroy immediately.
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
