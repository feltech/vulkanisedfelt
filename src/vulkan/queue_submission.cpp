// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./queue_submission.hpp"

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
#include <immer/array.hpp>
#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../text/string.hpp"
#include "./clear_commands.hpp"
#include "./clear_commands_types.hpp"
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
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
#include "./queue_submission_types.hpp"
#include "./render_pass.hpp"
#include "./renderer_errors.hpp"
#include "./synchronization_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct QueueSubmissionEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] queue submission plans are lazy and retain all submitted resources")
{
    backend::MockApi api;
    QueueSubmissionEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, pass_storage{}, target_storage{}, buffer_storage{};
    char wait_storage{}, signal_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    RecordedClearCommands recording{
        {reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}},
        {reinterpret_cast<VkFramebuffer>(&target_storage), [](auto) {}},
        {reinterpret_cast<VkCommandBuffer>(&buffer_storage), [](auto) {}}};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    semaphore_ptr signal_semaphore{reinterpret_cast<VkSemaphore>(&signal_storage), [](auto) {}};
    fence_ptr completion_fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr buffer_observer{recording.command_buffer};
    std::weak_ptr target_observer{recording.framebuffer};
    std::weak_ptr pass_observer{recording.render_pass};
    std::weak_ptr wait_observer{wait_semaphore};
    std::weak_ptr signal_observer{signal_semaphore};
    std::weak_ptr fence_observer{completion_fence};

    {
        // Building capabilities retains inputs without submitting commands or scheduling signals.
        auto plan = reader_io_vk_queue_submit<QueueSubmissionEnvironment>(
            std::move(device),
            reinterpret_cast<VkQueue>(&queue_storage),
            std::move(recording),
            std::move(wait_semaphore),
            std::move(signal_semaphore),
            std::move(completion_fence));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(buffer_observer.expired());
        CHECK_FALSE(target_observer.expired());
        CHECK_FALSE(pass_observer.expired());
        CHECK_FALSE(wait_observer.expired());
        CHECK_FALSE(signal_observer.expired());
        CHECK_FALSE(fence_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(buffer_observer.expired());
    CHECK(target_observer.expired());
    CHECK(pass_observer.expired());
    CHECK(wait_observer.expired());
    CHECK(signal_observer.expired());
    CHECK(fence_observer.expired());
}

TEST_CASE("[impure] queue submissions retain their recording across a timed out completion wait")
{
    backend::MockApi api;
    QueueSubmissionEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, pass_storage{}, target_storage{}, buffer_storage{};
    char wait_storage{}, signal_storage{}, fence_storage{};
    int cleanup_step = 0;
    device_ptr device{
        reinterpret_cast<VkDevice>(&device_storage), [&](auto) { CHECK_EQ(++cleanup_step, 4); }};
    RecordedClearCommands recording{
        {reinterpret_cast<VkRenderPass>(&pass_storage), [&](auto) { CHECK_EQ(++cleanup_step, 3); }},
        {reinterpret_cast<VkFramebuffer>(&target_storage),
         [&](auto) { CHECK_EQ(++cleanup_step, 2); }},
        {reinterpret_cast<VkCommandBuffer>(&buffer_storage),
         [&](auto) { CHECK_EQ(++cleanup_step, 1); }}};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    semaphore_ptr signal_semaphore{reinterpret_cast<VkSemaphore>(&signal_storage), [](auto) {}};
    fence_ptr completion_fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    bool has_wait = true;
    bool has_signal = true;
    SUBCASE("both binary semaphore operations") {}
    SUBCASE("no wait operation")
    {
        wait_semaphore.reset();
        has_wait = false;
    }
    SUBCASE("no signal operation")
    {
        signal_semaphore.reset();
        has_signal = false;
    }
    SUBCASE("no semaphore operations")
    {
        wait_semaphore.reset();
        signal_semaphore.reset();
        has_wait = false;
        has_signal = false;
    }
    std::weak_ptr device_observer{device};
    std::weak_ptr buffer_observer{recording.command_buffer};
    VkQueue queue = reinterpret_cast<VkQueue>(&queue_storage);
    VkCommandBuffer buffer_handle = recording.command_buffer.get();
    VkSemaphore wait_handle = wait_semaphore.get();
    VkSemaphore signal_handle = signal_semaphore.get();
    VkFence fence_handle = completion_fence.get();
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, vk_queue_submit(queue, 1, trompeloeil::_, fence_handle))
        .IN_SEQUENCE(calls)
        .LR_SIDE_EFFECT({
            // Submit descriptions are borrowed only for the synchronous native call.
            CHECK_EQ(_3->sType, VK_STRUCTURE_TYPE_SUBMIT_INFO);
            CHECK(_3->pNext == nullptr);
            CHECK_EQ(_3->commandBufferCount, 1U);
            CHECK_EQ(_3->pCommandBuffers[0], buffer_handle);
            CHECK_EQ(_3->waitSemaphoreCount, has_wait ? 1U : 0U);
            if (has_wait)
            {
                CHECK_EQ(_3->pWaitSemaphores[0], wait_handle);
                CHECK_EQ(_3->pWaitDstStageMask[0], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
            else
            {
                CHECK(_3->pWaitSemaphores == nullptr);
                CHECK(_3->pWaitDstStageMask == nullptr);
            }
            CHECK_EQ(_3->signalSemaphoreCount, has_signal ? 1U : 0U);
            if (has_signal)
            {
                CHECK_EQ(_3->pSignalSemaphores[0], signal_handle);
            }
            else
            {
                CHECK(_3->pSignalSemaphores == nullptr);
            }
        })
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_wait_for_fences(device.get(), 1, trompeloeil::_, VK_TRUE, 0))
        .IN_SEQUENCE(calls)
        .WITH(_3[0] == fence_handle)
        .RETURN(VK_TIMEOUT);
    REQUIRE_CALL(api, vk_wait_for_fences(device.get(), 1, trompeloeil::_, VK_TRUE, 99))
        .IN_SEQUENCE(calls)
        .WITH(_3[0] == fence_handle)
        .RETURN(VK_SUCCESS);

    {
        // Accepted work publishes keepalives even when the first completion poll times out.
        submitted_clear_commands_result outcome =
            reader_io_vk_queue_submit<QueueSubmissionEnvironment>(
                std::move(device),
                queue,
                std::move(recording),
                std::move(wait_semaphore),
                std::move(signal_semaphore),
                std::move(completion_fence))
                .build(environment)
                .sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(outcome));
        SubmittedClearCommands submission = fnfelt::result::get_success(std::move(outcome));
        vulkan_result<FenceWaitStatus> first_wait =
            reader_io_vk_wait_for_fence<QueueSubmissionEnvironment>(
                submission.device, submission.completion_fence, timeout_nanoseconds{0})
                .build(environment)
                .sync_wait(lf::lazy_pool{2});

        REQUIRE(fnfelt::result::is_success(first_wait));
        CHECK_EQ(fnfelt::result::get_success(first_wait), FenceWaitStatus::timed_out);
        CHECK_EQ(cleanup_step, 0);
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(buffer_observer.expired());
        CHECK_EQ(submission.wait_semaphore.get(), wait_handle);
        CHECK_EQ(submission.signal_semaphore.get(), signal_handle);
        vulkan_result<FenceWaitStatus> completed =
            reader_io_vk_wait_for_fence<QueueSubmissionEnvironment>(
                submission.device, submission.completion_fence, timeout_nanoseconds{99})
                .build(environment)
                .sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(completed));
        REQUIRE_EQ(fnfelt::result::get_success(completed), FenceWaitStatus::signaled);

        // The caller releases recording resources only after completion has been established.
    }

    CHECK_EQ(cleanup_step, 4);
    CHECK(device_observer.expired());
    CHECK(buffer_observer.expired());
}

