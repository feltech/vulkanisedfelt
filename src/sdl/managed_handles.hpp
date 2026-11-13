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

#include <SDL_video.h>
#include <libfork/core/defer.hpp>

#include "./main_thread.hpp"

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

/** Shared main-thread window ownership. */
using window_ptr = std::shared_ptr<SDL_Window>;

/**
 * Owns a window whose final release destroys it on main.
 *
 * Video remains initialized until the window is destroyed. The borrowed API must
 * outlive every owner copy.
 *
 * @tparam TApi Command implementation type.
 * @param window Acquired window.
 * @param api Borrowed SDL implementation.
 * @param video Shared video subsystem lifetime.
 * @return Shared window owner.
 */
template <class TApi>
auto make_window_ptr(SDL_Window * window, TApi & api, video_keepalive video) -> window_ptr
{
    return window_ptr{
        window,
        [&api, video = std::move(video)](SDL_Window * owned) mutable noexcept
        {
            contract_assert(is_main_thread());
            api.sdl_destroy_window(owned);
            // Weak window observers must not extend the video subsystem's lifetime.
            video.reset();
        }};
}
}  // namespace vulkandemo::sdl
