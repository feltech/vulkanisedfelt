// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/pending_events.hpp
 *
 * Bounded event collection on SDL's main thread.
 */
#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include <SDL_events.h>
#include <fnfelt/monad/io.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../execution/main_thread.hpp"
#include "../event_types.hpp"
#include "../pending_events_types.hpp"
#include "./event.hpp"

namespace vulkandemo::sdl::detail
{
/** Commands and dispatch needed for bounded event collection. */
template <class TEnvironment>
concept sdl_collect_pending_events_environment = sdl_poll_event_environment<TEnvironment>;

/**
 * Collects a bounded FIFO batch on main.
 *
 * @param api Borrowed SDL implementation alive through returned text ownership.
 * @param limit Maximum number of events to consume.
 * @return Owned events in polling order.
 */
template <class TApi>
auto collect_pending_events_io_action(std::reference_wrapper<TApi> api, event_batch_limit limit)
    -> immer::array<event> pre(execution::is_main_thread())
{
    auto events = immer::array<event>{}.transient();
    while (events.size() < value_of(limit))
    {
        SDL_Event native{};
        int status = api.get().sdl_poll_event(&native);
        contract_assert(status == 0 || status == 1);
        if (status == 0)
        {
            break;
        }
        events.push_back(own_event(api, native));
    }
    return std::move(events).persistent();
}

/**
 * Provides bounded event collection on main.
 *
 * @param limit Maximum number of events to consume.
 * @param environment Borrowed commands and serviced main context.
 * @return IO providing an immutable event batch.
 */
template <class TEnvironment>
constexpr auto collect_pending_events_reader_io_action(
    event_batch_limit limit, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Action providing a bounded owned snapshot of SDL's pending events.
    auto collect_events =
        std::bind_front(collect_pending_events_io_action<api_type>, environment.api, limit);

    // IO providing the batch after a single main-thread dispatch.
    return io::on<"collect_sdl_events_on_main"_ss>(
        environment.main_context, io::create<"collect_sdl_events"_ss>(std::move(collect_events)));
}
}  // namespace vulkandemo::sdl::detail
