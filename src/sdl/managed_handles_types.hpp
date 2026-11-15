// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_handles_types.hpp
 *
 * Shared ownership types for SDL resources.
 */
#pragma once

#include <memory>

#include "../execution/managed_resources_types.hpp"

namespace vulkandemo::sdl
{
/** Shared ownership of a subsystem's cleanup. */
using video_keepalive = std::shared_ptr<execution::resource_guard>;
}  // namespace vulkandemo::sdl
