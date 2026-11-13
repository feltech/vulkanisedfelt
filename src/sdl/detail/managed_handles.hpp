// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/managed_handles.hpp
 *
 * Main-thread window destruction with ordered video cleanup.
 */
#pragma once

#include <functional>
#include <memory>

#include <SDL_video.h>

#include "../../execution/main_thread.hpp"
#include "../../execution/managed_resources_types.hpp"

namespace vulkandemo::sdl::detail
{
/**
 * Destroys a window before releasing its video dependency.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed SDL implementation.
 * @param video Retained video subsystem ownership.
 * @param window Acquired window.
 */
template <class TApi>
void destroy_window(
    std::reference_wrapper<TApi> api,
    std::shared_ptr<execution::resource_guard> & video,
    SDL_Window * window) noexcept pre(execution::is_main_thread())
{
    api.get().sdl_destroy_window(window);
    // Weak window observers must not extend the video subsystem's lifetime.
    video.reset();
}
}  // namespace vulkandemo::sdl::detail
