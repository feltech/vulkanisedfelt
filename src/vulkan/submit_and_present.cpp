// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./submit_and_present.hpp"

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
#include "./fence_types.hpp"
#include "./queue_family_selection_types.hpp"
#include "./queue_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SubmitAndPresentEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};

struct SubmitAndPresentFixture
{
    backend::MockApi api;
    char device_storage{}, queue_storage{}, surface_storage{}, swapchain_storage{};
    char buffer_storage{}, framebuffer_storage{}, render_pass_storage{}, semaphore_storage{};
    char acquisition_storage{}, submission_storage{}, presentation_storage{};
    PresentationDevice context{
        {reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}},
        {reinterpret_cast<VkDevice>(&device_storage), [](auto) {}},
        {},
        PresentationQueueFamilies{queue_family_index{0}, queue_family_index{0}},
        reinterpret_cast<VkQueue>(&queue_storage),
        reinterpret_cast<VkQueue>(&queue_storage)};
    RecordedClearCommands recording{
        {reinterpret_cast<VkRenderPass>(&render_pass_storage), [](auto) {}},
        {reinterpret_cast<VkFramebuffer>(&framebuffer_storage), [](auto) {}},
        {reinterpret_cast<VkCommandBuffer>(&buffer_storage), [](auto) {}}};
    AcquiredSwapchainImage acquired_image{
        {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}},
        {},
        {reinterpret_cast<VkFence>(&acquisition_storage), [](auto) {}},
        swapchain_image_index{2},
        false};
    semaphore_ptr rendering_semaphore{
        reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}};
    fence_ptr submission_fence{reinterpret_cast<VkFence>(&submission_storage), [](auto) {}};
    fence_ptr presentation_fence{reinterpret_cast<VkFence>(&presentation_storage), [](auto) {}};

    auto operation()
    {
        return reader_io_submit_and_present<SubmitAndPresentEnvironment>(
            context,
            recording,
            acquired_image,
            rendering_semaphore,
            submission_fence,
            presentation_fence);
    }
};
}  // namespace

TEST_CASE("[impure] bounded presentation waits for rendering and release in order")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    VkResult presentation_status = VK_SUCCESS;
    SUBCASE("optimal frame") {}
    SUBCASE("suboptimal presentation")
    {
        presentation_status = VK_SUBOPTIMAL_KHR;
    }
    SUBCASE("suboptimal acquisition")
    {
        fixture.acquired_image.is_suboptimal = true;
    }
    trompeloeil::sequence execution;
    REQUIRE_CALL(
        fixture.api,
        vk_queue_submit(
            fixture.context.graphics_queue, 1, trompeloeil::_, fixture.submission_fence.get()))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api,
        vk_wait_for_fences(
            fixture.context.device.get(), 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.submission_fence.get())
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(fixture.context.present_queue, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(presentation_status);
    REQUIRE_CALL(
        fixture.api,
        vk_wait_for_fences(
            fixture.context.device.get(), 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.presentation_fence.get())
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);

    // Construction is lazy; execution returns only after every queued use permits release.
    auto operation = fixture.operation();
    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        std::move(operation).build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(fnfelt::result::get_success(outcome).image_index, swapchain_image_index{2});
    CHECK_EQ(
        fnfelt::result::get_success(outcome).is_suboptimal,
        fixture.acquired_image.is_suboptimal || presentation_status == VK_SUBOPTIMAL_KHR);
}

TEST_CASE("[impure] bounded presentation waits for safe release before returning an enqueued error")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    VkResult status = VK_ERROR_OUT_OF_DATE_KHR;
    SUBCASE("out of date") {}
    SUBCASE("surface lost")
    {
        status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("exclusive mode lost")
    {
        status = VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT;
    }
    SUBCASE("device lost")
    {
        status = VK_ERROR_DEVICE_LOST;
    }
    trompeloeil::sequence execution;
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(status);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.presentation_fence.get())
        .IN_SEQUENCE(execution)
        .RETURN(VK_ERROR_DEVICE_LOST);

    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        fixture.operation().build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::queue_present);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
}

