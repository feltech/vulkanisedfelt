// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/window_id.hpp
 *
 * Deferred discovery of SDL window identifiers.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/window_id.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans discovery of the retained window's identifier on main.
 *
 * The borrowed API and serviced main context must outlive execution and cleanup.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Nonempty SDL window owner.
 * @return Reusable ReaderIO providing its identifier or an owned SDL failure.
 */
template <detail::sdl_get_window_id_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_get_window_id(window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing the window-identifier query with retained ownership.
    auto get_window_id =
        std::bind_front(detail::get_window_id_reader_io_action<TEnvironment>, std::move(window));

    // ReaderIO providing the identifier from SDL's main thread.
    return reader_io::create<"sdl_get_window_id"_ss, TEnvironment>(std::move(get_window_id));
}
}  // namespace vulkandemo::sdl
