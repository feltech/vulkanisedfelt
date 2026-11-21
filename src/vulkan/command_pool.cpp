// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./command_pool.hpp"

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
#include "./queue_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct CommandPoolEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] command pool plans are lazy and retain their device")
{
    backend::MockApi api;
    CommandPoolEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};

    {
        // Neither planning nor binding an environment acquires a native pool.
        auto plan = reader_io_vk_create_command_pool<CommandPoolEnvironment>(
            std::move(device), queue_family_index{4});
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
    }

    CHECK(device_observer.expired());
}

TEST_CASE("[impure] command pools use the requested family and destroy before their device")
{
    backend::MockApi api;
    CommandPoolEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    std::weak_ptr device_observer{device};
    VkCommandPoolCreateInfo captured{};
    REQUIRE_CALL(
        api, vk_create_command_pool(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .SIDE_EFFECT(*_4 = pool_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_command_pool(device_handle, pool_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // Execution produces a pool independently retaining its logical device.
    vulkan_result<command_pool_ptr> outcome =
        reader_io_vk_create_command_pool<CommandPoolEnvironment>(
            std::move(device), queue_family_index{4})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    CHECK_EQ(captured.queueFamilyIndex, 4U);
    command_pool_ptr command_pool = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(command_pool.get(), pool_handle);
    command_pool_ptr another_owner{command_pool};
    std::weak_ptr pool_observer{command_pool};
    command_pool.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(device_observer.expired());

    another_owner.reset();

    CHECK_EQ(cleanup_step, 2);
    CHECK(pool_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] command pool failures preserve status without destroying an unacquired pool")
{
    backend::MockApi api;
    CommandPoolEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    REQUIRE_CALL(api, vk_create_command_pool(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // Failed acquisition releases its retained input without native pool destruction.
    vulkan_result<command_pool_ptr> outcome =
        reader_io_vk_create_command_pool<CommandPoolEnvironment>(
            std::move(device), queue_family_index{2})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_command_pool);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(device_observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
