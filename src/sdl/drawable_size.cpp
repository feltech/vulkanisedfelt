// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./drawable_size.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <thread>

#include <SDL.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./video.hpp"
#include "./window.hpp"
#include "./window_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[impure] drawable-size plans retain the window without querying")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    char storage{};
    window_ptr window{reinterpret_cast<SDL_Window *>(&storage), [](SDL_Window *) {}};
    std::weak_ptr weak_window{window};

    {
        auto plan =
            reader_io_sdl_vulkan_get_drawable_size<detail::TestState<backend::MockApi>>(window);
        window.reset();
        [[maybe_unused]] auto discarded = plan.build(runtime.environment());

        // Preparing an IO retains ownership but performs no SDL calls.
        CHECK_FALSE(weak_window.expired());
    }

    CHECK(weak_window.expired());
}

TEST_CASE("[impure] drawable-size queries run on main and refresh on replay")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char storage{};
    window_ptr window{reinterpret_cast<SDL_Window *>(&storage), [](SDL_Window *) {}};
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, sdl_vulkan_get_drawable_size(window.get(), trompeloeil::_, trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_2 = 1280)
        .SIDE_EFFECT(*_3 = 960)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, sdl_vulkan_get_drawable_size(window.get(), trompeloeil::_, trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_2 = 0)
        .SIDE_EFFECT(*_3 = 0)
        .IN_SEQUENCE(calls);
    auto plan = reader_io_sdl_vulkan_get_drawable_size<detail::TestState<backend::MockApi>>(window);

    DrawableSize first = runtime.run(plan);
    DrawableSize second = runtime.run(plan);

    CHECK(first.width == drawable_width{1280});
    CHECK(first.height == drawable_height{960});
    CHECK(second.width == drawable_width{0});
    CHECK(second.height == drawable_height{0});
    CHECK_FALSE(runtime.resources().window_owner);
}

TEST_CASE("[integration][windowed] a Vulkan window exposes live drawable pixels")
{
    backend::Api api;
    using environment_type = detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    detail::TestRuntime runtime{api};
    sdl_result<void> video = runtime.run(state_io_sdl_initialize_video<state_type>());
    REQUIRE(fnfelt::result::is_success(video));
    WindowOptions options{
        .width = window_width{64},
        .height = window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl_result<SDL_Window *> window = runtime.run(state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));

    DrawableSize size = runtime.run(
        reader_io_sdl_vulkan_get_drawable_size<environment_type>(runtime.resources().window_owner));

    CHECK_GT(value_of(size.width), 0);
    CHECK_GT(value_of(size.height), 0);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
