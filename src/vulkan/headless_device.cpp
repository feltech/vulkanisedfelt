// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./headless_device.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <variant>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array_transient.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./instance.hpp"
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
struct VkQueue_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct HeadlessEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct HeadlessFixture
{
    backend::MockApi api;
    VkInstance_T instance_handle;
    VkPhysicalDevice_T physical_device;
    bool is_instance_destroyed = false;
    trompeloeil::sequence acquisitions;
    immer::array_transient<std::shared_ptr<trompeloeil::expectation>> expectations;
    instance_ptr instance;

    HeadlessFixture()
    {
        keep(NAMED_ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction")));
        keep(NAMED_ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction")));
        keep(NAMED_REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr))
                 .LR_SIDE_EFFECT(is_instance_destroyed = true));
        instance = make_instance_ptr(&instance_handle, api);
    }

    void keep(std::unique_ptr<trompeloeil::expectation> expectation)
    {
        expectations.push_back(std::shared_ptr<trompeloeil::expectation>{std::move(expectation)});
    }

    template <std::size_t runs = 1>
    void enumerate(bool has_device = true, VkResult status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_2 = has_device ? 1U : 0U)
                 .RETURN(status)
                 .TIMES(runs));
        if (has_device && status == VK_SUCCESS)
        {
            keep(
                NAMED_REQUIRE_CALL(
                    api,
                    vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, trompeloeil::_))
                    .WITH(_3 != nullptr && *_2 == 1U)
                    .LR_SIDE_EFFECT(*_3 = &physical_device)
                    .RETURN(VK_SUCCESS)
                    .TIMES(runs));
        }
    }

    template <std::size_t runs = 1>
    void assess(VkQueueFlags flags = VK_QUEUE_GRAPHICS_BIT, VkResult extension_status = VK_SUCCESS)
    {
        // The useful queue is family one, so the composed action must preserve selection metadata.
        VkQueueFamilyProperties queue_info{};
        queue_info.queueCount = 1;
        queue_info.queueFlags = flags;
        keep(NAMED_REQUIRE_CALL(
                 api, vk_get_physical_device_properties(&physical_device, trompeloeil::_))
                 .SIDE_EFFECT(_2->deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_queue_family_properties(
                     &physical_device, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_2 = 2)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_queue_family_properties(
                     &physical_device, trompeloeil::_, trompeloeil::_))
                 .WITH(_3 != nullptr && *_2 == 2U)
                 .SIDE_EFFECT(_3[0] = VkQueueFamilyProperties{})
                 .SIDE_EFFECT(_3[1] = queue_info)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_enumerate_device_extension_properties(
                     &physical_device, nullptr, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_3 = 0)
                 .RETURN(extension_status)
                 .TIMES(runs));
    }

    void create(VkDevice device, VkResult status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
                 .WITH(_2 != nullptr && _4 != nullptr)
                 .WITH(_2->queueCreateInfoCount == 1 && _2->pQueueCreateInfos != nullptr)
                 .WITH(_2->pQueueCreateInfos[0].queueFamilyIndex == 1)
                 .WITH(_2->pQueueCreateInfos[0].queueCount == 1)
                 .WITH(_2->enabledExtensionCount == 0)
                 .SIDE_EFFECT(*_4 = device)
                 .RETURN(status)
                 .IN_SEQUENCE(acquisitions));
    }

    void queue(VkDevice device, VkQueue handle)
    {
        keep(NAMED_REQUIRE_CALL(api, vk_get_device_queue(device, 1, 0, trompeloeil::_))
                 .WITH(_4 != nullptr)
                 .SIDE_EFFECT(*_4 = handle)
                 .IN_SEQUENCE(acquisitions));
    }
};
}  // namespace

TEST_CASE("[impure] headless device setup is lazy and discarded plans retain their instance")
{
    HeadlessFixture fixture;
    HeadlessEnvironment environment{std::ref(fixture.api)};

    {
        auto plan = reader_io_create_headless_device<decltype(environment)>(
            std::move(fixture.instance), VK_QUEUE_GRAPHICS_BIT);
        [[maybe_unused]] auto deferred = plan.build(environment);

        // No discovery or creation expectation exists before the action is executed.
        CHECK_FALSE(fixture.is_instance_destroyed);
    }

    CHECK(fixture.is_instance_destroyed);
}

