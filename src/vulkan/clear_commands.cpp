// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./clear_commands.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

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
#include "../sdl/instance_extensions.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../text/string.hpp"
#include "./clear_commands_types.hpp"
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./managed_handles.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device.hpp"
#include "./presentation_framebuffers.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./render_pass.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct ClearCommandsEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] clear command plans are lazy and retain recording inputs")
{
    backend::MockApi api;
    ClearCommandsEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{}, framebuffer_storage{}, pass_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    command_pool_ptr pool{reinterpret_cast<VkCommandPool>(&pool_storage), [](auto) {}};
    framebuffer_ptr framebuffer{reinterpret_cast<VkFramebuffer>(&framebuffer_storage), [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr pool_observer{pool};
    std::weak_ptr framebuffer_observer{framebuffer};
    std::weak_ptr pass_observer{render_pass};

    {
        // Planning retains all inputs without allocation or native recording commands.
        auto plan = reader_io_record_clear_commands<ClearCommandsEnvironment>(
            std::move(device),
            std::move(pool),
            std::move(framebuffer),
            std::move(render_pass),
            VkExtent2D{640, 480},
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}});
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(pool_observer.expired());
        CHECK_FALSE(framebuffer_observer.expired());
        CHECK_FALSE(pass_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(pool_observer.expired());
    CHECK(framebuffer_observer.expired());
    CHECK(pass_observer.expired());
}

TEST_CASE("[impure] clear recordings retain resources until their command buffer is freed")
{
    backend::MockApi api;
    ClearCommandsEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{}, framebuffer_storage{}, pass_storage{}, buffer_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    VkFramebuffer framebuffer_handle = reinterpret_cast<VkFramebuffer>(&framebuffer_storage);
    VkRenderPass pass_handle = reinterpret_cast<VkRenderPass>(&pass_storage);
    VkCommandBuffer buffer_handle = reinterpret_cast<VkCommandBuffer>(&buffer_storage);
    int cleanup_step = 0;
    VkRenderPassBeginInfo captured{};
    VkClearValue captured_clear{};
    trompeloeil::sequence commands;
    REQUIRE_CALL(api, vk_allocate_command_buffers(device_handle, trompeloeil::_, trompeloeil::_))
        .IN_SEQUENCE(commands)
        .WITH(
            _2->commandPool == pool_handle && _2->commandBufferCount == 1 &&
            _2->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
        .SIDE_EFFECT(*_3 = buffer_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_begin_command_buffer(buffer_handle, trompeloeil::_))
        .IN_SEQUENCE(commands)
        .WITH(_2->flags == VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api, vk_cmd_begin_render_pass(buffer_handle, trompeloeil::_, VK_SUBPASS_CONTENTS_INLINE))
        .IN_SEQUENCE(commands)
        .LR_SIDE_EFFECT(captured = *_2)
        .LR_SIDE_EFFECT(captured_clear = _2->pClearValues[0]);
    REQUIRE_CALL(api, vk_cmd_end_render_pass(buffer_handle)).IN_SEQUENCE(commands);
    REQUIRE_CALL(api, vk_end_command_buffer(buffer_handle))
        .IN_SEQUENCE(commands)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_free_command_buffers(device_handle, pool_handle, 1, trompeloeil::_))
        .WITH(*_4 == buffer_handle)
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));
    REQUIRE_CALL(api, vk_destroy_command_pool(device_handle, pool_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 2));
    REQUIRE_CALL(api, vk_destroy_render_pass(device_handle, pass_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 4));

    // The completed result outlives its plan and every original owner.
    recorded_clear_commands_result outcome = [&]
    {
        device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 5); }};
        command_pool_ptr pool = make_command_pool_ptr(pool_handle, device, api);
        render_pass_ptr render_pass = make_render_pass_ptr(pass_handle, device, api);
        framebuffer_ptr framebuffer{
            framebuffer_handle,
            [parent = render_pass, &cleanup_step](auto) mutable
            {
                CHECK_EQ(++cleanup_step, 3);
                parent.reset();
            }};
        return reader_io_record_clear_commands<ClearCommandsEnvironment>(
                   std::move(device),
                   std::move(pool),
                   std::move(framebuffer),
                   std::move(render_pass),
                   VkExtent2D{640, 480},
                   VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});
    }();

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.renderPass, pass_handle);
    CHECK_EQ(captured.framebuffer, framebuffer_handle);
    CHECK_EQ(captured.renderArea.offset.x, 0);
    CHECK_EQ(captured.renderArea.offset.y, 0);
    CHECK_EQ(captured.renderArea.extent.width, 640U);
    CHECK_EQ(captured.renderArea.extent.height, 480U);
    CHECK_EQ(captured.clearValueCount, 1U);
    CHECK_EQ(captured_clear.color.float32[0], doctest::Approx(0.1F));
    CHECK_EQ(captured_clear.color.float32[1], doctest::Approx(0.2F));
    CHECK_EQ(captured_clear.color.float32[2], doctest::Approx(0.3F));
    CHECK_EQ(captured_clear.color.float32[3], doctest::Approx(1.0F));
    CHECK_EQ(cleanup_step, 0);

    // Destroying the complete recording frees the buffer before its recorded resources.
    outcome = fnfelt::result::stopped<recorded_clear_commands_result>();

    CHECK_EQ(cleanup_step, 5);
}

