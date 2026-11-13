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

#include "./window_types.hpp"
#include "detail/window.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans creation of a centered window after video initialization.
 *
 * Use a fresh main-owned holder for each run and retain it until all borrowed
 * window users finish. The borrowed API must outlive the holder.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param options Owned title, dimensions and SDL flags.
 * @return ReaderIO producing a borrowed window or an owned SDL failure.
 */
template <sdl_create_window_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_create_window(WindowOptions options = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing main-thread window-acquisition IO.
    auto io_create_window =
        std::bind_front(detail::io_create_window<TEnvironment>, std::move(options));

    // ReaderIO providing a window view tied to the main-owned holder.
    return reader_io::create<"sdl_create_window"_ss, TEnvironment>(std::move(io_create_window));
}
}  // namespace vulkandemo::sdl