TEST_CASE(
    "[impure] headless device setup returns owned handles and destroys device before instance")
{
    HeadlessFixture fixture;
    HeadlessEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    VkQueue_T queue;
    bool is_device_destroyed = false;
    fixture.enumerate();
    fixture.assess();
    fixture.create(&device);
    fixture.queue(&device, &queue);
    fixture.keep(NAMED_REQUIRE_CALL(fixture.api, vk_destroy_device(&device, nullptr))
                     .LR_SIDE_EFFECT(CHECK_FALSE(fixture.is_instance_destroyed))
                     .LR_SIDE_EFFECT(is_device_destroyed = true));
    headless_device_result outcome;

    {
        auto plan = reader_io_create_headless_device<decltype(environment)>(
            std::move(fixture.instance), VK_QUEUE_GRAPHICS_BIT);

        outcome = plan.build(environment).sync_wait(lf::lazy_pool{2});
    }

    // The returned context owns the device and transitively owns the instance after plans expire.
    REQUIRE(fnfelt::result::is_success(outcome));
    auto retained = fnfelt::result::get_success(outcome);
    CHECK(retained.device.get() == &device);
    CHECK(retained.physical_device == &fixture.physical_device);
    CHECK_EQ(retained.queue_family_index, queue_family_index{1});
    CHECK(retained.queue == &queue);
    CHECK_FALSE(is_device_destroyed);
    CHECK_FALSE(fixture.is_instance_destroyed);
    std::weak_ptr<VkDevice_T> weak = retained.device;

    outcome = fnfelt::result::stopped<headless_device_result>();

    CHECK_FALSE(is_device_destroyed);

    retained.device.reset();

    CHECK(is_device_destroyed);
    CHECK(fixture.is_instance_destroyed);
    CHECK(weak.expired());
}

TEST_CASE("[impure] headless device setup preserves discovery failures without creating a device")
{
    HeadlessFixture fixture;
    HeadlessEnvironment environment{std::ref(fixture.api)};
    bool has_candidates = true;
    bool has_queue = true;
    VkResult enumeration_status = VK_SUCCESS;
    VkResult extension_status = VK_SUCCESS;
    SUBCASE("physical device enumeration fails")
    {
        enumeration_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("no physical devices are available")
    {
        has_candidates = false;
    }
    SUBCASE("no queue family satisfies the requirements")
    {
        has_queue = false;
    }
    SUBCASE("device extension discovery fails")
    {
        extension_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    fixture.enumerate(has_candidates, enumeration_status);
    if (has_candidates && enumeration_status == VK_SUCCESS)
    {
        fixture.assess(has_queue ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_COMPUTE_BIT, extension_status);
    }

    auto outcome = reader_io_create_headless_device<decltype(environment)>(
                       fixture.instance, VK_QUEUE_GRAPHICS_BIT)
                       .build(environment)
                       .sync_wait(lf::lazy_pool{2});

    // Missing create and queue expectations prove that the discovery error bypasses both effects.
    REQUIRE(fnfelt::result::is_error(outcome));
    auto const & error = fnfelt::result::get_error(outcome);
    if (enumeration_status != VK_SUCCESS || extension_status != VK_SUCCESS)
    {
        REQUIRE(std::holds_alternative<VulkanOperationFailure>(error));
        auto failure = std::get<VulkanOperationFailure>(error);
        if (enumeration_status != VK_SUCCESS)
        {
            CHECK(failure.cause.operation == FallibleVulkanOperation::enumerate_physical_devices);
            CHECK_EQ(failure.cause.result, enumeration_status);
            CHECK_FALSE(failure.device_index.has_value());
        }
        else
        {
            CHECK(failure.cause.operation == FallibleVulkanOperation::enumerate_device_extensions);
            CHECK_EQ(failure.cause.result, extension_status);
            CHECK_EQ(failure.device_index, physical_device_index{0});
        }
    }
    else
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(error));
    }
}

