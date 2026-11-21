// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./queue_presentation.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <variant>

#include <SDL.h>
#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "./clear_commands.hpp"
#include "./clear_commands_types.hpp"
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./fence.hpp"
#include "./fence_types.hpp"
#include "./image_acquisition.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_framebuffers.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_image_views_types.hpp"
#include "./presentation_instance.hpp"
#include "./presentation_instance_types.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./presentation_swapchain_types.hpp"
#include "./queue_family_selection_types.hpp"
#include "./queue_presentation_types.hpp"
#include "./queue_submission.hpp"
#include "./queue_submission_types.hpp"
#include "./queue_types.hpp"
#include "./render_pass.hpp"
#include "./renderer_errors.hpp"
#include "./semaphore.hpp"
#include "./synchronization_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct QueuePresentationEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};

struct QueuePresentationFixture
{
    backend::MockApi api;
    char device_storage{}, queue_storage{}, surface_storage{}, swapchain_storage{};
    char acquisition_semaphore_storage{}, acquisition_fence_storage{}, wait_storage{},
        fence_storage{};
    PresentationDevice context{
        {reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}},
        {reinterpret_cast<VkDevice>(&device_storage), [](auto) {}},
        {},
        PresentationQueueFamilies{queue_family_index{0}, queue_family_index{0}},
        reinterpret_cast<VkQueue>(&queue_storage),
        reinterpret_cast<VkQueue>(&queue_storage)};
    AcquiredSwapchainImage acquired_image{
        {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}},
        {reinterpret_cast<VkSemaphore>(&acquisition_semaphore_storage), [](auto) {}},
        {reinterpret_cast<VkFence>(&acquisition_fence_storage), [](auto) {}},
        swapchain_image_index{2},
        true};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    fence_ptr presentation_fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
};
}  // namespace

