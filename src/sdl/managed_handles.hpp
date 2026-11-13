// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_handles.hpp
 *
 * Shared ownership with main-thread cleanup for SDL resources.
 */
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <SDL_video.h>

#include "../execution/managed_resources.hpp"
#include "./managed_handles_types.hpp"
#include "detail/managed_handles.hpp"

namespace vulkandemo::sdl
{
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
 * @param main_context Serviced main-thread context alive through final release.
 * @return Shared window owner.
 */
template <class TApi>
auto make_window_ptr(
    SDL_Window * window,
    TApi & api,
    video_keepalive video,
    lf::worker_context * main_context) noexcept -> window_ptr
{
    return execution::make_main_thread_ptr(
        window,
        main_context,
        std::bind_front(detail::destroy_window<TApi>, std::ref(api), std::move(video)));
}
}  // namespace vulkandemo::sdl
