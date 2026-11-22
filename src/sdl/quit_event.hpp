// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/quit_event.hpp
 *
 * Pure quit-request classification for one SDL window.
 */
#pragma once

#include <variant>

#include <SDL_events.h>

#include "./event_types.hpp"
#include "./window_id_types.hpp"

namespace vulkandemo::sdl
{
/**
 * Identifies an application quit or the target window's close request.
 *
 * Other event data remains available for independent handling.
 *
 * @param polled_event Owned event snapshot.
 * @param target_window Window whose close request ends the application.
 * @return Whether the event requests application shutdown.
 */
[[nodiscard]] constexpr auto should_quit(
    event const & polled_event, window_id target_window) noexcept -> bool
{
    NativeEvent const * native = std::get_if<NativeEvent>(&polled_event);
    if (!native)
    {
        return false;
    }
    return native->native.type == SDL_QUIT ||
        (native->native.type == SDL_WINDOWEVENT &&
         native->native.window.event == SDL_WINDOWEVENT_CLOSE &&
         native->native.window.windowID == value_of(target_window));
}
}  // namespace vulkandemo::sdl