TEST_CASE("[impure] fenced presentation plans retain inputs without native effects")
{
    QueuePresentationFixture fixture;
    QueuePresentationEnvironment environment{std::ref(fixture.api)};
    std::weak_ptr device_observer{fixture.context.device};
    std::weak_ptr swapchain_observer{fixture.acquired_image.swapchain};
    std::weak_ptr wait_observer{fixture.wait_semaphore};
    std::weak_ptr fence_observer{fixture.presentation_fence};

    {
        // Materializing the IO retains inputs after its temporary ReaderIO has gone away.
        auto operation = reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
                             std::move(fixture.context.device),
                             fixture.context.present_queue,
                             std::move(fixture.acquired_image),
                             std::move(fixture.wait_semaphore),
                             std::move(fixture.presentation_fence))
                             .build(environment)
                             .io();
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
        CHECK_FALSE(wait_observer.expired());
        CHECK_FALSE(fence_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(wait_observer.expired());
    CHECK(fence_observer.expired());
}

TEST_CASE("[impure] fenced presentation retains request owners through explicit completion waits")
{
    QueuePresentationFixture fixture;
    QueuePresentationEnvironment environment{std::ref(fixture.api)};
    VkResult status = VK_SUCCESS;
    SUBCASE("successful request with a binary wait") {}
    SUBCASE("suboptimal request with a binary wait")
    {
        status = VK_SUBOPTIMAL_KHR;
    }
    SUBCASE("successful request without a binary wait")
    {
        fixture.wait_semaphore.reset();
    }
    SUBCASE("suboptimal request without a binary wait")
    {
        status = VK_SUBOPTIMAL_KHR;
        fixture.wait_semaphore.reset();
    }
    VkDevice device_handle = fixture.context.device.get();
    VkQueue queue = fixture.context.present_queue;
    VkSwapchainKHR swapchain = fixture.acquired_image.swapchain.get();
    VkSemaphore wait_handle = fixture.wait_semaphore.get();
    VkFence fence_handle = fixture.presentation_fence.get();
    std::weak_ptr device_observer{fixture.context.device};
    std::weak_ptr swapchain_observer{fixture.acquired_image.swapchain};
    std::weak_ptr acquisition_semaphore_observer{fixture.acquired_image.semaphore};
    std::weak_ptr acquisition_fence_observer{fixture.acquired_image.fence};
    std::weak_ptr wait_observer{fixture.wait_semaphore};
    std::weak_ptr fence_observer{fixture.presentation_fence};
    trompeloeil::sequence calls;
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(queue, trompeloeil::_))
        .IN_SEQUENCE(calls)
        .LR_SIDE_EFFECT({
            // The call-local chain associates exactly this image's unsignaled release fence.
            CHECK_EQ(_2->sType, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
            REQUIRE(_2->pNext != nullptr);
            auto const * fence_info =
                static_cast<VkSwapchainPresentFenceInfoEXT const *>(_2->pNext);
            CHECK_EQ(fence_info->sType, VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT);
            CHECK(fence_info->pNext == nullptr);
            CHECK_EQ(fence_info->swapchainCount, 1U);
            REQUIRE(fence_info->pFences != nullptr);
            CHECK_EQ(fence_info->pFences[0], fence_handle);
            CHECK_EQ(_2->swapchainCount, 1U);
            CHECK_EQ(_2->pSwapchains[0], swapchain);
            CHECK_EQ(_2->pImageIndices[0], 2U);
            CHECK(_2->pResults == nullptr);
            CHECK_EQ(_2->waitSemaphoreCount, wait_handle ? 1U : 0U);
            if (wait_handle)
            {
                CHECK_EQ(_2->pWaitSemaphores[0], wait_handle);
            }
            else
            {
                CHECK(_2->pWaitSemaphores == nullptr);
            }
            CHECK_FALSE(device_observer.expired());
            CHECK_FALSE(swapchain_observer.expired());
            CHECK_FALSE(fence_observer.expired());
        })
        .LR_RETURN(status);
    REQUIRE_CALL(fixture.api, vk_wait_for_fences(device_handle, 1, trompeloeil::_, VK_TRUE, 0))
        .IN_SEQUENCE(calls)
        .WITH(_3[0] == fence_handle)
        .RETURN(VK_TIMEOUT);
    REQUIRE_CALL(fixture.api, vk_wait_for_fences(device_handle, 1, trompeloeil::_, VK_TRUE, 99))
        .IN_SEQUENCE(calls)
        .WITH(_3[0] == fence_handle)
        .RETURN(VK_SUCCESS);

    {
        QueuedPresentation presentation;
        {
            // Consume a materialized IO; the returned request then owns every native keepalive.
            auto operation = reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
                                 std::move(fixture.context.device),
                                 fixture.context.present_queue,
                                 std::move(fixture.acquired_image),
                                 std::move(fixture.wait_semaphore),
                                 std::move(fixture.presentation_fence))
                                 .build(environment)
                                 .io();
            queued_presentation_result outcome = operation.build().sync_wait(lf::lazy_pool{2});
            REQUIRE(fnfelt::result::is_success(outcome));
            presentation = fnfelt::result::get_success(std::move(outcome));
        }
        CHECK_EQ(presentation.device.get(), device_handle);
        CHECK_EQ(presentation.acquired_image.swapchain.get(), swapchain);
        CHECK(presentation.acquired_image.is_suboptimal);
        CHECK_EQ(presentation.is_suboptimal, status == VK_SUBOPTIMAL_KHR);
        CHECK_EQ(presentation.wait_semaphore.get(), wait_handle);
        CHECK_EQ(presentation.presentation_fence.get(), fence_handle);

        // Timeout leaves ownership intact; only the later signaled wait permits resource release.
        vulkan_result<FenceWaitStatus> first_wait =
            reader_io_vk_wait_for_fence<QueuePresentationEnvironment>(
                presentation.device, presentation.presentation_fence, timeout_nanoseconds{0})
                .build(environment)
                .sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(first_wait));
        CHECK_EQ(fnfelt::result::get_success(first_wait), FenceWaitStatus::timed_out);
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
        CHECK_FALSE(acquisition_semaphore_observer.expired());
        CHECK_FALSE(acquisition_fence_observer.expired());
        CHECK_EQ(wait_observer.expired(), wait_handle == nullptr);
        CHECK_FALSE(fence_observer.expired());
        vulkan_result<FenceWaitStatus> second_wait =
            reader_io_vk_wait_for_fence<QueuePresentationEnvironment>(
                presentation.device, presentation.presentation_fence, timeout_nanoseconds{99})
                .build(environment)
                .sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(second_wait));
        CHECK_EQ(fnfelt::result::get_success(second_wait), FenceWaitStatus::signaled);
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(acquisition_semaphore_observer.expired());
    CHECK(acquisition_fence_observer.expired());
    CHECK(wait_observer.expired());
    CHECK(fence_observer.expired());
}

