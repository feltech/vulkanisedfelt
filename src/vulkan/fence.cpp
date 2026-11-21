// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./fence.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./headless_device.hpp"
#include "./headless_device_types.hpp"
#include "./instance.hpp"
#include "./managed_handles_types.hpp"
#include "./renderer_errors.hpp"
#include "./semaphore.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct FenceEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};

struct SynchronizationEnvironment
{
    std::reference_wrapper<backend::Api> api;
};
}  // namespace

TEST_CASE("[impure] fence plans are lazy and retain their device")
{
    backend::MockApi api;
    FenceEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};

    {
        // Planning and binding capabilities retain the device without acquiring a fence.
        auto plan = reader_io_vk_create_fence<FenceEnvironment>(std::move(device));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
    }

    CHECK(device_observer.expired());
}

TEST_CASE("[impure] unsignaled fences destroy before their retained device")
{
    backend::MockApi api;
    FenceEnvironment environment{std::ref(api)};
    char device_storage{}, fence_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkFence fence_handle = reinterpret_cast<VkFence>(&fence_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    std::weak_ptr device_observer{device};
    VkFenceCreateInfo captured{};
    REQUIRE_CALL(api, vk_create_fence(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .SIDE_EFFECT(*_4 = fence_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_fence(device_handle, fence_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // Execution acquires the native object with its default initial state.
    vulkan_result<fence_ptr> outcome =
        reader_io_vk_create_fence<FenceEnvironment>(std::move(device))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    fence_ptr fence = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(fence.get(), fence_handle);
    fence_ptr another_owner{fence};
    std::weak_ptr fence_observer{fence};
    fence.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(device_observer.expired());

    // The native object and its device are released even while a weak observer remains.
    another_owner.reset();

    CHECK_EQ(cleanup_step, 2);
    CHECK(fence_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] fence creation failures preserve status without native destruction")
{
    backend::MockApi api;
    FenceEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    REQUIRE_CALL(api, vk_create_fence(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // No owner is published, so only the retained input is released.
    vulkan_result<fence_ptr> outcome =
        reader_io_vk_create_fence<FenceEnvironment>(std::move(device))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_fence);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(device_observer.expired());
}

TEST_CASE("[integration][headless] binary semaphore and unsignaled fence owners can be released")
{
    backend::Api api;
    SynchronizationEnvironment environment{std::ref(api)};
    lf::lazy_pool workers{2};
    renderer_result<instance_ptr> instance =
        reader_io_vk_create_instance<SynchronizationEnvironment>()
            .build(environment)
            .sync_wait(workers);
    REQUIRE(fnfelt::result::is_success(instance));
    headless_device_result device =
        reader_io_create_headless_device<SynchronizationEnvironment>(
            fnfelt::result::get_success(instance), VK_QUEUE_GRAPHICS_BIT)
            .build(environment)
            .sync_wait(workers);
    REQUIRE(fnfelt::result::is_success(device));
    HeadlessDevice const & headless_device = fnfelt::result::get_success(device);

    // Neither synchronization object is used by submitted work, so cleanup needs no wait.
    vulkan_result<semaphore_ptr> semaphore =
        reader_io_vk_create_semaphore<SynchronizationEnvironment>(headless_device.device)
            .build(environment)
            .sync_wait(workers);
    vulkan_result<fence_ptr> fence =
        reader_io_vk_create_fence<SynchronizationEnvironment>(headless_device.device)
            .build(environment)
            .sync_wait(workers);

    REQUIRE(fnfelt::result::is_success(semaphore));
    REQUIRE(fnfelt::result::is_success(fence));
    CHECK(fnfelt::result::get_success(semaphore) != nullptr);
    CHECK(fnfelt::result::get_success(fence) != nullptr);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
