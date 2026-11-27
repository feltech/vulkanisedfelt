// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./resize_event.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <variant>

#include <SDL_events.h>
#include <doctest/doctest.h>

#include "./event_types.hpp"
#include "./window_id_types.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[pure] window resize classification matches only the target window")
{
    for (Uint8 event_kind : {SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED})
    {
        SDL_Event native{};
        native.window.type = SDL_WINDOWEVENT;
        native.window.windowID = 41;
        native.window.event = event_kind;
        native.window.data1 = 1024;
        native.window.data2 = 768;
        event polled_event{NativeEvent{native}};

        CHECK(is_window_resize(polled_event, window_id{41}));
        CHECK_FALSE(is_window_resize(polled_event, window_id{73}));
    }
}

TEST_CASE("[pure] non-resize events remain intact without reporting a resize")
{
    SDL_Event close{};
    close.window.type = SDL_WINDOWEVENT;
    close.window.windowID = 41;
    close.window.event = SDL_WINDOWEVENT_CLOSE;
    SDL_Event minimized{};
    minimized.window.type = SDL_WINDOWEVENT;
    minimized.window.windowID = 41;
    minimized.window.event = SDL_WINDOWEVENT_MINIMIZED;
    SDL_Event moved{};
    moved.window.type = SDL_WINDOWEVENT;
    moved.window.windowID = 41;
    moved.window.event = SDL_WINDOWEVENT_MOVED;
    moved.window.data1 = 55;
    moved.window.data2 = 66;
    SDL_Event quit{};
    quit.quit.type = SDL_QUIT;

    event close_event{NativeEvent{close}};
    event minimized_event{NativeEvent{minimized}};
    event moved_event{NativeEvent{moved}};
    event quit_event{NativeEvent{quit}};
    event drop_event{DropEvent{SDL_DROPBEGIN, 0, window_id{41}, {}}};
    event editing_event{ExtendedTextEditingEvent{0, window_id{41}, 1, 2, {}}};
    event unsupported_event{UnsupportedEvent{SDL_USEREVENT, 0}};

    CHECK_FALSE(is_window_resize(close_event, window_id{41}));
    CHECK_FALSE(is_window_resize(minimized_event, window_id{41}));
    CHECK_FALSE(is_window_resize(moved_event, window_id{41}));
    CHECK_FALSE(is_window_resize(quit_event, window_id{41}));
    CHECK_FALSE(is_window_resize(drop_event, window_id{41}));
    CHECK_FALSE(is_window_resize(editing_event, window_id{41}));
    CHECK_FALSE(is_window_resize(unsupported_event, window_id{41}));
    CHECK_EQ(std::get<NativeEvent>(moved_event).native.window.data1, 55);
    CHECK_EQ(std::get<NativeEvent>(moved_event).native.window.data2, 66);
    CHECK_EQ(std::get<ExtendedTextEditingEvent>(editing_event).start, 1);
    CHECK_EQ(std::get<ExtendedTextEditingEvent>(editing_event).length, 2);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
