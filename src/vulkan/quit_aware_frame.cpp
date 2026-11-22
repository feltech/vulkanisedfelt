// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./quit_aware_frame.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <memory>
#include <thread>
#include <utility>

#include <SDL_events.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "./errors.hpp"
#include "./quit_aware_frame_types.hpp"

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
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
