// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/video.hpp
 *
 * Deferred main-thread SDL video initialization.
 */
#pragma once

#include <utility>

#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/monad/state_io.hpp>

#include "detail/video.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans video initialization with ownership retained in updated state.
 *
 * The immutable state box borrows api and supplies main_context and sdl_resources.
 * The API and serviced context must outlive every resource owner.
 *
 * @tparam TState Owned execution state.
 * @return StateIO completing initialization or producing an owned SDL failure.
 */
template <detail::sdl_initialize_video_state TState>
[[nodiscard]] constexpr auto state_io_sdl_initialize_video()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    namespace state_io = fnfelt::monad::state_io;

    // Read-only acquisition provides an owner before StateIO retains it in a new snapshot.
    auto acquire_video = reader_io::create<"sdl_initialize_video"_ss, TState>(
        detail::initialize_video_reader_io_action<TState>);
    auto retain_video = state_io::lift<TState>(std::move(acquire_video))
                            .template store<"retain_sdl_video"_ss>(detail::store_video<TState>);

    // State already retains the owner; discard the extra value before subsequent work.
    return state_io::transform<"sdl_video_initialized"_ss>(
        std::move(retain_video), [](auto const &) {});
}
}  // namespace vulkandemo::sdl