TEST_CASE("[impure] clear recording failures bypass later commands and release acquisitions")
{
    backend::MockApi api;
    ClearCommandsEnvironment environment{std::ref(api)};
    char device_storage{}, pool_storage{}, framebuffer_storage{}, pass_storage{}, buffer_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    VkCommandBuffer buffer_handle = reinterpret_cast<VkCommandBuffer>(&buffer_storage);
    FallibleVulkanOperation failed_operation = FallibleVulkanOperation::allocate_command_buffer;
    SUBCASE("allocation fails before recording starts") {}
    SUBCASE("begin fails before render pass commands")
    {
        failed_operation = FallibleVulkanOperation::begin_command_buffer;
    }
    SUBCASE("end fails and publishes no executable recording")
    {
        failed_operation = FallibleVulkanOperation::end_command_buffer;
    }
    bool is_buffer_freed = false;
    bool is_framebuffer_released = false;
    device_ptr device{device_handle, [](auto) {}};
    command_pool_ptr pool{pool_handle, [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}};
    framebuffer_ptr framebuffer{
        reinterpret_cast<VkFramebuffer>(&framebuffer_storage),
        [&](auto)
        {
            // A created buffer is freed before its referenced resources can disappear.
            CHECK(
                (failed_operation == FallibleVulkanOperation::allocate_command_buffer ||
                 is_buffer_freed));
            is_framebuffer_released = true;
        }};
    std::weak_ptr device_observer{device};
    std::weak_ptr pool_observer{pool};
    std::weak_ptr pass_observer{render_pass};
    VkResult allocation_status =
        failed_operation == FallibleVulkanOperation::allocate_command_buffer
        ? VK_ERROR_OUT_OF_HOST_MEMORY
        : VK_SUCCESS;
    REQUIRE_CALL(api, vk_allocate_command_buffers(device_handle, trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(*_3 = buffer_handle)
        .RETURN(allocation_status);
    std::unique_ptr<trompeloeil::expectation> begin_expectation;
    std::unique_ptr<trompeloeil::expectation> pass_begin_expectation;
    std::unique_ptr<trompeloeil::expectation> pass_end_expectation;
    std::unique_ptr<trompeloeil::expectation> end_expectation;
    std::unique_ptr<trompeloeil::expectation> free_expectation;
    if (allocation_status == VK_SUCCESS)
    {
        VkResult begin_status = failed_operation == FallibleVulkanOperation::begin_command_buffer
            ? VK_ERROR_OUT_OF_HOST_MEMORY
            : VK_SUCCESS;
        begin_expectation =
            NAMED_REQUIRE_CALL(api, vk_begin_command_buffer(buffer_handle, trompeloeil::_))
                .RETURN(begin_status);
        free_expectation =
            NAMED_REQUIRE_CALL(
                api, vk_free_command_buffers(device_handle, pool_handle, 1, trompeloeil::_))
                .WITH(*_4 == buffer_handle)
                .LR_SIDE_EFFECT(is_buffer_freed = true);
        if (begin_status == VK_SUCCESS)
        {
            pass_begin_expectation = NAMED_REQUIRE_CALL(
                api,
                vk_cmd_begin_render_pass(
                    buffer_handle, trompeloeil::_, VK_SUBPASS_CONTENTS_INLINE));
            pass_end_expectation = NAMED_REQUIRE_CALL(api, vk_cmd_end_render_pass(buffer_handle));
            end_expectation = NAMED_REQUIRE_CALL(api, vk_end_command_buffer(buffer_handle))
                                  .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
        }
    }

    // Every fallible native boundary short-circuits the remaining recording chain.
    recorded_clear_commands_result outcome =
        reader_io_record_clear_commands<ClearCommandsEnvironment>(
            std::move(device),
            std::move(pool),
            std::move(framebuffer),
            std::move(render_pass),
            VkExtent2D{640, 480},
            VkClearColorValue{{0.0F, 0.0F, 0.0F, 1.0F}})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, failed_operation);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(is_framebuffer_released);
    CHECK(device_observer.expired());
    CHECK(pool_observer.expired());
    CHECK(pass_observer.expired());
}

TEST_CASE("[impure] repeated clear plans allocate distinct initial-state primary buffers")
{
    backend::MockApi api;
    ClearCommandsEnvironment environment{std::ref(api)};
    lf::lazy_pool workers{2};
    char device_storage{}, pool_storage{}, framebuffer_storage{}, pass_storage{}, first_buffer{},
        second_buffer{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkCommandPool pool_handle = reinterpret_cast<VkCommandPool>(&pool_storage);
    VkCommandBuffer first_handle = reinterpret_cast<VkCommandBuffer>(&first_buffer);
    VkCommandBuffer second_handle = reinterpret_cast<VkCommandBuffer>(&second_buffer);
    device_ptr device{device_handle, [](auto) {}};
    command_pool_ptr pool{pool_handle, [](auto) {}};
    framebuffer_ptr framebuffer{reinterpret_cast<VkFramebuffer>(&framebuffer_storage), [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}};
    int allocation_count = 0;
    REQUIRE_CALL(api, vk_allocate_command_buffers(device_handle, trompeloeil::_, trompeloeil::_))
        .TIMES(2)
        .LR_SIDE_EFFECT(*_3 = ++allocation_count == 1 ? first_handle : second_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_begin_command_buffer(trompeloeil::_, trompeloeil::_))
        .TIMES(2)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api, vk_cmd_begin_render_pass(trompeloeil::_, trompeloeil::_, VK_SUBPASS_CONTENTS_INLINE))
        .TIMES(2);
    REQUIRE_CALL(api, vk_cmd_end_render_pass(trompeloeil::_)).TIMES(2);
    REQUIRE_CALL(api, vk_end_command_buffer(trompeloeil::_)).TIMES(2).RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_free_command_buffers(device_handle, pool_handle, 1, trompeloeil::_))
        .WITH(*_4 == first_handle || *_4 == second_handle)
        .TIMES(2);
    auto plan = reader_io_record_clear_commands<ClearCommandsEnvironment>(
        std::move(device),
        std::move(pool),
        std::move(framebuffer),
        std::move(render_pass),
        VkExtent2D{640, 480},
        VkClearColorValue{{0.0F, 0.0F, 0.0F, 1.0F}});

    // Sequential executions allocate new buffers rather than re-beginning an executable one.
    recorded_clear_commands_result first = plan.build(environment).sync_wait(workers);
    recorded_clear_commands_result second = plan.build(environment).sync_wait(workers);

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    CHECK_EQ(fnfelt::result::get_success(first).command_buffer.get(), first_handle);
    CHECK_EQ(fnfelt::result::get_success(second).command_buffer.get(), second_handle);
    CHECK_EQ(allocation_count, 2);
}

