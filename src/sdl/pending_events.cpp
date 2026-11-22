// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./pending_events.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <thread>
#include <variant>

#include <SDL_events.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <immer/array.hpp>

#include "../backend/mock_api.hpp"
#include "./event_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[impure] pending event collection is lazy and accepts a zero bound")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    auto empty_plan = reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
        event_batch_limit{0});
    [[maybe_unused]] auto pending_plan =
        reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
            event_batch_limit{8});

    immer::array<event> events = runtime.run(empty_plan);

    CHECK(events.empty());
}

TEST_CASE("[impure] pending event collection stops at an empty poll on main")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id main_thread = std::this_thread::get_id();
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(0);
    auto plan = reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
        event_batch_limit{8});

    immer::array<event> events = runtime.run(plan);

    CHECK(events.empty());
}

TEST_CASE("[impure] pending event batches preserve FIFO and leave capped events queued")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id main_thread = std::this_thread::get_id();
    trompeloeil::sequence polling;
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(_1->common = SDL_CommonEvent{SDL_KEYDOWN, 11})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(_1->common = SDL_CommonEvent{SDL_KEYUP, 12})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(_1->common = SDL_CommonEvent{SDL_QUIT, 13})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    auto plan = reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
        event_batch_limit{2});
    auto remainder = reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
        event_batch_limit{1});

    immer::array<event> first = runtime.run(plan);
    CHECK_EQ(first.size(), 2);
    // No lookahead consumes the quit event after the batch reaches its bound.
    immer::array<event> second = runtime.run(remainder);

    REQUIRE_EQ(first.size(), 2);
    CHECK_EQ(std::get<NativeEvent>(first[0]).native.common.timestamp, 11);
    CHECK_EQ(std::get<NativeEvent>(first[1]).native.common.timestamp, 12);
    REQUIRE_EQ(second.size(), 1);
    CHECK_EQ(std::get<NativeEvent>(second[0]).native.type, SDL_QUIT);
}

TEST_CASE("[impure] pending event batches share SDL text until the last batch is released")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    char text[] = "owned text";
    bool is_freed = false;
    trompeloeil::sequence polling;
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .LR_SIDE_EFFECT(_1->drop = SDL_DropEvent{SDL_DROPTEXT, 1, text, 3})
        .RETURN(1)
        .IN_SEQUENCE(polling);
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_)).RETURN(0).IN_SEQUENCE(polling);
    REQUIRE_CALL(api, sdl_free(static_cast<void *>(text))).LR_SIDE_EFFECT(is_freed = true);
    auto plan = reader_io_sdl_collect_pending_events<detail::TestState<backend::MockApi>>(
        event_batch_limit{8});

    immer::array<event> events = runtime.run(plan);
    immer::array shared_events{events};
    events = {};

    REQUIRE_EQ(shared_events.size(), 1);
    CHECK_EQ(std::get<DropEvent>(shared_events[0]).text.get(), text);
    CHECK_FALSE(is_freed);
    shared_events = {};
    CHECK(is_freed);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
