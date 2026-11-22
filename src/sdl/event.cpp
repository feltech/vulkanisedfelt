// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./event.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_video.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./event_types.hpp"
#include "./quit_event.hpp"
#include "./video.hpp"
#include "./window.hpp"
#include "./window_id.hpp"
#include "./window_id_types.hpp"
#include "./window_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[impure] event-poll plans do not consume events before execution")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};

    // Planning and materializing an IO leave the global event queue untouched.
    auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();
    [[maybe_unused]] auto materialized = plan.build(runtime.environment()).io();
}

TEST_CASE("[impure] event polling preserves resize data and distinguishes an empty queue")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    SDL_Event resized{};
    resized.window.type = SDL_WINDOWEVENT;
    resized.window.timestamp = 17;
    resized.window.windowID = 41;
    resized.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resized.window.data1 = 960;
    resized.window.data2 = 540;
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_1 = resized)
        .RETURN(1)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(0)
        .IN_SEQUENCE(calls);
    auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

    std::optional<event> first = runtime.run(plan);
    std::optional<event> second = runtime.run(plan);

    REQUIRE(first.has_value());
    REQUIRE(std::holds_alternative<NativeEvent>(*first));
    SDL_WindowEvent const & snapshot = std::get<NativeEvent>(*first).native.window;
    CHECK_EQ(snapshot.timestamp, 17);
    CHECK_EQ(snapshot.windowID, 41);
    CHECK_EQ(snapshot.event, SDL_WINDOWEVENT_SIZE_CHANGED);
    CHECK_EQ(snapshot.data1, 960);
    CHECK_EQ(snapshot.data2, 540);
    CHECK_FALSE(second.has_value());
}

TEST_CASE("[impure] pointer-free keyboard payloads remain intact")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    SDL_Event pressed{};
    pressed.key.type = SDL_KEYDOWN;
    pressed.key.windowID = 53;
    pressed.key.repeat = 1;
    pressed.key.keysym.sym = SDLK_SPACE;
    pressed.key.keysym.mod = KMOD_SHIFT;
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_)).SIDE_EFFECT(*_1 = pressed).RETURN(1);
    auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

    std::optional<event> outcome = runtime.run(plan);

    REQUIRE(outcome.has_value());
    REQUIRE(std::holds_alternative<NativeEvent>(*outcome));
    SDL_KeyboardEvent const & snapshot = std::get<NativeEvent>(*outcome).native.key;
    CHECK_EQ(snapshot.windowID, 53);
    CHECK_EQ(snapshot.repeat, 1);
    CHECK_EQ(snapshot.keysym.sym, SDLK_SPACE);
    CHECK_EQ(snapshot.keysym.mod, KMOD_SHIFT);
}

TEST_CASE("[impure] drop text retains the native allocation until the last event copy")
{
    for (SDL_EventType type : {SDL_DROPFILE, SDL_DROPTEXT})
    {
        backend::MockApi api;
        detail::TestRuntime runtime{api};
        char buffer[] = "dropped payload";
        bool is_freed = false;
        REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
            .LR_SIDE_EFFECT(_1->drop = SDL_DropEvent{type, 19, buffer, 47})
            .RETURN(1);
        REQUIRE_CALL(api, sdl_free(static_cast<void *>(buffer))).LR_SIDE_EFFECT(is_freed = true);
        auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

        std::optional<event> outcome = runtime.run(plan);

        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<DropEvent>(*outcome));
        DropEvent const & dropped = std::get<DropEvent>(*outcome);
        CHECK_EQ(dropped.type, type);
        CHECK_EQ(dropped.timestamp, 19);
        CHECK(dropped.window == window_id{47});
        CHECK_EQ(dropped.text.get(), buffer);
        event shared = *outcome;
        outcome.reset();
        CHECK_FALSE(is_freed);

        shared = UnsupportedEvent{SDL_USEREVENT, 0};

        CHECK(is_freed);
    }
}

TEST_CASE("[impure] drop begin and completion events have no text allocation")
{
    for (SDL_EventType type : {SDL_DROPBEGIN, SDL_DROPCOMPLETE})
    {
        backend::MockApi api;
        detail::TestRuntime runtime{api};
        REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
            .SIDE_EFFECT(_1->drop = SDL_DropEvent{type, 23, nullptr, 61})
            .RETURN(1);
        auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

        std::optional<event> outcome = runtime.run(plan);

        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<DropEvent>(*outcome));
        DropEvent const & dropped = std::get<DropEvent>(*outcome);
        CHECK_EQ(dropped.type, type);
        CHECK_EQ(dropped.timestamp, 23);
        CHECK(dropped.window == window_id{61});
        CHECK_FALSE(dropped.text);
    }
}

TEST_CASE("[impure] extended editing text shares ownership without copying")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    char buffer[] = "extended composition";
    bool is_freed = false;
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .LR_SIDE_EFFECT(
            _1->editExt = SDL_TextEditingExtEvent{SDL_TEXTEDITING_EXT, 29, 71, buffer, 3, 8})
        .RETURN(1);
    REQUIRE_CALL(api, sdl_free(static_cast<void *>(buffer))).LR_SIDE_EFFECT(is_freed = true);
    auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

    std::optional<event> outcome = runtime.run(plan);

    REQUIRE(outcome.has_value());
    REQUIRE(std::holds_alternative<ExtendedTextEditingEvent>(*outcome));
    ExtendedTextEditingEvent const & editing = std::get<ExtendedTextEditingEvent>(*outcome);
    CHECK_EQ(editing.timestamp, 29);
    CHECK(editing.window == window_id{71});
    CHECK_EQ(editing.start, 3);
    CHECK_EQ(editing.length, 8);
    CHECK_EQ(editing.text.get(), buffer);
    event shared = *outcome;
    outcome.reset();
    CHECK_FALSE(is_freed);

    shared = UnsupportedEvent{SDL_USEREVENT, 0};

    CHECK(is_freed);
}

