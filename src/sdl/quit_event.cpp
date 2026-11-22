// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./quit_event.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <SDL_events.h>
#include <doctest/doctest.h>

#include "./event_types.hpp"
#include "./window_id_types.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[pure] application quit events apply to every target window")
{
    SDL_Event native{};
    native.quit.type = SDL_QUIT;
    event polled_event{NativeEvent{native}};

    CHECK(should_quit(polled_event, window_id{41}));
    CHECK(should_quit(polled_event, window_id{73}));
}

TEST_CASE("[pure] close requests apply only to the target window")
{
    SDL_Event native{};
    native.window.type = SDL_WINDOWEVENT;
    native.window.windowID = 41;
    native.window.event = SDL_WINDOWEVENT_CLOSE;
    event polled_event{NativeEvent{native}};

    CHECK(should_quit(polled_event, window_id{41}));
    CHECK_FALSE(should_quit(polled_event, window_id{73}));
}

TEST_CASE("[pure] non-quit events remain available without requesting shutdown")
{
    SDL_Event resized{};
    resized.window.type = SDL_WINDOWEVENT;
    resized.window.windowID = 41;
    resized.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resized.window.data1 = 960;
    resized.window.data2 = 540;
    event resize_event{NativeEvent{resized}};
    event drop_event{DropEvent{SDL_DROPBEGIN, 0, window_id{41}, {}}};
    event unsupported_event{UnsupportedEvent{SDL_USEREVENT, 0}};

    CHECK_FALSE(should_quit(resize_event, window_id{41}));
    CHECK_FALSE(should_quit(drop_event, window_id{41}));
    CHECK_FALSE(should_quit(unsupported_event, window_id{41}));
    CHECK_EQ(std::get<NativeEvent>(resize_event).native.window.data1, 960);
    CHECK_EQ(std::get<NativeEvent>(resize_event).native.window.data2, 540);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
