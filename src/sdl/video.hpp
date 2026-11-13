// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/video.hpp
 *
 * Deferred main-thread SDL video initialization.
 */
#pragma once

#include <fnfelt/monad/reader_io.hpp>

#include "detail/video.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans video initialization with ownership retained on main.
 *
 * The environment borrows api and sdl_resources through reference wrappers and
 * supplies main_context. Keep the API, holder and context alive through execution;
 * destroy the holder on main after all worker borrowers finish.
 *
 * @tparam TEnvironment Execution capabilities.
 * @return ReaderIO completing initialization or producing an owned SDL failure.
 */
template <sdl_initialize_video_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_initialize_video()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing main-thread video-acquisition IO.
    auto io_initialize_video = detail::io_initialize_video<TEnvironment>;

    // ReaderIO providing video initialization with main-owned cleanup.
    return reader_io::create<"sdl_initialize_video"_ss, TEnvironment>(io_initialize_video);
}
}  // namespace vulkandemo::sdl