TEST_CASE("[impure] fenced presentation errors preserve status while callers retain every input")
{
    QueuePresentationFixture fixture;
    QueuePresentationEnvironment environment{std::ref(fixture.api)};
    VkResult status = VK_ERROR_OUT_OF_DATE_KHR;
    SUBCASE("out of date still enqueues the fenced request") {}
    SUBCASE("surface loss still enqueues the fenced request")
    {
        status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("exclusive mode loss still enqueues the fenced request")
    {
        status = VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT;
    }
    SUBCASE("host allocation failure does not associate the fence")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("device allocation failure does not associate the fence")
    {
        status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("device loss requires its own cleanup policy")
    {
        status = VK_ERROR_DEVICE_LOST;
    }
    VkQueue queue = fixture.context.present_queue;
    VkFence fence_handle = fixture.presentation_fence.get();
    REQUIRE_CALL(fixture.api, vk_queue_present_khr(queue, trompeloeil::_))
        .WITH(
            static_cast<VkSwapchainPresentFenceInfoEXT const *>(_2->pNext)->pFences[0] ==
            fence_handle)
        .LR_RETURN(status);

    // Rejections may still associate the fence; errors never consume the caller's originals.
    queued_presentation_result outcome =
        reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
            fixture.context.device,
            fixture.context.present_queue,
            fixture.acquired_image,
            fixture.wait_semaphore,
            fixture.presentation_fence)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::queue_present);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    CHECK_EQ(fixture.context.device.use_count(), 1);
    CHECK_EQ(fixture.acquired_image.swapchain.use_count(), 1);
    CHECK_EQ(fixture.acquired_image.semaphore.use_count(), 1);
    CHECK_EQ(fixture.acquired_image.fence.use_count(), 1);
    CHECK_EQ(fixture.wait_semaphore.use_count(), 1);
    CHECK_EQ(fixture.presentation_fence.use_count(), 1);
}

