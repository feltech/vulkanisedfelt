// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./window_id.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

#include <SDL_video.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./window_id_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[impure] window-identifier plans retain ownership without querying")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    char storage{};
    window_ptr window{reinterpret_cast<SDL_Window *>(&storage), [](SDL_Window *) {}};
    std::weak_ptr weak_window{window};

    {
        auto plan = reader_io_sdl_get_window_id<detail::TestState<backend::MockApi>>(window);
        window.reset();
        [[maybe_unused]] auto materialized = plan.build(runtime.environment()).io();

        // Both the plan and its materialized IO retain the window without calling SDL.
        CHECK_FALSE(weak_window.expired());
    }

    CHECK(weak_window.expired());
}

TEST_CASE("[impure] window-identifier queries run on main and refresh on replay")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char storage{};
    window_ptr window{reinterpret_cast<SDL_Window *>(&storage), [](SDL_Window *) {}};
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, sdl_get_window_id(window.get()))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(41)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, sdl_get_window_id(window.get()))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(73)
        .IN_SEQUENCE(calls);
    auto plan = reader_io_sdl_get_window_id<detail::TestState<backend::MockApi>>(window);

    sdl_result<window_id> first = runtime.run(plan);
    sdl_result<window_id> second = runtime.run(plan);

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    CHECK(fnfelt::result::get_success(first) == window_id{41});
    CHECK(fnfelt::result::get_success(second) == window_id{73});
}

TEST_CASE("[impure] window-identifier failures own error text copied on main")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char storage{};
    window_ptr window{reinterpret_cast<SDL_Window *>(&storage), [](SDL_Window *) {}};
    char error_text[] = "unknown window";
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, sdl_get_window_id(window.get())).RETURN(0).IN_SEQUENCE(calls);
    REQUIRE_CALL(api, sdl_get_error())
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_RETURN(error_text)
        .IN_SEQUENCE(calls);
    auto plan = reader_io_sdl_get_window_id<detail::TestState<backend::MockApi>>(window);

    sdl_result<window_id> outcome = runtime.run(plan);
    error_text[0] = 'X';

    REQUIRE(fnfelt::result::is_error(outcome));
    SdlFailure const & failure = fnfelt::result::get_error(outcome);
    CHECK(failure.operation == FallibleSdlOperation::get_window_id);
    CHECK_EQ(std::string_view(failure.message.data(), failure.message.size()), "unknown window");
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
