// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./window.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./video.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
namespace
{
using backend::MockApi;
using mock_environment = detail::TestEnvironment<MockApi>;

auto hidden_options() -> WindowOptions
{
    return {
        .title = text::String{"test"},
        .width = window_width{64},
        .height = window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
}
}  // namespace

TEST_CASE("[pure] SDL window dimensions are distinct domains")
{
    static_assert(!std::is_convertible_v<window_width, window_height>);
    static_assert(!std::is_convertible_v<int, window_width>);
    static_assert(!std::is_convertible_v<window_width, int>);
    CHECK_EQ(value_of(hidden_options().width), 64);
    CHECK_EQ(value_of(hidden_options().height), 48);
}

TEST_CASE("[impure] SDL window is lazy main-owned and retains video until destruction")
{
    MockApi api;
    trompeloeil::sequence order;
    auto const main_thread = std::this_thread::get_id();
    // The mock treats this stable object address as an opaque token, never dereferencing it.
    int fake_window = 0;
    auto * handle = reinterpret_cast<SDL_Window *>(&fake_window);
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(
        api,
        sdl_create_window(
            trompeloeil::_,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            64,
            48,
            static_cast<Uint32>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)))
        .WITH(std::strcmp(_1, "test") == 0 && std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order)
        .RETURN(handle);
    REQUIRE_CALL(api, sdl_destroy_window(handle))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order);

    detail::TestRuntime runtime{api};
    auto video = runtime.run(reader_io_sdl_initialize_video<mock_environment>());
    REQUIRE(fnfelt::result::is_success(video));
    auto plan = reader_io_sdl_create_window<mock_environment>(hidden_options());
    [[maybe_unused]] auto discarded = plan.build(runtime.environment());

    auto result = runtime.run(plan);

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result) == handle);
    CHECK(runtime.resources().window_owner.get() == handle);
    // Main-owned anchors outlive every worker result, including failed later runs.
}

TEST_CASE("[impure] SDL window copies share ownership until the final main-thread release")
{
    MockApi api;
    trompeloeil::sequence order;
    auto const main_thread = std::this_thread::get_id();
    bool is_window_destroyed = false;
    bool is_video_stopped = false;
    int fake_window = 0;
    auto * handle = reinterpret_cast<SDL_Window *>(&fake_window);
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(
        api,
        sdl_create_window(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_))
        .IN_SEQUENCE(order)
        .RETURN(handle);
    REQUIRE_CALL(api, sdl_destroy_window(handle))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order)
        .LR_SIDE_EFFECT(is_window_destroyed = true);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .IN_SEQUENCE(order)
        .LR_SIDE_EFFECT(is_video_stopped = true);
    detail::TestRuntime runtime{api};
    REQUIRE(
        fnfelt::result::is_success(
            runtime.run(reader_io_sdl_initialize_video<mock_environment>())));
    REQUIRE(
        fnfelt::result::is_success(
            runtime.run(reader_io_sdl_create_window<mock_environment>(hidden_options()))));

    // A copied owner keeps the window alive after the holder releases its share.
    auto & resources = runtime.environment().sdl_resources.get();
    auto owner = resources.window_owner;
    std::weak_ptr<SDL_Window> observer = owner;
    resources.window_owner.reset();
    resources.video_owner.reset();

    CHECK(owner.get() == handle);
    CHECK_FALSE(is_window_destroyed);
    CHECK(runtime.resources().window_owner.get() == nullptr);
    CHECK_FALSE(is_video_stopped);

    // Final window release also releases video, even while a weak observer remains.
    owner.reset();

    CHECK(is_window_destroyed);
    CHECK(is_video_stopped);
    CHECK(observer.expired());
}

TEST_CASE("[impure] assigning SDL resource snapshots destroys each window before video")
{
    MockApi api;
    trompeloeil::sequence order;
    int first_token = 0;
    int second_token = 0;
    auto * first_handle = reinterpret_cast<SDL_Window *>(&first_token);
    auto * second_handle = reinterpret_cast<SDL_Window *>(&second_token);
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(
        api,
        sdl_create_window(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_))
        .IN_SEQUENCE(order)
        .RETURN(first_handle);
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(
        api,
        sdl_create_window(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_))
        .IN_SEQUENCE(order)
        .RETURN(second_handle);
    REQUIRE_CALL(api, sdl_destroy_window(first_handle)).IN_SEQUENCE(order);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order);
    REQUIRE_CALL(api, sdl_destroy_window(second_handle)).IN_SEQUENCE(order);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order);
    SdlResources first;
    SdlResources second;
    REQUIRE(fnfelt::result::is_success(detail::initialize_video(std::ref(api), first)));
    REQUIRE(
        fnfelt::result::is_success(detail::create_window(hidden_options(), std::ref(api), first)));
    REQUIRE(fnfelt::result::is_success(detail::initialize_video(std::ref(api), second)));
    REQUIRE(
        fnfelt::result::is_success(detail::create_window(hidden_options(), std::ref(api), second)));

    // Replacing live state releases its previous video share before its window share.
    // The window's own video dependency preserves cleanup order in both assignments.
    SUBCASE("copy assignment")
    {
        first = second;
    }
    SUBCASE("move assignment")
    {
        first = std::move(second);
    }

    CHECK(first.window_owner.get() == second_handle);
}

TEST_CASE("[impure] SDL window failure copies its error and keeps video alive")
{
    MockApi api;
    auto error = std::to_array("window failed");
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(0);
    REQUIRE_CALL(
        api,
        sdl_create_window(
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_,
            trompeloeil::_))
        .RETURN(nullptr);
    REQUIRE_CALL(api, sdl_get_error()).RETURN(error.data());
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO));
    detail::TestRuntime runtime{api};
    auto video = runtime.run(reader_io_sdl_initialize_video<mock_environment>());
    REQUIRE(fnfelt::result::is_success(video));

    auto result = runtime.run(reader_io_sdl_create_window<mock_environment>(hidden_options()));
    error[0] = 'X';

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).operation == FallibleSdlOperation::create_window);
    CHECK_EQ(fnfelt::result::get_error(result).message[0], 'w');
    CHECK(runtime.resources().window_owner.get() == nullptr);
    CHECK(runtime.resources().video_owner != nullptr);
}

TEST_CASE("[integration][windowed] SDL creates a Vulkan window and releases it on main")
{
    backend::Api api;
    using environment_type = detail::TestEnvironment<backend::Api>;
    detail::TestRuntime runtime{api};
    auto video = runtime.run(reader_io_sdl_initialize_video<environment_type>());
    REQUIRE(fnfelt::result::is_success(video));

    auto window = runtime.run(reader_io_sdl_create_window<environment_type>(hidden_options()));

    REQUIRE(fnfelt::result::is_success(window));
    CHECK(fnfelt::result::get_success(window) != nullptr);
    CHECK(fnfelt::result::get_success(window) == runtime.resources().window_owner.get());
}
}  // namespace vulkandemo::sdl

namespace vulkandemo::backend
{
namespace
{
template <class TApi>
struct SdlQueryEnvironment
{
    std::reference_wrapper<TApi> api;
    lf::worker_context * main_context;
};

template <class TApi>
struct SdlResourceEnvironment
{
    std::reference_wrapper<TApi> api;
    lf::worker_context * main_context;
    std::reference_wrapper<sdl::SdlResources> sdl_resources;
};

static_assert(sdl::sdl_create_window_environment<SdlResourceEnvironment<MockApi> const &>);
static_assert(!sdl::sdl_create_window_environment<SdlQueryEnvironment<Api>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
