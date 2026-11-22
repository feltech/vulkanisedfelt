// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/event_types.hpp
 *
 * Owned snapshots of SDL events and explicit unsupported-event metadata.
 */
#pragma once

#include <cstdint>
#include <variant>

#include <SDL_events.h>

#include "./managed_text_types.hpp"
#include "./window_id_types.hpp"

namespace vulkandemo::sdl
{
/** A complete native snapshot whose active payload contains no borrowed pointers. */
struct NativeEvent
{
    /** Known pointer-free SDL event and its active payload. */
    SDL_Event native{};
};

/** Drag-and-drop event metadata with shared ownership of any SDL text. */
struct DropEvent
{
    /** Drop file, text, begin or completion event type. */
    SDL_EventType type;
    /** SDL timestamp in milliseconds. */
    std::uint32_t timestamp;
    /** Target window, or zero when no window is associated. */
    window_id window;
    /** Original SDL text allocation, empty for begin/completion events. */
    text_ptr text;
};

/** Extended text composition with shared ownership of its SDL text. */
struct ExtendedTextEditingEvent
{
    /** SDL timestamp in milliseconds. */
    std::uint32_t timestamp;
    /** Window receiving composition input. */
    window_id window;
    /** Beginning of the selected editing text. */
    std::int32_t start;
    /** Length of the selected editing text. */
    std::int32_t length;
    /** Original SDL text allocation, or empty when unavailable. */
    text_ptr text;
};

/** Metadata for system, user and unknown events without borrowing their payloads. */
struct UnsupportedEvent
{
    /** Original event type, including registered application event values. */
    std::uint32_t type;
    /** SDL timestamp in milliseconds. */
    std::uint32_t timestamp;
};

/** Owned polled event; application-owned user payloads are never adopted or released. */
using event = std::variant<NativeEvent, DropEvent, ExtendedTextEditingEvent, UnsupportedEvent>;
}  // namespace vulkandemo::sdl
