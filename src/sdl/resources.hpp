// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/resources.hpp
 *
 * RAII-managed SDL resources.
 */
#pragma once

#include "./managed_handles_types.hpp"

namespace vulkandemo::sdl
{
/**
 * Collects shared ownership of SDL resources.
 *
 * State snapshots share these owners through immutable boxed storage. Borrowers
 * must finish before the final owner releases its resource, and the borrowed API
 * and serviced main context must outlive every shared owner.
 * Final release waits for cleanup on main, including when invoked by a worker.
 */
struct SdlResources
{
    /** Main-thread video subsystem ownership. */
    video_keepalive video_owner;
    /** Main-thread window ownership. */
    window_ptr window_owner;
};
}  // namespace vulkandemo::sdl
