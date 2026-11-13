// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/resources.hpp
 *
 * Main-owned anchors for borrowed SDL resource views.
 */
#pragma once

#include "./managed_handles.hpp"

namespace vulkandemo::sdl
{
/**
 * Keeps SDL resource ownership on the constructing main thread.
 *
 * Use a fresh holder for each run. Borrowers must finish before destruction, and
 * the borrowed API must outlive every shared owner. Final releases occur on main.
 * Do not read resource views concurrently with acquisition.
 */
struct SdlResources
{
    /** Main-thread video subsystem ownership. */
    video_keepalive video_owner;
};
}  // namespace vulkandemo::sdl
