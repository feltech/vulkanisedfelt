// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/resize_event.hpp
 *
 * Pure window resize-event classification for one SDL window.
 */
#pragma once

#include <variant>

#include <SDL_events.h>

#include "./event_types.hpp"
#include "./window_id_types.hpp"

namespace vulkandemo::sdl
{
/**
 * Identifies a resize or size-changed event for the target window.
 *
 * Other event data remains available for independent handling.
 *
 * @param polled_event Owned event snapshot.
 * @param target_window Window whose resize events should be matched.
 * @return Whether the event reports a size change on @p target_window.
 */
[[nodiscard]] constexpr auto is_window_resize(
    event const & polled_event, window_id target_window) noexcept -> bool
{
    NativeEvent const * native = std::get_if<NativeEvent>(&polled_event);
    if (!native)
    {
        return false;
    }
    return native->native.type == SDL_WINDOWEVENT &&
        native->native.window.windowID == value_of(target_window) &&
        (native->native.window.event == SDL_WINDOWEVENT_RESIZED ||
         native->native.window.event == SDL_WINDOWEVENT_SIZE_CHANGED);
}
}  // namespace vulkandemo::sdl
