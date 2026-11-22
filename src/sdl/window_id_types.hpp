// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/window_id_types.hpp
 *
 * Distinct identifiers for SDL windows.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::sdl
{
struct WindowIdTag;
/** Identifies a live SDL window. */
using window_id = strong::type<std::uint32_t, WindowIdTag, strong::regular>;
}  // namespace vulkandemo::sdl
