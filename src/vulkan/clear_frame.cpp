// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./clear_frame.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./queue_family_selection_types.hpp"
#include "./queue_types.hpp"
#include "./submit_and_present_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct ClearFrameEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
    PresentationFramebuffers targets;
    command_pool_ptr command_pool;
};

struct ClearFrameFixture
{
    backend::MockApi api;
    char device_storage{}, queue_storage{}, surface_storage{}, swapchain_storage{}, pool_storage{};
    char framebuffer_storage{}, second_framebuffer_storage{}, render_pass_storage{},
        buffer_storage{};
    char first_fence_storage{}, second_fence_storage{}, third_fence_storage{}, semaphore_storage{};
    PresentationFramebuffers targets;
    command_pool_ptr pool{reinterpret_cast<VkCommandPool>(&pool_storage), [](auto) {}};
    immer::array<VkFence> fences{
        reinterpret_cast<VkFence>(&first_fence_storage),
        reinterpret_cast<VkFence>(&second_fence_storage),
        reinterpret_cast<VkFence>(&third_fence_storage)};
    VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(&semaphore_storage);
    std::atomic<int> created_fences{0};

    ClearFrameFixture()
    {
        auto & swapchain =
            targets.presentation_image_views.presentation_swapchain_images.presentation_swapchain;
        swapchain.presentation_device = PresentationDevice{
            {reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}},
            {reinterpret_cast<VkDevice>(&device_storage), [](auto) {}},
            {},
            PresentationQueueFamilies{queue_family_index{0}, queue_family_index{0}},
            reinterpret_cast<VkQueue>(&queue_storage),
            reinterpret_cast<VkQueue>(&queue_storage)};
        swapchain.swapchain = {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
        swapchain.configuration.extent = VkExtent2D{32, 24};
        targets.render_pass = {reinterpret_cast<VkRenderPass>(&render_pass_storage), [](auto) {}};
        targets.framebuffers = immer::array<framebuffer_ptr>{
            {reinterpret_cast<VkFramebuffer>(&framebuffer_storage), [](auto) {}},
            {reinterpret_cast<VkFramebuffer>(&second_framebuffer_storage), [](auto) {}}};
    }

    auto operation()
    {
        return reader_io_clear_and_present<ClearFrameEnvironment>(
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}}, timeout_nanoseconds{1'000'000'000});
    }

    auto environment() -> ClearFrameEnvironment
    {
        return {std::ref(api), targets, pool};
    }
};
}  // namespace

TEST_CASE("[impure] bounded clear frame completes acquisition before recording and presentation")
{
    ClearFrameFixture fixture;
    ClearFrameEnvironment environment = fixture.environment();
    trompeloeil::sequence execution;
    VkFence acquisition_fence = VK_NULL_HANDLE;
    VkFence submission_fence = VK_NULL_HANDLE;
    VkFence presentation_fence = VK_NULL_HANDLE;
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(3)
        .LR_SIDE_EFFECT(*_4 = fixture.fences[fixture.created_fences++])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_fence(trompeloeil::_, trompeloeil::_, nullptr)).TIMES(3);
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_4 = fixture.semaphore)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_semaphore(trompeloeil::_, fixture.semaphore, nullptr));
    REQUIRE_CALL(
        fixture.api,
        vk_acquire_next_image_khr(
            trompeloeil::_, trompeloeil::_, 1'000'000'000, nullptr, trompeloeil::_, trompeloeil::_))
        .LR_SIDE_EFFECT({
            acquisition_fence = _5;
            *_6 = 1;
        })
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUBOPTIMAL_KHR);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == acquisition_fence)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_allocate_command_buffers(trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_WITH(_2->commandPool == environment.command_pool.get())
        .LR_SIDE_EFFECT(*_3 = reinterpret_cast<VkCommandBuffer>(&fixture.buffer_storage))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api,
        vk_free_command_buffers(trompeloeil::_, fixture.pool.get(), 1, trompeloeil::_));
    REQUIRE_CALL(fixture.api, vk_begin_command_buffer(trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api,
        vk_cmd_begin_render_pass(trompeloeil::_, trompeloeil::_, VK_SUBPASS_CONTENTS_INLINE))
        .LR_WITH(_2->framebuffer == fixture.targets.framebuffers[1].get())
        .IN_SEQUENCE(execution);
    REQUIRE_CALL(fixture.api, vk_cmd_end_render_pass(trompeloeil::_)).IN_SEQUENCE(execution);
    REQUIRE_CALL(fixture.api, vk_end_command_buffer(trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .LR_SIDE_EFFECT(submission_fence = _4)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == submission_fence)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(trompeloeil::_, trompeloeil::_))
        .LR_SIDE_EFFECT(
            presentation_fence =
                *static_cast<VkSwapchainPresentFenceInfoEXT const *>(_2->pNext)->pFences)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == presentation_fence)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);

    // The complete plan is lazy; each stage begins only after the preceding required completion.
    auto operation = fixture.operation();
    CHECK_EQ(fixture.created_fences.load(std::memory_order_relaxed), 0);
    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        std::move(operation).build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(fnfelt::result::get_success(outcome).image_index, swapchain_image_index{1});
    CHECK(fnfelt::result::get_success(outcome).is_suboptimal);
    CHECK_EQ(fixture.created_fences.load(std::memory_order_relaxed), 3);
}

