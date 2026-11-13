// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/errors.hpp
 *
 * Owned failures from SDL operations.
 */
#pragma once

#include <fnfelt/result.hpp>
#include <immer/array.hpp>

namespace vulkandemo::sdl
{
/** Fallible SDL operations in the migrated platform boundary. */
enum class FallibleSdlOperation
{
    /** Video subsystem initialization. */
    initialize_video,
    /** Window creation. */
    create_window
};

/** An SDL failure with an owned copy of the calling thread's error text. */
struct SdlFailure
{
    /** Operation that reported the failure. */
    FallibleSdlOperation operation;
    /** Owned error bytes without a trailing NUL. */
    immer::array<char> message;
};

/** Common outcome of SDL operations. */
template <class T>
using sdl_result = fnfelt::Result<T, SdlFailure>;
}  // namespace vulkandemo::sdl
