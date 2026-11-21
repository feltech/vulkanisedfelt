// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./command_buffer.hpp"

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
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./headless_device.hpp"
#include "./headless_device_types.hpp"
#include "./instance.hpp"
#include "./managed_handles.hpp"
#include "./managed_handles_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct CommandBufferEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] beginning command buffer recording is lazy")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char buffer_storage{};
    command_buffer_ptr command_buffer{
        reinterpret_cast<VkCommandBuffer>(&buffer_storage), [](auto) {}};
    std::weak_ptr buffer_observer{command_buffer};

    {
        // Planning and binding retain the buffer without changing its native state.
        auto plan =
            reader_io_vk_begin_command_buffer<decltype(environment)>(std::move(command_buffer));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(buffer_observer.expired());
    }

    CHECK(buffer_observer.expired());
}

TEST_CASE("[impure] primary command buffers begin one-time recording and retain ownership")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char buffer_storage{};
    VkCommandBuffer buffer_handle = reinterpret_cast<VkCommandBuffer>(&buffer_storage);
    bool is_buffer_released = false;
    command_buffer_ptr command_buffer{buffer_handle, [&](auto) { is_buffer_released = true; }};
    VkCommandBufferBeginInfo captured{};
    REQUIRE_CALL(api, vk_begin_command_buffer(buffer_handle, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .RETURN(VK_SUCCESS);

    // Successful begin forwards ownership while the native buffer enters recording state.
    vulkan_result<command_buffer_ptr> outcome =
        reader_io_vk_begin_command_buffer<decltype(environment)>(std::move(command_buffer))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    CHECK(captured.pInheritanceInfo == nullptr);
    command_buffer_ptr recording_buffer = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(recording_buffer.get(), buffer_handle);
    CHECK_FALSE(is_buffer_released);

    recording_buffer.reset();

    CHECK(is_buffer_released);
}

TEST_CASE("[impure] beginning command buffer failures preserve the native error")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char buffer_storage{};
    command_buffer_ptr command_buffer{
        reinterpret_cast<VkCommandBuffer>(&buffer_storage), [](auto) {}};
    std::weak_ptr buffer_observer{command_buffer};
    REQUIRE_CALL(api, vk_begin_command_buffer(command_buffer.get(), trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);

    // Failed begin publishes no recording owner and releases the retained input.
    vulkan_result<command_buffer_ptr> outcome =
        reader_io_vk_begin_command_buffer<decltype(environment)>(std::move(command_buffer))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation,
        FallibleVulkanOperation::begin_command_buffer);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(buffer_observer.expired());
}

TEST_CASE("[impure] command buffer plans are lazy and retain their pool and device")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    command_pool_ptr command_pool{reinterpret_cast<VkCommandPool>(&pool_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr pool_observer{command_pool};

    {
        // A bound plan retains inputs without allocating or freeing command buffers.
        auto plan = reader_io_vk_allocate_command_buffer<decltype(environment)>(
            std::move(device), std::move(command_pool));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(pool_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(pool_observer.expired());
}

TEST_CASE("[impure] primary command buffers free before their pool and device")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{}, buffer_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    VkCommandBuffer buffer_handle = reinterpret_cast<VkCommandBuffer>(&buffer_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 3); }};
    command_pool_ptr command_pool = make_command_pool_ptr(pool_handle, device, api);
    std::weak_ptr device_observer{device};
    std::weak_ptr pool_observer{command_pool};
    VkCommandBufferAllocateInfo captured{};
    REQUIRE_CALL(api, vk_allocate_command_buffers(device_handle, trompeloeil::_, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .SIDE_EFFECT(*_3 = buffer_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_free_command_buffers(device_handle, pool_handle, 1, trompeloeil::_))
        .WITH(*_4 == buffer_handle)
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));
    REQUIRE_CALL(api, vk_destroy_command_pool(device_handle, pool_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 2));

    // A single primary buffer begins in the native initial state and owns both parents.
    vulkan_result<command_buffer_ptr> outcome =
        reader_io_vk_allocate_command_buffer<decltype(environment)>(
            std::move(device), std::move(command_pool))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.commandPool, pool_handle);
    CHECK_EQ(captured.level, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CHECK_EQ(captured.commandBufferCount, 1U);
    command_buffer_ptr command_buffer = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(command_buffer.get(), buffer_handle);
    command_buffer_ptr another_owner{command_buffer};
    std::weak_ptr buffer_observer{command_buffer};
    command_buffer.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(pool_observer.expired());
    CHECK_FALSE(device_observer.expired());

    // Final release is serialized with all other host access to the same pool.
    another_owner.reset();

    CHECK_EQ(cleanup_step, 3);
    CHECK(buffer_observer.expired());
    CHECK(pool_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] failed command buffer allocation preserves status without a second free")
{
    backend::MockApi api;
    CommandBufferEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    device_ptr device{device_handle, [](auto) {}};
    command_pool_ptr command_pool = make_command_pool_ptr(pool_handle, device, api);
    std::weak_ptr device_observer{device};
    std::weak_ptr pool_observer{command_pool};
    REQUIRE_CALL(api, vk_allocate_command_buffers(device_handle, trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(*_3 = VK_NULL_HANDLE)
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    REQUIRE_CALL(api, vk_destroy_command_pool(device_handle, pool_handle, nullptr));

    // Vulkan has already freed any partial allocation; no buffer owner is published.
    vulkan_result<command_buffer_ptr> outcome =
        reader_io_vk_allocate_command_buffer<decltype(environment)>(
            std::move(device), std::move(command_pool))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation,
        FallibleVulkanOperation::allocate_command_buffer);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(pool_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[integration][headless] a primary command buffer can be allocated and freed")
{
    backend::Api api;
    CommandBufferEnvironment environment{std::ref(api)};
    lf::lazy_pool workers{2};
    renderer_result<instance_ptr> instance =
        reader_io_vk_create_instance<decltype(environment)>().build(environment).sync_wait(workers);
    REQUIRE(fnfelt::result::is_success(instance));
    headless_device_result device =
        reader_io_create_headless_device<decltype(environment)>(
            fnfelt::result::get_success(instance), VK_QUEUE_GRAPHICS_BIT)
            .build(environment)
            .sync_wait(workers);
    REQUIRE(fnfelt::result::is_success(device));
    HeadlessDevice const & headless_device = fnfelt::result::get_success(device);
    vulkan_result<command_pool_ptr> command_pool =
        reader_io_vk_create_command_pool<decltype(environment)>(
            headless_device.device, headless_device.queue_family_index)
            .build(environment)
            .sync_wait(workers);
    REQUIRE(fnfelt::result::is_success(command_pool));

    // Allocation and cleanup are serialized; the buffer is never recorded or submitted.
    vulkan_result<command_buffer_ptr> outcome =
        reader_io_vk_allocate_command_buffer<decltype(environment)>(
            headless_device.device, fnfelt::result::get_success(command_pool))
            .build(environment)
            .sync_wait(workers);

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome) != nullptr);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
