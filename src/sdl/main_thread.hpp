// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/main_thread.hpp
 *
 * Identification of SDL's application main thread.
 */
#pragma once

namespace vulkandemo::sdl
{
/** @return Whether execution is on the application main thread. */
[[nodiscard]] auto is_main_thread() noexcept -> bool;
}  // namespace vulkandemo::sdl
