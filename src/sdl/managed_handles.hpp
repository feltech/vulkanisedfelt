// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_handles.hpp
 *
 * Main-owned cleanup guards for SDL resources.
 */
#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <libfork/core/defer.hpp>

namespace vulkandemo::sdl
{
/** Owned cleanup that cannot propagate failures during destruction. */
using resource_cleanup = std::move_only_function<void() noexcept>;

/** Immovable guard running a successfully acquired resource's cleanup. */
using resource_guard = lf::defer<resource_cleanup>;

/** Shared ownership of a subsystem's cleanup. */
using video_keepalive = std::shared_ptr<resource_guard>;

/**
 * Shares cleanup until its final owner is released.
 *
 * Allocation failure is terminal.
 *
 * @tparam TCleanup Cleanup callable type.
 * @param cleanup Action performed on final release.
 * @return Shared deferred cleanup.
 */
template <class TCleanup>
auto make_deferred(TCleanup && cleanup) noexcept
{
    using guard_type = lf::defer<std::decay_t<TCleanup>>;
    return std::make_shared<guard_type>(std::forward<TCleanup>(cleanup));
}

}  // namespace vulkandemo::sdl
