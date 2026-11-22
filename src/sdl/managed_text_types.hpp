// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_text_types.hpp
 *
 * Shared immutable ownership of SDL-allocated text.
 */
#pragma once

#include <memory>

namespace vulkandemo::sdl
{
/** Shared ownership of terminated SDL text without copying its bytes. */
using text_ptr = std::shared_ptr<char const>;
}  // namespace vulkandemo::sdl