TEST_CASE("[impure] bounded presentation skips unassociated fences after allocation failure")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    bool has_submission_succeeded = false;
    VkResult failure = VK_ERROR_OUT_OF_HOST_MEMORY;
    SUBCASE("submission host allocation") {}
    SUBCASE("submission device allocation")
    {
        failure = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("presentation host allocation")
    {
        has_submission_succeeded = true;
    }
    SUBCASE("presentation device allocation")
    {
        has_submission_succeeded = true;
        failure = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .RETURN(has_submission_succeeded ? VK_SUCCESS : failure);
    std::unique_ptr<trompeloeil::expectation> wait, present;
    if (has_submission_succeeded)
    {
        wait = NAMED_REQUIRE_CALL(
                   fixture.api,
                   vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
                   .LR_WITH(*_3 == fixture.submission_fence.get())
                   .RETURN(VK_SUCCESS);
        present =
            NAMED_REQUIRE_CALL(fixture.api, vk_queue_present_khr(trompeloeil::_, trompeloeil::_))
                .RETURN(failure);
    }

    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        fixture.operation().build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, failure);
}

TEST_CASE("[impure] bounded presentation reports device loss after the submission completion wait")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    VkResult submission_status = VK_ERROR_DEVICE_LOST;
    SUBCASE("device loss during submission") {}
    SUBCASE("device loss during completion")
    {
        submission_status = VK_SUCCESS;
    }
    trompeloeil::sequence execution;
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(execution)
        .RETURN(submission_status);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.submission_fence.get())
        .IN_SEQUENCE(execution)
        .RETURN(VK_ERROR_DEVICE_LOST);

    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        fixture.operation().build(environment).io().build().sync_wait(scheduler);

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_DEVICE_LOST);
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation,
        submission_status == VK_SUCCESS ? FallibleVulkanOperation::wait_for_fence
                                        : FallibleVulkanOperation::queue_submit);
}

TEST_CASE("[impure] unclassified queue failures invoke the terminal API effect")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    bool has_submission_completed = false;
    VulkanFailure failure{FallibleVulkanOperation::queue_submit, VK_ERROR_UNKNOWN};
    SUBCASE("unknown submission outcome") {}
    SUBCASE("unknown presentation outcome")
    {
        has_submission_completed = true;
        failure.operation = FallibleVulkanOperation::queue_present;
    }
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .RETURN(has_submission_completed ? VK_SUCCESS : VK_ERROR_UNKNOWN);
    std::unique_ptr<trompeloeil::expectation> completion_wait, presentation;
    if (has_submission_completed)
    {
        completion_wait =
            NAMED_REQUIRE_CALL(
                fixture.api,
                vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
                .RETURN(VK_SUCCESS);
        presentation =
            NAMED_REQUIRE_CALL(fixture.api, vk_queue_present_khr(trompeloeil::_, trompeloeil::_))
                .RETURN(VK_ERROR_UNKNOWN);
    }
    REQUIRE_CALL(fixture.api, report_fatal_error_effect(trompeloeil::_))
        .LR_WITH(_1.operation == failure.operation && _1.result == failure.result)
        .THROW(failure);

    // Fake handles permit a throwing mock to observe a terminal effect without queued GPU work.
    auto operation = fixture.operation();
    CHECK_THROWS_AS(
        (void)std::move(operation).build(environment).io().build().sync_wait(), VulkanFailure);
}

TEST_CASE("[impure] bounded presentation scope retains final owners through release")
{
    SubmitAndPresentFixture fixture;
    SubmitAndPresentEnvironment environment{std::ref(fixture.api)};
    VkResult presentation_status = VK_SUCCESS;
    SUBCASE("success") {}
    SUBCASE("enqueued error")
    {
        presentation_status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    bool has_presentation_completed = false;
    bool has_swapchain_been_destroyed = false;
    fixture.acquired_image.swapchain = {
        reinterpret_cast<VkSwapchainKHR>(&fixture.swapchain_storage),
        [&](auto)
        {
            CHECK(has_presentation_completed);
            has_swapchain_been_destroyed = true;
        }};
    std::weak_ptr swapchain_observer{fixture.acquired_image.swapchain};
    REQUIRE_CALL(fixture.api, vk_queue_submit(trompeloeil::_, 1, trompeloeil::_, trompeloeil::_))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.submission_fence.get())
        .LR_SIDE_EFFECT(CHECK_FALSE(swapchain_observer.expired()))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(trompeloeil::_, trompeloeil::_))
        .RETURN(presentation_status);
    REQUIRE_CALL(
        fixture.api, vk_wait_for_fences(trompeloeil::_, 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .LR_WITH(*_3 == fixture.presentation_fence.get())
        .LR_SIDE_EFFECT(CHECK_FALSE(swapchain_observer.expired()))
        .LR_SIDE_EFFECT(has_presentation_completed = true)
        .RETURN(VK_SUCCESS);

    // The inner StateIO scope receives the last image owner, independently of the native Results.
    auto operation = reader_io_submit_and_present<SubmitAndPresentEnvironment>(
        fixture.context,
        fixture.recording,
        std::move(fixture.acquired_image),
        fixture.rendering_semaphore,
        fixture.submission_fence,
        fixture.presentation_fence);
    lf::lazy_pool scheduler{1};
    completed_presentation_result outcome =
        std::move(operation).build(environment).io().build().sync_wait(scheduler);

    CHECK_EQ(fnfelt::result::is_success(outcome), presentation_status == VK_SUCCESS);
    CHECK(has_presentation_completed);
    CHECK(has_swapchain_been_destroyed);
    CHECK(swapchain_observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
