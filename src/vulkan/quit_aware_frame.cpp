// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./quit_aware_frame.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

#include <SDL_events.h>
#include <SDL_video.h>
#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/errors.hpp"
#include "../sdl/event_types.hpp"
#include "../sdl/pending_events.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../sdl/window_id.hpp"
#include "../sdl/window_types.hpp"
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./presentation_device.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_framebuffers.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_image_views_types.hpp"
#include "./presentation_instance.hpp"
#include "./presentation_instance_types.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./quit_aware_frame_types.hpp"
#include "./render_pass.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct QuitAwareFrameEnvironment : sdl::detail::TestState<TApi>
{
    PresentationFramebuffers targets;
    command_pool_ptr command_pool;
};

template <class TApi, class TPlan>
quit_aware_frame_result run_quit_aware_frame(
    sdl::detail::TestRuntime<TApi> & runtime,
    TPlan const & plan,
    PresentationFramebuffers targets,
    command_pool_ptr command_pool)
{
    using fnfelt::literals::operator""_ss;
    using base_environment = sdl::detail::TestState<TApi>;
    // The test runtime services main-thread dispatch while executing the frame's own environment.
    auto run_frame = [&](base_environment execution)
    {
        return plan
            .build(
                QuitAwareFrameEnvironment<TApi>{
                    std::move(execution), std::move(targets), std::move(command_pool)})
            .io();
    };
    return runtime.run(
        fnfelt::monad::reader_io::create<"run_quit_aware_frame_test"_ss, base_environment>(
            std::move(run_frame)));
}

struct QuitAwareFrameFixture
{
    backend::MockApi api;
    sdl::detail::TestRuntime<backend::MockApi> runtime{api};
    char device_storage{}, pool_storage{}, render_pass_storage{}, framebuffer_storage{};
    PresentationFramebuffers targets;
    command_pool_ptr command_pool{reinterpret_cast<VkCommandPool>(&pool_storage), [](auto) {}};

    QuitAwareFrameFixture()
    {
        targets.render_pass = {reinterpret_cast<VkRenderPass>(&render_pass_storage), [](auto) {}};
        targets.framebuffers = immer::array<framebuffer_ptr>{
            {reinterpret_cast<VkFramebuffer>(&framebuffer_storage), [](auto) {}}};
        targets.presentation_image_views.presentation_swapchain_images.presentation_swapchain
            .presentation_device.device = {
            reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    }

    auto operation(sdl::event_batch_limit limit = sdl::event_batch_limit{8})
    {
        return reader_io_clear_and_present_unless_quit<QuitAwareFrameEnvironment<backend::MockApi>>(
            sdl::window_id{41},
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}},
            timeout_nanoseconds{1'000'000'000},
            limit);
    }
};
}  // namespace

TEST_CASE("[impure] quit-aware frame planning has no effects")
{
    QuitAwareFrameFixture fixture;

    auto operation = fixture.operation();
    [[maybe_unused]] auto bound =
        operation
            .build(
                QuitAwareFrameEnvironment<backend::MockApi>{
                    fixture.runtime.environment(), fixture.targets, fixture.command_pool})
            .io();
}

TEST_CASE("[impure] quit requests cancel before every new frame effect")
{
    for (bool is_global_quit : {false, true})
    {
        QuitAwareFrameFixture fixture;
        SDL_Event request{};
        request.type = is_global_quit ? SDL_QUIT : SDL_WINDOWEVENT;
        request.window.windowID = 41;
        request.window.event = SDL_WINDOWEVENT_CLOSE;
        std::thread::id main_thread = std::this_thread::get_id();
        trompeloeil::sequence polling;
        REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
            .WITH(std::this_thread::get_id() == main_thread)
            .SIDE_EFFECT(*_1 = request)
            .RETURN(1)
            .IN_SEQUENCE(polling);
        REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_)).RETURN(0).IN_SEQUENCE(polling);
        // Unconfigured Vulkan mocks reject even synchronization creation on the quit branch.
        auto operation = fixture.operation();

        quit_aware_frame_result outcome =
            run_quit_aware_frame(fixture.runtime, operation, fixture.targets, fixture.command_pool);

        CHECK(fnfelt::result::is_stopped(outcome));
    }
}

