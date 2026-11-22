// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/pending_events.hpp
 *
 * Deferred bounded SDL event collection.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./pending_events_types.hpp"
#include "detail/pending_events.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans collection of pending events in FIFO order.
 *
 * Stops on an empty poll or after limit events, leaving the remainder queued.
 * A zero limit consumes nothing. SDL event processing must remain initialized throughout
 * execution. The borrowed API must outlive all returned text owners; its allocator must
 * support their final release thread. Unsupported payloads retain metadata only.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param limit Maximum number of events to consume.
 * @return ReaderIO providing an immutable owned batch.
 */
template <detail::sdl_collect_pending_events_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_collect_pending_events(event_batch_limit limit)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing a main-thread event batch bounded independently of queue producers.
    auto collect_events =
        std::bind_front(detail::collect_pending_events_reader_io_action<TEnvironment>, limit);
    return reader_io::create<"collect_sdl_events"_ss, TEnvironment>(std::move(collect_events));
}
}  // namespace vulkandemo::sdl
