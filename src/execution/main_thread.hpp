// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file execution/main_thread.hpp
 *
 * Identification of the application main thread.
 */
#pragma once

namespace vulkandemo::execution
{
/** @return Whether execution is on the application main thread. */
[[nodiscard]] auto is_main_thread() noexcept -> bool;
}  // namespace vulkandemo::execution
