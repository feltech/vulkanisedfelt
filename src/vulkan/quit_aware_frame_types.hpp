// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/quit_aware_frame_types.hpp
 *
 * Completed frame and its unconsumed application events.
 */
#pragma once

#include <immer/array.hpp>

#include "../sdl/event_types.hpp"
#include "./errors.hpp"
#include "./submit_and_present_types.hpp"

namespace vulkandemo::vulkan
{
/** Completed presentation with its collected input available for further handling. */
struct QuitAwareFrame
{
    /** Presentation whose queued uses are complete. */
    CompletedPresentation presentation;
    /** Input events in polling order, including non-quit window events. */
    immer::array<sdl::event> events;
};

/** Completed frame, cancellation at the quit checkpoint, or a Vulkan failure. */
using quit_aware_frame_result = vulkan_result<QuitAwareFrame>;
}  // namespace vulkandemo::vulkan