TEST_CASE(
    "[impure] failed clear recording releases its abandoned swapchain after acquisition completion")
{
    ClearFrameFixture fixture;
    bool has_acquisition_completed = false;
    bool has_swapchain_been_destroyed = false;
    auto & swapchain = fixture.targets.presentation_image_views.presentation_swapchain_images
                           .presentation_swapchain.swapchain;
    swapchain = {
        reinterpret_cast<VkSwapchainKHR>(&fixture.swapchain_storage),
        [&](auto)
        {
            CHECK(has_acquisition_completed);
            has_swapchain_been_destroyed = true;
        }};
    std::weak_ptr swapchain_observer{swapchain};
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(3)
        .LR_SIDE_EFFECT(*_4 = fixture.fences[fixture.created_fences++])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_fence(trompeloeil::_, trompeloeil::_, nullptr)).TIMES(3);
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_4 = fixture.semaphore)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_semaphore(trompeloeil::_, fixture.semaphore, nullptr));
    trompeloeil::sequence execution;
    REQUIRE_CALL(
        fixture.api,
        vk_acquire_next_image_khr(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            nullptr,
            trompeloeil::_,
            trompeloeil::_))
        .SIDE_EFFECT(*_6 = 0)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_SIDE_EFFECT(has_acquisition_completed = true)
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_allocate_command_buffers(trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);

    // Move the only target owner into the execution environment.
    ClearFrameEnvironment environment{
        std::ref(fixture.api), std::move(fixture.targets), fixture.pool};
    auto operation = reader_io_clear_and_present<ClearFrameEnvironment>(
        VkClearColorValue{}, timeout_nanoseconds{100});
    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        std::move(operation).build(std::move(environment)).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation,
        FallibleVulkanOperation::allocate_command_buffer);
    CHECK(has_acquisition_completed);
    CHECK(has_swapchain_been_destroyed);
    CHECK(swapchain_observer.expired());
}
TEST_CASE(
    "[impure] bounded clear frame creates fresh signals on replay and skips waits when acquisition "
    "fails")
{
    ClearFrameFixture fixture;
    ClearFrameEnvironment environment = fixture.environment();
    VkResult status = VK_TIMEOUT;
    SUBCASE("acquisition timeout") {}
    SUBCASE("image not ready")
    {
        status = VK_NOT_READY;
    }
    SUBCASE("out of date")
    {
        status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(9)
        .LR_SIDE_EFFECT(*_4 = fixture.fences[fixture.created_fences++ % 3])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_fence(trompeloeil::_, trompeloeil::_, nullptr)).TIMES(9);
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(3)
        .LR_SIDE_EFFECT(*_4 = fixture.semaphore)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_semaphore(trompeloeil::_, fixture.semaphore, nullptr))
        .TIMES(3);
    REQUIRE_CALL(
        fixture.api,
        vk_acquire_next_image_khr(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            nullptr,
            trompeloeil::_,
            trompeloeil::_))
        .TIMES(3)
        .RETURN(status);

    // Each execution owns newly created synchronization even though acquisition publishes no image.
    auto operation = fixture.operation();
    lf::lazy_pool scheduler{1};
    completed_presentation_result first =
        operation.build(environment).io().build().sync_wait(scheduler);
    completed_presentation_result second =
        operation.build(environment).io().build().sync_wait(scheduler);
    completed_presentation_result third =
        std::move(operation).build(environment).io().build().sync_wait(scheduler);

    for (auto const & outcome : {first, second, third})
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    }
    CHECK_EQ(fixture.created_fences.load(std::memory_order_relaxed), 9);
}

TEST_CASE("[impure] bounded clear frame stops before acquisition when signal creation fails")
{
    ClearFrameFixture fixture;
    ClearFrameEnvironment environment = fixture.environment();
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(3)
        .LR_SIDE_EFFECT({
            if (fixture.created_fences.fetch_add(1, std::memory_order_relaxed) == 0)
            {
                *_4 = fixture.fences[0];
            }
        })
        .RETURN(*_4 != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY);
    REQUIRE_CALL(fixture.api, vk_destroy_fence(trompeloeil::_, fixture.fences[0], nullptr));
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        fixture.operation().build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_fence);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
}
TEST_CASE("[impure] bounded clear frame abandons acquisition after a device-loss completion")
{
    ClearFrameFixture fixture;
    ClearFrameEnvironment environment = fixture.environment();
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .TIMES(3)
        .LR_SIDE_EFFECT(*_4 = fixture.fences[fixture.created_fences++])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_fence(trompeloeil::_, trompeloeil::_, nullptr)).TIMES(3);
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_4 = fixture.semaphore)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_destroy_semaphore(trompeloeil::_, fixture.semaphore, nullptr));
    REQUIRE_CALL(
        fixture.api,
        vk_acquire_next_image_khr(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            nullptr,
            trompeloeil::_,
            trompeloeil::_))
        .SIDE_EFFECT(*_6 = 0)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .RETURN(VK_ERROR_DEVICE_LOST);

    // No recording or queue operation starts after the failed acquisition-completion wait.
    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        fixture.operation().build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::wait_for_fence);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_DEVICE_LOST);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