TEST_CASE("[impure] missing extended editing text preserves event metadata")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
        .SIDE_EFFECT(
            _1->editExt = SDL_TextEditingExtEvent{SDL_TEXTEDITING_EXT, 37, 79, nullptr, 2, 4})
        .RETURN(1);
    auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

    std::optional<event> outcome = runtime.run(plan);

    REQUIRE(outcome.has_value());
    REQUIRE(std::holds_alternative<ExtendedTextEditingEvent>(*outcome));
    ExtendedTextEditingEvent const & editing = std::get<ExtendedTextEditingEvent>(*outcome);
    CHECK_EQ(editing.timestamp, 37);
    CHECK(editing.window == window_id{79});
    CHECK_EQ(editing.start, 2);
    CHECK_EQ(editing.length, 4);
    CHECK_FALSE(editing.text);
}

TEST_CASE("[impure] unsupported event payloads are neither borrowed nor freed")
{
    for (Uint32 type : {Uint32{SDL_SYSWMEVENT}, Uint32{SDL_USEREVENT + 7}, Uint32{0x4321}})
    {
        backend::MockApi api;
        detail::TestRuntime runtime{api};
        char application_payload{};
        REQUIRE_CALL(api, sdl_poll_event(trompeloeil::_))
            .LR_SIDE_EFFECT(
                _1->user = SDL_UserEvent{type, 31, 0, 99, &application_payload, nullptr})
            .RETURN(1);
        auto plan = reader_io_sdl_poll_event<detail::TestState<backend::MockApi>>();

        std::optional<event> outcome = runtime.run(plan);

        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<UnsupportedEvent>(*outcome));
        UnsupportedEvent const & unsupported = std::get<UnsupportedEvent>(*outcome);
        CHECK_EQ(unsupported.type, type);
        CHECK_EQ(unsupported.timestamp, 31);
    }
}

TEST_CASE("[integration][windowed] polling preserves window events and detects quit requests")
{
    backend::Api api;
    using environment_type = detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    detail::TestRuntime runtime{api};
    sdl_result<void> video = runtime.run(state_io_sdl_initialize_video<state_type>());
    REQUIRE(fnfelt::result::is_success(video));
    WindowOptions options{
        .width = window_width{64}, .height = window_height{48}, .flags = SDL_WINDOW_HIDDEN};
    sdl_result<SDL_Window *> window = runtime.run(state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    sdl_result<window_id> identifier = runtime.run(
        reader_io_sdl_get_window_id<environment_type>(runtime.resources().window_owner));
    REQUIRE(fnfelt::result::is_success(identifier));
    window_id target_window = fnfelt::result::get_success(identifier);
    REQUIRE_NE(value_of(target_window), 0);
    SDL_Event resized{};
    resized.window.type = SDL_WINDOWEVENT;
    resized.window.windowID = value_of(target_window);
    resized.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resized.window.data1 = 123;
    resized.window.data2 = 87;
    SDL_Event closed = resized;
    closed.window.event = SDL_WINDOWEVENT_CLOSE;
    SDL_Event quit{};
    quit.quit.type = SDL_QUIT;
    REQUIRE_EQ(SDL_PushEvent(&resized), 1);
    REQUIRE_EQ(SDL_PushEvent(&closed), 1);
    REQUIRE_EQ(SDL_PushEvent(&quit), 1);
    // The runtime's state keeps video and the window alive throughout polling.
    auto poll_event = reader_io_sdl_poll_event<environment_type>();
    bool has_resize = false;
    bool has_close = false;
    bool has_quit = false;

    // Startup events may precede the three controlled events; polling remains bounded.
    for (std::size_t attempt = 0; attempt != 256 && !(has_resize && has_close && has_quit);
         ++attempt)
    {
        std::optional<event> outcome = runtime.run(poll_event);
        if (!outcome)
        {
            continue;
        }
        NativeEvent const * native = std::get_if<NativeEvent>(&*outcome);
        if (!native)
        {
            continue;
        }
        if (native->native.type == SDL_QUIT && has_close)
        {
            CHECK(should_quit(*outcome, target_window));
            has_quit = true;
        }
        else if (
            native->native.type == SDL_WINDOWEVENT &&
            native->native.window.windowID == value_of(target_window))
        {
            if (native->native.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
                native->native.window.data1 == 123 && native->native.window.data2 == 87)
            {
                CHECK_EQ(native->native.window.data1, 123);
                CHECK_EQ(native->native.window.data2, 87);
                CHECK_FALSE(should_quit(*outcome, target_window));
                has_resize = true;
            }
            else if (native->native.window.event == SDL_WINDOWEVENT_CLOSE && has_resize)
            {
                CHECK(should_quit(*outcome, target_window));
                has_close = true;
            }
        }
    }

    CHECK(has_resize);
    CHECK(has_close);
    CHECK(has_quit);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
