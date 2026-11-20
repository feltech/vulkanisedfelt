// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/drawable_size.hpp
 *
 * Deferred Vulkan drawable-size discovery.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/drawable_size.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans a live snapshot of the window's Vulkan drawable dimensions in pixels.
 *
 * The retained window must have SDL_WINDOW_VULKAN. The borrowed API and serviced main context
 * must outlive execution and cleanup. Zero dimensions are valid, for example while minimized.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Nonempty Vulkan-capable window owner.
 * @return Reusable ReaderIO providing current pixel dimensions on each execution.
 */
template <detail::sdl_vulkan_get_drawable_size_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_vulkan_get_drawable_size(window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing main-thread drawable-size IO with retained window ownership.
    auto get_drawable_size = std::bind_front(
        detail::get_drawable_size_reader_io_action<TEnvironment>, std::move(window));

    // ReaderIO providing pixels independently of the window's screen-coordinate size.
    return reader_io::create<"sdl_vulkan_get_drawable_size"_ss, TEnvironment>(
        std::move(get_drawable_size));
}
}  // namespace vulkandemo::sdl