TEST_CASE("[impure] unrelated window close passes the checkpoint and preserves frame failures")
{
    QuitAwareFrameFixture fixture;
    bool is_polling_complete = false;
    std::weak_ptr pool_owner{fixture.command_pool};
    std::weak_ptr render_pass_owner{fixture.targets.render_pass};
    trompeloeil::sequence polling;
    REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(
            _1->window =
                SDL_WindowEvent{SDL_WINDOWEVENT, 0, 42, SDL_WINDOWEVENT_CLOSE, 0, 0, 0, 0, 0})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
        .LR_SIDE_EFFECT(is_polling_complete = true)
        .RETURN(0)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(
        fixture.api, vk_create_fence(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_WITH(is_polling_complete && !pool_owner.expired() && !render_pass_owner.expired())
        .TIMES(3)
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    REQUIRE_CALL(
        fixture.api, vk_create_semaphore(trompeloeil::_, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_WITH(is_polling_complete)
        .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    auto operation = fixture.operation();

    quit_aware_frame_result outcome = run_quit_aware_frame(
        fixture.runtime, operation, std::move(fixture.targets), std::move(fixture.command_pool));

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_fence);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(pool_owner.expired());
    CHECK(render_pass_owner.expired());
}

TEST_CASE("[impure] capped quit checkpoint leaves later requests for the next execution")
{
    QuitAwareFrameFixture fixture;
    trompeloeil::sequence polling;
    REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(
            _1->window =
                SDL_WindowEvent{SDL_WINDOWEVENT, 0, 41, SDL_WINDOWEVENT_CLOSE, 0, 0, 0, 0, 0})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(_1->quit = SDL_QuitEvent{SDL_QUIT, 0})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    auto operation = fixture.operation(sdl::event_batch_limit{1});

    quit_aware_frame_result first =
        run_quit_aware_frame(fixture.runtime, operation, fixture.targets, fixture.command_pool);
    quit_aware_frame_result second =
        run_quit_aware_frame(fixture.runtime, operation, fixture.targets, fixture.command_pool);

    CHECK(fnfelt::result::is_stopped(first));
    CHECK(fnfelt::result::is_stopped(second));
}

TEST_CASE("[impure] frame environment retains final owners through polling and cancellation")
{
    QuitAwareFrameFixture fixture;
    std::weak_ptr pool_owner{fixture.command_pool};
    std::weak_ptr render_pass_owner{fixture.targets.render_pass};
    REQUIRE_CALL(fixture.api, sdl_poll_event(trompeloeil::_))
        .LR_WITH(!pool_owner.expired() && !render_pass_owner.expired())
        .SIDE_EFFECT(_1->quit = SDL_QuitEvent{SDL_QUIT, 0})
        .RETURN(1);
    auto operation = fixture.operation(sdl::event_batch_limit{1});

    // The transient environment takes the fixture's last resource owners.
    quit_aware_frame_result outcome = run_quit_aware_frame(
        fixture.runtime, operation, std::move(fixture.targets), std::move(fixture.command_pool));

    CHECK(fnfelt::result::is_stopped(outcome));
    CHECK(pool_owner.expired());
    CHECK(render_pass_owner.expired());
}

namespace
{
struct IntegrationFrameTargets
{
    PresentationFramebuffers targets;
    command_pool_ptr command_pool;
    sdl::window_id window;
};

auto create_integration_frame_targets(sdl::detail::TestRuntime<backend::Api> & runtime)
    -> std::optional<IntegrationFrameTargets>
{
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    namespace result = fnfelt::result;
    REQUIRE(result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<state_type>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    REQUIRE(result::is_success(runtime.run(sdl::state_io_sdl_create_window<state_type>(options))));
    sdl::sdl_result<sdl::window_id> identifier = runtime.run(
        sdl::reader_io_sdl_get_window_id<environment_type>(runtime.resources().window_owner));
    REQUIRE(result::is_success(identifier));
    renderer_result<PresentationInstance> instance = runtime.run(
        reader_io_create_presentation_instance<environment_type>(runtime.resources().window_owner));
    if (result::is_error(instance))
    {
        CHECK(std::holds_alternative<PresentationInstanceFailure>(result::get_error(instance)));
        return std::nullopt;
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
        return std::nullopt;
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
    VkFormat format = result::get_success(views)
                          .presentation_swapchain_images.presentation_swapchain.configuration
                          .surface_format.format;
    vulkan_result<render_pass_ptr> render_pass =
        runtime.run(reader_io_vk_create_render_pass<environment_type>(context.device, format));
    REQUIRE(result::is_success(render_pass));
    presentation_framebuffers_result targets = runtime.run(
        reader_io_create_presentation_framebuffers<environment_type>(
            result::get_success(std::move(views)), result::get_success(render_pass)));
    REQUIRE(result::is_success(targets));
    vulkan_result<command_pool_ptr> command_pool = runtime.run(
        reader_io_vk_create_command_pool<environment_type>(
            context.device, context.queue_families.graphics_family_index));
    REQUIRE(result::is_success(command_pool));

    // Discard startup input before injecting the specific events under test.
    immer::array<sdl::event> startup_events = runtime.run(
        sdl::reader_io_sdl_collect_pending_events<environment_type>(sdl::event_batch_limit{256}));
    REQUIRE_LT(startup_events.size(), 256);
    return IntegrationFrameTargets{
        result::get_success(std::move(targets)),
        result::get_success(std::move(command_pool)),
        result::get_success(identifier)};
}
}  // namespace

TEST_CASE("[integration][windowed] quit-aware frame cancels for a queued target close")
{
    backend::Api api;
    sdl::detail::TestRuntime runtime{api};
    std::optional frame{create_integration_frame_targets(runtime)};
    if (!frame)
    {
        return;
    }
    SDL_Event close{};
    close.window.type = SDL_WINDOWEVENT;
    close.window.windowID = value_of(frame->window);
    close.window.event = SDL_WINDOWEVENT_CLOSE;
    REQUIRE_EQ(SDL_PushEvent(&close), 1);

    // A real close request cancels before this otherwise fully configured frame begins.
    auto operation =
        reader_io_clear_and_present_unless_quit<QuitAwareFrameEnvironment<backend::Api>>(
            frame->window,
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}},
            timeout_nanoseconds{1'000'000'000},
            sdl::event_batch_limit{256});
    quit_aware_frame_result outcome = run_quit_aware_frame(
        runtime, operation, std::move(frame->targets), std::move(frame->command_pool));

    CHECK(fnfelt::result::is_stopped(outcome));
}

TEST_CASE("[integration][windowed] quit-aware frame presents and preserves other window input")
{
    backend::Api api;
    sdl::detail::TestRuntime runtime{api};
    std::optional frame{create_integration_frame_targets(runtime)};
    if (!frame)
    {
        return;
    }
    SDL_Event resized{};
    resized.window.type = SDL_WINDOWEVENT;
    resized.window.windowID = value_of(frame->window);
    resized.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resized.window.data1 = 123;
    resized.window.data2 = 87;
    SDL_Event foreign_close{};
    foreign_close.window.type = SDL_WINDOWEVENT;
    foreign_close.window.windowID = value_of(frame->window) + 1;
    foreign_close.window.event = SDL_WINDOWEVENT_CLOSE;
    REQUIRE_EQ(SDL_PushEvent(&resized), 1);
    REQUIRE_EQ(SDL_PushEvent(&foreign_close), 1);

    // Non-quit input permits the existing bounded render and survives in the successful output.
    auto operation =
        reader_io_clear_and_present_unless_quit<QuitAwareFrameEnvironment<backend::Api>>(
            frame->window,
            VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}},
            timeout_nanoseconds{1'000'000'000},
            sdl::event_batch_limit{256});
    quit_aware_frame_result outcome = run_quit_aware_frame(
        runtime, operation, std::move(frame->targets), std::move(frame->command_pool));

    REQUIRE(fnfelt::result::is_success(outcome));
    auto const & events = fnfelt::result::get_success(outcome).events;
    CHECK(
        std::ranges::any_of(
            events,
            [](sdl::event const & event)
            {
                auto const * native = std::get_if<sdl::NativeEvent>(&event);
                return native && native->native.type == SDL_WINDOWEVENT &&
                    native->native.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
                    native->native.window.data1 == 123 && native->native.window.data2 == 87;
            }));
    CHECK(
        std::ranges::any_of(
            events,
            [window = frame->window](sdl::event const & event)
            {
                auto const * native = std::get_if<sdl::NativeEvent>(&event);
                return native && native->native.type == SDL_WINDOWEVENT &&
                    native->native.window.event == SDL_WINDOWEVENT_CLOSE &&
                    native->native.window.windowID == value_of(window) + 1;
            }));
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
