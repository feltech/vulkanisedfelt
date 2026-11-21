// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./semaphore.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SemaphoreEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] semaphore plans are lazy and retain their device")
{
    backend::MockApi api;
    SemaphoreEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};

    {
        // Planning and binding capabilities retain the device without acquiring a semaphore.
        auto plan = reader_io_vk_create_semaphore<SemaphoreEnvironment>(std::move(device));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
    }

    CHECK(device_observer.expired());
}

TEST_CASE("[impure] binary semaphores destroy before their retained device")
{
    backend::MockApi api;
    SemaphoreEnvironment environment{std::ref(api)};
    char device_storage{}, semaphore_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkSemaphore semaphore_handle = reinterpret_cast<VkSemaphore>(&semaphore_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    std::weak_ptr device_observer{device};
    VkSemaphoreCreateInfo captured{};
    REQUIRE_CALL(api, vk_create_semaphore(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .SIDE_EFFECT(*_4 = semaphore_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_semaphore(device_handle, semaphore_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // Execution acquires the native object with its default initial state.
    vulkan_result<semaphore_ptr> outcome =
        reader_io_vk_create_semaphore<SemaphoreEnvironment>(std::move(device))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    semaphore_ptr semaphore = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(semaphore.get(), semaphore_handle);
    semaphore_ptr another_owner{semaphore};
    std::weak_ptr semaphore_observer{semaphore};
    semaphore.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(device_observer.expired());

    // The native object and its device are released even while a weak observer remains.
    another_owner.reset();

    CHECK_EQ(cleanup_step, 2);
    CHECK(semaphore_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] semaphore creation failures preserve status without native destruction")
{
    backend::MockApi api;
    SemaphoreEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    REQUIRE_CALL(api, vk_create_semaphore(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // No owner is published, so only the retained input is released.
    vulkan_result<semaphore_ptr> outcome =
        reader_io_vk_create_semaphore<SemaphoreEnvironment>(std::move(device))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_semaphore);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(device_observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