TEST_CASE("[integration][windowed] one clear frame releases presentation resources through a fence")
{
    backend::Api api;
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    namespace result = fnfelt::result;
    sdl::detail::TestRuntime runtime{api};
    REQUIRE(result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<state_type>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl::sdl_result<SDL_Window *> window =
        runtime.run(sdl::state_io_sdl_create_window<state_type>(options));
    REQUIRE(result::is_success(window));
    renderer_result<PresentationInstance> instance = runtime.run(
        reader_io_create_presentation_instance<environment_type>(runtime.resources().window_owner));
    if (result::is_error(instance))
    {
        CHECK(std::holds_alternative<PresentationInstanceFailure>(result::get_error(instance)));
        return;
    }
    REQUIRE(result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, result::get_success(instance).instance));
    REQUIRE(result::is_success(surface));
    presentation_device_result device = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            result::get_success(instance), result::get_success(surface)));
    if (result::is_error(device))
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(result::get_error(device)));
        return;
    }
    REQUIRE(result::is_success(device));
    PresentationDevice context = result::get_success(device);
    presentation_swapchain_images_result images = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            context, runtime.resources().window_owner));
    REQUIRE(result::is_success(images));
    presentation_image_views_result views = runtime.run(
        reader_io_create_presentation_image_views<environment_type>(
            result::get_success(std::move(images))));
    REQUIRE(result::is_success(views));
    PresentationSwapchain presentation_swapchain =
        result::get_success(views).presentation_swapchain_images.presentation_swapchain;
    VkExtent2D extent = presentation_swapchain.configuration.extent;
    vulkan_result<render_pass_ptr> render_pass = runtime.run(
        reader_io_vk_create_render_pass<environment_type>(
            context.device, presentation_swapchain.configuration.surface_format.format));
    REQUIRE(result::is_success(render_pass));
    presentation_framebuffers_result framebuffers = runtime.run(
        reader_io_create_presentation_framebuffers<environment_type>(
            result::get_success(std::move(views)), result::get_success(render_pass)));
    REQUIRE(result::is_success(framebuffers));
    vulkan_result<command_pool_ptr> pool = runtime.run(
        reader_io_vk_create_command_pool<environment_type>(
            context.device, context.queue_families.graphics_family_index));
    REQUIRE(result::is_success(pool));
    vulkan_result<fence_ptr> acquisition_fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(context.device));
    REQUIRE(result::is_success(acquisition_fence));
    vulkan_result<fence_ptr> submission_fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(context.device));
    REQUIRE(result::is_success(submission_fence));
    vulkan_result<fence_ptr> presentation_fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(context.device));
    REQUIRE(result::is_success(presentation_fence));
    vulkan_result<semaphore_ptr> rendering_semaphore =
        runtime.run(reader_io_vk_create_semaphore<environment_type>(context.device));
    REQUIRE(result::is_success(rendering_semaphore));
    constexpr timeout_nanoseconds indefinitely{std::numeric_limits<std::uint64_t>::max()};

    // Originals remain outside noexcept phases: an unexpected failure cannot unwind pending owners.
    auto wait_for_release = [&](fence_ptr fence) noexcept -> vulkan_result<FenceWaitStatus>
    {
        vulkan_result<FenceWaitStatus> completion = runtime.run(
            reader_io_vk_wait_for_fence<environment_type>(
                context.device, std::move(fence), indefinitely));
        if (result::is_error(completion))
        {
            // A finite device-loss wait permits teardown, without claiming successful work.
            REQUIRE_EQ(result::get_error(completion).result, VK_ERROR_DEVICE_LOST);
        }
        else
        {
            REQUIRE(result::is_success(completion));
            REQUIRE_EQ(result::get_success(completion), FenceWaitStatus::signaled);
        }
        return completion;
    };

    // Failed acquisition leaves its signals unaffected; successful acquisition is host-completed.
    vulkan_result<AcquiredSwapchainImage> acquired = runtime.run(
        reader_io_vk_acquire_next_image_khr<environment_type>(
            context.device,
            presentation_swapchain.swapchain,
            timeout_nanoseconds{1'000'000'000},
            {},
            result::get_success(acquisition_fence)));
    REQUIRE(result::is_success(acquired));
    vulkan_result<FenceWaitStatus> acquisition_completed =
        wait_for_release(result::get_success(acquisition_fence));
    REQUIRE(result::is_success(acquisition_completed));
    swapchain_image_index image_index = result::get_success(acquired).image_index;
    recorded_clear_commands_result recording = runtime.run(
        reader_io_record_clear_commands<environment_type>(
            context.device,
            result::get_success(pool),
            result::get_success(framebuffers).framebuffers[value_of(image_index)],
            result::get_success(render_pass),
            extent,
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}}));
    REQUIRE(result::is_success(recording));

    queued_presentation_result presented = [&]() noexcept
    {
        submitted_clear_commands_result submitted = runtime.run(
            reader_io_vk_queue_submit<environment_type>(
                context.device,
                context.graphics_queue,
                result::get_success(recording),
                {},
                result::get_success(rendering_semaphore),
                result::get_success(submission_fence)));
        if (result::is_error(submitted))
        {
            VkResult status = result::get_error(submitted).result;
            REQUIRE(
                (status == VK_ERROR_OUT_OF_HOST_MEMORY || status == VK_ERROR_OUT_OF_DEVICE_MEMORY ||
                 status == VK_ERROR_DEVICE_LOST));
            if (status == VK_ERROR_DEVICE_LOST)
            {
                // Device loss can still leave submission resources in use until this wait returns.
                wait_for_release(result::get_success(submission_fence));
            }
            return result::error<queued_presentation_result>(result::get_error(submitted));
        }
        REQUIRE(result::is_success(submitted));

        // Complete rendering first, so even an unqueued presentation leaves no pending submission.
        vulkan_result<FenceWaitStatus> rendering_completed =
            wait_for_release(result::get_success(submission_fence));
        if (result::is_error(rendering_completed))
        {
            return result::error<queued_presentation_result>(
                result::get_error(rendering_completed));
        }
        queued_presentation_result outcome = runtime.run(
            reader_io_vk_queue_present_khr<environment_type>(
                context.device,
                context.present_queue,
                result::get_success(acquired),
                result::get_success(rendering_semaphore),
                result::get_success(presentation_fence)));
        VkResult status = VK_SUCCESS;
        if (result::is_error(outcome))
        {
            status = result::get_error(outcome).result;
        }
        else
        {
            REQUIRE(result::is_success(outcome));
        }
        bool const has_unchanged_state =
            status == VK_ERROR_OUT_OF_HOST_MEMORY || status == VK_ERROR_OUT_OF_DEVICE_MEMORY;
        // These rejections still enqueue presentation; OOM leaves the fence unassociated.
        REQUIRE(
            (status == VK_SUCCESS || status == VK_ERROR_OUT_OF_DATE_KHR ||
             status == VK_ERROR_SURFACE_LOST_KHR ||
             status == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT ||
             status == VK_ERROR_DEVICE_LOST || has_unchanged_state));
        if (!has_unchanged_state)
        {
            vulkan_result<FenceWaitStatus> release =
                wait_for_release(result::get_success(presentation_fence));
            if (result::is_error(release) && result::is_success(outcome))
            {
                return result::error<queued_presentation_result>(result::get_error(release));
            }
        }
        return outcome;
    }();

    // Native failures are reported only after all potentially queued uses permit resource release.
    CHECK(result::is_success(presented));
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
