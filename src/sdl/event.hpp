// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/event.hpp
 *
 * Deferred single-event polling on SDL's main thread.
 */
#pragma once

#include <fnfelt/monad/reader_io.hpp>

#include "detail/event.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans retrieval of one owned event from the global SDL queue.
 *
 * Pointer-free events retain their native payloads. SDL text allocations are shared
 * without copying; the borrowed API must outlive every returned text owner.
 * System, application-user and unknown event payloads are represented by metadata
 * only and are never adopted or freed. Polling does not drain or discard later events.
 * SDL event processing must remain initialized throughout execution.
 *
 * @tparam TEnvironment Execution capabilities.
 * @return Reusable ReaderIO providing an event, or an empty optional for an empty queue.
 */
template <detail::sdl_poll_event_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_poll_event()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing an owned snapshot without consuming the rest of the queue.
    return reader_io::create<"sdl_poll_event"_ss, TEnvironment>(
        detail::poll_event_reader_io_action<TEnvironment>);
}
}  // namespace vulkandemo::sdl
