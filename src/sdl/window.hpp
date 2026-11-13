// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/window.hpp
 *
 * Deferred SDL window creation with main-owned lifetime.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/monad/state_io.hpp>

#include "./window_types.hpp"
#include "detail/window.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans creation of a centered window after video initialization.
 *
 * The immutable state box retains window ownership in sdl_resources.
 * Keep a state snapshot alive until borrowed window users finish; the borrowed
 * API and serviced main context must outlive every resource owner.
 *
 * @tparam TState Owned execution state.
 * @param options Owned title, dimensions and SDL flags.
 * @return StateIO producing a borrowed window or an owned SDL failure.
 */
template <detail::sdl_create_window_state TState>
[[nodiscard]] constexpr auto state_io_sdl_create_window(WindowOptions options = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    namespace state_io = fnfelt::monad::state_io;

    // Factory providing main-thread window-acquisition IO.
    auto io_create_window = std::bind_front(detail::io_create_window<TState>, std::move(options));

    // Read-only acquisition provides an owner before StateIO retains it in a new snapshot.
    auto acquire_window =
        reader_io::create<"sdl_create_window"_ss, TState>(std::move(io_create_window));
    auto retain_window = state_io::lift<TState>(std::move(acquire_window))
                             .template store<"retain_sdl_window"_ss>(detail::store_window<TState>);

    return state_io::transform<"sdl_window_view"_ss>(
        std::move(retain_window), detail::window_handle);
}
}  // namespace vulkandemo::sdl
