// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_handles_types.hpp
 *
 * Shared ownership types for SDL resources.
 */
#pragma once

#include <memory>

#include <SDL_video.h>

#include "../execution/managed_resources_types.hpp"

namespace vulkandemo::sdl
{
/** Shared ownership of a subsystem's cleanup. */
using video_keepalive = std::shared_ptr<execution::resource_guard>;

/** Shared main-thread window ownership. */
using window_ptr = std::shared_ptr<SDL_Window>;
}  // namespace vulkandemo::sdl
