// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/pending_events_types.hpp
 *
 * Bounds for one SDL event batch.
 */
#pragma once

#include <cstddef>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::sdl
{
struct EventBatchLimitTag;
/** Maximum number of events consumed by one collection operation. */
using event_batch_limit = strong::type<std::size_t, EventBatchLimitTag, strong::regular>;
}  // namespace vulkandemo::sdl
