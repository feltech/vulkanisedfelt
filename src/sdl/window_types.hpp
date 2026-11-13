// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/window_types.hpp
 *
 * Owned SDL window configuration and distinct dimensions.
 */
#pragma once

#include <SDL_video.h>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "../text/string.hpp"

namespace vulkandemo::sdl
{
struct WindowWidthTag;
/** Horizontal window size in screen coordinates. */
using window_width = strong::type<int, WindowWidthTag, strong::regular>;

struct WindowHeightTag;
/** Vertical window size in screen coordinates. */
using window_height = strong::type<int, WindowHeightTag, strong::regular>;

/** Owned configuration for a centered SDL window. */
struct WindowOptions
{
    /** Owned UTF-8 window title. */
    text::String title{"vulkanisedfelt"};
    /** Requested horizontal size. */
    sdl::window_width width{1280};
    /** Requested vertical size. */
    sdl::window_height height{720};
    /** SDL creation flags, including Vulkan support by default. */
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
};
}  // namespace vulkandemo::sdl
