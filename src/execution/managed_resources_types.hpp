// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file execution/managed_resources_types.hpp
 *
 * Cleanup storage for shared resources.
 */
#pragma once

#include <functional>

#include <libfork/core/defer.hpp>

namespace vulkandemo::execution
{
/** Owned cleanup that cannot propagate failures during destruction. */
using resource_cleanup = std::move_only_function<void() noexcept>;

/** Shared cleanup storage for resources without a pointer. */
using resource_guard = lf::defer<resource_cleanup>;
}  // namespace vulkandemo::execution