TEST_CASE("[impure] headless device creation failure bypasses queue retrieval")
{
    HeadlessFixture fixture;
    HeadlessEnvironment environment{std::ref(fixture.api)};
    fixture.enumerate();
    fixture.assess();
    fixture.create(VK_NULL_HANDLE, VK_ERROR_INITIALIZATION_FAILED);

    auto outcome = reader_io_create_headless_device<decltype(environment)>(
                       fixture.instance, VK_QUEUE_GRAPHICS_BIT)
                       .build(environment)
                       .sync_wait(lf::lazy_pool{2});

    // Failed acquisition has neither a queue query nor an acquired device to destroy.
    REQUIRE(fnfelt::result::is_error(outcome));
    REQUIRE(std::holds_alternative<VulkanOperationFailure>(fnfelt::result::get_error(outcome)));
    auto failure = std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome));
    CHECK(failure.cause.operation == FallibleVulkanOperation::create_device);
    CHECK_EQ(failure.cause.result, VK_ERROR_INITIALIZATION_FAILED);
    CHECK_FALSE(failure.device_index.has_value());
    CHECK_EQ(fixture.instance.use_count(), 1);
}

TEST_CASE("[impure] headless device plans acquire independent devices and queues on repeated runs")
{
    HeadlessFixture fixture;
    HeadlessEnvironment environment{std::ref(fixture.api)};
    VkDevice_T first_device;
    VkDevice_T second_device;
    VkQueue_T first_queue;
    VkQueue_T second_queue;
    fixture.enumerate<2>();
    fixture.assess<2>();
    fixture.create(&first_device);
    fixture.queue(&first_device, &first_queue);
    fixture.create(&second_device);
    fixture.queue(&second_device, &second_queue);
    fixture.keep(NAMED_REQUIRE_CALL(fixture.api, vk_destroy_device(&first_device, nullptr)));
    fixture.keep(NAMED_REQUIRE_CALL(fixture.api, vk_destroy_device(&second_device, nullptr)));
    auto plan = reader_io_create_headless_device<decltype(environment)>(
        fixture.instance, VK_QUEUE_GRAPHICS_BIT);

    auto first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    auto const & first_context = fnfelt::result::get_success(first);
    auto const & second_context = fnfelt::result::get_success(second);
    CHECK(first_context.device.get() == &first_device);
    CHECK(first_context.queue == &first_queue);
    CHECK(second_context.device.get() == &second_device);
    CHECK(second_context.queue == &second_queue);
    CHECK(first_context.physical_device == second_context.physical_device);
    CHECK_EQ(first_context.queue_family_index, second_context.queue_family_index);
}

TEST_CASE("[integration][headless] a complete headless device and queue can be created")
{
    backend::Api api;
    HeadlessEnvironment environment{std::ref(api)};
    auto instance = reader_io_vk_create_instance<decltype(environment)>()
                        .build(environment)
                        .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));

    auto outcome = reader_io_create_headless_device<decltype(environment)>(
                       fnfelt::result::get_success(instance), VK_QUEUE_GRAPHICS_BIT)
                       .build(environment)
                       .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    auto const & context = fnfelt::result::get_success(outcome);
    CHECK(context.device.get() != VK_NULL_HANDLE);
    CHECK(context.physical_device != VK_NULL_HANDLE);
    CHECK(context.queue != VK_NULL_HANDLE);
    // No GPU work was submitted; the complete context can release its owners at scope exit.
}
}  // namespace vulkandemo::vulkan

namespace vulkandemo::backend
{
namespace
{
template <class TApi>
struct ApiEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct DeviceQueryApi
{
    VkResult vk_enumerate_physical_devices(VkInstance, std::uint32_t *, VkPhysicalDevice *);
    void vk_get_physical_device_properties(VkPhysicalDevice, VkPhysicalDeviceProperties *);
    void vk_get_physical_device_queue_family_properties(
        VkPhysicalDevice, std::uint32_t *, VkQueueFamilyProperties *);
    VkResult vk_enumerate_device_extension_properties(
        VkPhysicalDevice, char const *, std::uint32_t *, VkExtensionProperties *);
};

static_assert(!vulkan::create_headless_device_environment<ApiEnvironment<DeviceQueryApi>>);
static_assert(vulkan::create_headless_device_environment<ApiEnvironment<Api>>);
static_assert(vulkan::create_headless_device_environment<ApiEnvironment<MockApi>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