TEST_CASE("[impure] queue submission failures preserve status while callers retain inputs")
{
    backend::MockApi api;
    QueueSubmissionEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, pass_storage{}, target_storage{}, buffer_storage{};
    char wait_storage{}, signal_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    RecordedClearCommands recording{
        {reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}},
        {reinterpret_cast<VkFramebuffer>(&target_storage), [](auto) {}},
        {reinterpret_cast<VkCommandBuffer>(&buffer_storage), [](auto) {}}};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    semaphore_ptr signal_semaphore{reinterpret_cast<VkSemaphore>(&signal_storage), [](auto) {}};
    fence_ptr completion_fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    std::weak_ptr buffer_observer{recording.command_buffer};
    std::weak_ptr fence_observer{completion_fence};
    VkResult status = VK_ERROR_OUT_OF_HOST_MEMORY;
    SUBCASE("host allocation failure") {}
    SUBCASE("device allocation failure")
    {
        status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("device loss does not establish completion")
    {
        status = VK_ERROR_DEVICE_LOST;
    }
    REQUIRE_CALL(
        api,
        vk_queue_submit(
            reinterpret_cast<VkQueue>(&queue_storage), 1, trompeloeil::_, completion_fence.get()))
        .LR_RETURN(status);

    // Caller-owned inputs outlive an error; device loss is not proof that resource use is complete.
    submitted_clear_commands_result outcome = reader_io_vk_queue_submit<QueueSubmissionEnvironment>(
                                                  device,
                                                  reinterpret_cast<VkQueue>(&queue_storage),
                                                  recording,
                                                  wait_semaphore,
                                                  signal_semaphore,
                                                  completion_fence)
                                                  .build(environment)
                                                  .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::queue_submit);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    CHECK_FALSE(buffer_observer.expired());
    CHECK_FALSE(fence_observer.expired());
}

