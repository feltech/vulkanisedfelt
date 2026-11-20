// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/drawable_size_types.hpp
 *
 * Live drawable dimensions in pixels.
 */
#pragma once

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::sdl
{
struct DrawableWidthTag;
struct DrawableHeightTag;
/** Horizontal drawable pixel count. */
using drawable_width = strong::type<int, DrawableWidthTag, strong::regular>;
/** Vertical drawable pixel count. */
using drawable_height = strong::type<int, DrawableHeightTag, strong::regular>;

/** Pixel dimensions, including a zero area when no drawable is available. */
struct DrawableSize
{
    /** Horizontal pixel count. */
    drawable_width width;
    /** Vertical pixel count. */
    drawable_height height;
};
}  // namespace vulkandemo::sdl