TEST_CASE("[integration][windowed] a clear pass can be recorded without acquiring or submitting")
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
    sdl::sdl_result<immer::array<extension_name>> extensions = runtime.run(
        sdl::reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
            fnfelt::result::get_success(window)));
    REQUIRE(fnfelt::result::is_success(extensions));
    renderer_result<instance_ptr> instance = runtime.run(
        reader_io_vk_create_instance<environment_type>(fnfelt::result::get_success(extensions)));
    REQUIRE(fnfelt::result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance)));
    REQUIRE(fnfelt::result::is_success(surface));
    presentation_device_result device = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            fnfelt::result::get_success(instance), fnfelt::result::get_success(surface)));
    REQUIRE(fnfelt::result::is_success(device));
    presentation_swapchain_images_result images = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            fnfelt::result::get_success(device), runtime.resources().window_owner));
    REQUIRE(fnfelt::result::is_success(images));
    presentation_image_views_result views = runtime.run(
        reader_io_create_presentation_image_views<environment_type>(
            fnfelt::result::get_success(std::move(images))));
    REQUIRE(fnfelt::result::is_success(views));
    PresentationSwapchain const & presentation_swapchain =
        fnfelt::result::get_success(views).presentation_swapchain_images.presentation_swapchain;
    VkExtent2D extent = presentation_swapchain.configuration.extent;
    vulkan_result<render_pass_ptr> render_pass = runtime.run(
        reader_io_vk_create_render_pass<environment_type>(
            presentation_swapchain.presentation_device.device,
            presentation_swapchain.configuration.surface_format.format));
    REQUIRE(fnfelt::result::is_success(render_pass));
    presentation_framebuffers_result framebuffers = runtime.run(
        reader_io_create_presentation_framebuffers<environment_type>(
            fnfelt::result::get_success(std::move(views)),
            fnfelt::result::get_success(render_pass)));
    REQUIRE(fnfelt::result::is_success(framebuffers));
    REQUIRE_FALSE(fnfelt::result::get_success(framebuffers).framebuffers.empty());
    PresentationDevice const & presentation_device = fnfelt::result::get_success(device);
    vulkan_result<command_pool_ptr> pool = runtime.run(
        reader_io_vk_create_command_pool<environment_type>(
            presentation_device.device, presentation_device.queue_families.graphics_family_index));
    REQUIRE(fnfelt::result::is_success(pool));

    // Recording only references the swapchain image; acquisition is required before execution.
    recorded_clear_commands_result outcome = runtime.run(
        reader_io_record_clear_commands<environment_type>(
            presentation_device.device,
            fnfelt::result::get_success(pool),
            fnfelt::result::get_success(framebuffers).framebuffers[0],
            fnfelt::result::get_success(render_pass),
            extent,
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}}));

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome).command_buffer != nullptr);
    CHECK_EQ(
        fnfelt::result::get_success(outcome).framebuffer,
        fnfelt::result::get_success(framebuffers).framebuffers[0]);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