TEST_CASE("[integration][windowed] recorded clear work completes through a submission fence")
{
    backend::Api api;
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    sdl::detail::TestRuntime runtime{api};
    REQUIRE(
        fnfelt::result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<state_type>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl::sdl_result<SDL_Window *> window =
        runtime.run(sdl::state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    renderer_result<PresentationInstance> instance = runtime.run(
        reader_io_create_presentation_instance<environment_type>(runtime.resources().window_owner));
    if (fnfelt::result::is_error(instance))
    {
        CHECK(
            std::holds_alternative<PresentationInstanceFailure>(
                fnfelt::result::get_error(instance)));
        return;
    }
    REQUIRE(fnfelt::result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance).instance));
    REQUIRE(fnfelt::result::is_success(surface));
    presentation_device_result device = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            fnfelt::result::get_success(instance), fnfelt::result::get_success(surface)));
    if (fnfelt::result::is_error(device))
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(device)));
        return;
    }
    REQUIRE(fnfelt::result::is_success(device));
    presentation_swapchain_images_result images = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            fnfelt::result::get_success(device), runtime.resources().window_owner));
    REQUIRE(fnfelt::result::is_success(images));
    presentation_image_views_result views = runtime.run(
        reader_io_create_presentation_image_views<environment_type>(
            fnfelt::result::get_success(std::move(images))));
    REQUIRE(fnfelt::result::is_success(views));
    PresentationSwapchain presentation_swapchain =
        fnfelt::result::get_success(views).presentation_swapchain_images.presentation_swapchain;
    VkExtent2D extent = presentation_swapchain.configuration.extent;
    PresentationDevice presentation_device = fnfelt::result::get_success(device);
    vulkan_result<render_pass_ptr> render_pass = runtime.run(
        reader_io_vk_create_render_pass<environment_type>(
            presentation_device.device,
            presentation_swapchain.configuration.surface_format.format));
    REQUIRE(fnfelt::result::is_success(render_pass));
    presentation_framebuffers_result framebuffers = runtime.run(
        reader_io_create_presentation_framebuffers<environment_type>(
            fnfelt::result::get_success(std::move(views)),
            fnfelt::result::get_success(render_pass)));
    REQUIRE(fnfelt::result::is_success(framebuffers));
    vulkan_result<command_pool_ptr> pool = runtime.run(
        reader_io_vk_create_command_pool<environment_type>(
            presentation_device.device, presentation_device.queue_families.graphics_family_index));
    REQUIRE(fnfelt::result::is_success(pool));
    vulkan_result<fence_ptr> acquisition_fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(presentation_device.device));
    REQUIRE(fnfelt::result::is_success(acquisition_fence));
    vulkan_result<fence_ptr> completion_fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(presentation_device.device));
    REQUIRE(fnfelt::result::is_success(completion_fence));

    // Host-completed acquisition establishes image availability without a submit semaphore wait.
    vulkan_result<AcquiredSwapchainImage> acquired = runtime.run(
        reader_io_vk_acquire_next_image_khr<environment_type>(
            presentation_device.device,
            presentation_swapchain.swapchain,
            timeout_nanoseconds{1'000'000'000},
            {},
            fnfelt::result::get_success(acquisition_fence)));
    REQUIRE(fnfelt::result::is_success(acquired));
    vulkan_result<FenceWaitStatus> acquisition_completed = runtime.run(
        reader_io_vk_wait_for_fence<environment_type>(
            presentation_device.device,
            fnfelt::result::get_success(acquired).fence,
            timeout_nanoseconds{std::numeric_limits<std::uint64_t>::max()}));
    REQUIRE(fnfelt::result::is_success(acquisition_completed));
    REQUIRE_EQ(fnfelt::result::get_success(acquisition_completed), FenceWaitStatus::signaled);
    swapchain_image_index image_index = fnfelt::result::get_success(acquired).image_index;
    recorded_clear_commands_result recording = runtime.run(
        reader_io_record_clear_commands<environment_type>(
            presentation_device.device,
            fnfelt::result::get_success(pool),
            fnfelt::result::get_success(framebuffers).framebuffers[value_of(image_index)],
            fnfelt::result::get_success(render_pass),
            extent,
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}}));
    REQUIRE(fnfelt::result::is_success(recording));

    // Retain every input across submission errors as well as through successful completion.
    submitted_clear_commands_result submitted = runtime.run(
        reader_io_vk_queue_submit<environment_type>(
            presentation_device.device,
            presentation_device.graphics_queue,
            fnfelt::result::get_success(recording),
            {},
            {},
            fnfelt::result::get_success(completion_fence)));
    REQUIRE(fnfelt::result::is_success(submitted));
    SubmittedClearCommands const & submission = fnfelt::result::get_success(submitted);
    vulkan_result<FenceWaitStatus> completion = runtime.run(
        reader_io_vk_wait_for_fence<environment_type>(
            submission.device,
            submission.completion_fence,
            timeout_nanoseconds{std::numeric_limits<std::uint64_t>::max()}));

    REQUIRE(fnfelt::result::is_success(completion));
    CHECK_EQ(fnfelt::result::get_success(completion), FenceWaitStatus::signaled);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
