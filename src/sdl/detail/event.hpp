// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/event.hpp
 *
 * Main-thread event polling and ownership of SDL payloads.
 */
#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include <SDL_events.h>
#include <fnfelt/monad/io.hpp>
#include <libfork/core/ext/context.hpp>

#include "../../backend/detail/api.hpp"
#include "../../execution/main_thread.hpp"
#include "../event_types.hpp"
#include "../managed_text.hpp"
#include "../window_id_types.hpp"

namespace vulkandemo::sdl::detail
{
/** Commands and main-thread dispatch needed to own a polled event. */
template <class TEnvironment>
concept sdl_poll_event_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment const & environment, SDL_Event * output, void * memory)
{
    {environment.main_context}->std::convertible_to<lf::worker_context *>;
    {environment.api.get().sdl_poll_event(output)}->std::same_as<int>;
    {environment.api.get().sdl_free(memory)}->std::same_as<void>;
};

/**
 * Identifies known SDL events whose active payload contains no pointers.
 *
 * @param type Native event type.
 * @return Whether its complete payload can be retained by value.
 */
auto has_pointer_free_payload(Uint32 type) noexcept -> bool;

/**
 * Adopts owned native payloads and snapshots all other supported event data.
 *
 * @param api Borrowed SDL implementation alive through returned text ownership.
 * @param native Retrieved native event whose text ownership is transferred.
 * @return Owned event snapshot.
 */
template <class TApi>
auto own_event(std::reference_wrapper<TApi> api, SDL_Event native) -> event
{
    switch (native.type)
    {
        case SDL_DROPFILE:
        case SDL_DROPTEXT:
        case SDL_DROPBEGIN:
        case SDL_DROPCOMPLETE:
            return DropEvent{
                static_cast<SDL_EventType>(native.type),
                native.drop.timestamp,
                window_id{native.drop.windowID},
                make_text_ptr(native.drop.file, api.get())};
        case SDL_TEXTEDITING_EXT:
            return ExtendedTextEditingEvent{
                native.editExt.timestamp,
                window_id{native.editExt.windowID},
                native.editExt.start,
                native.editExt.length,
                make_text_ptr(native.editExt.text, api.get())};
        default:
            if (has_pointer_free_payload(native.type))
            {
                return NativeEvent{native};
            }
            return UnsupportedEvent{native.type, native.common.timestamp};
    }
}

/**
 * Retrieves and owns one event on main.
 *
 * SDL event processing must remain initialized throughout execution.
 *
 * @param api Borrowed SDL implementation alive through returned text ownership.
 * @return One owned event, or an empty optional when no event is pending.
 */
template <class TApi>
auto poll_event_io_action(std::reference_wrapper<TApi> api)
    -> std::optional<event> pre(execution::is_main_thread())
{
    SDL_Event native{};
    int status = api.get().sdl_poll_event(&native);
    contract_assert(status == 0 || status == 1);
    if (status == 0)
    {
        return std::nullopt;
    }
    return own_event(api, native);
}

/**
 * Provides single-event polling on main.
 *
 * @param environment Borrowed commands and serviced main context.
 * @return IO providing an optional owned event.
 */
template <class TEnvironment>
constexpr auto poll_event_reader_io_action(TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Action providing one owned event from SDL's global queue.
    auto poll_event = std::bind_front(poll_event_io_action<api_type>, environment.api);

    // IO providing an optional event from SDL's main thread.
    return io::on<"sdl_poll_event_on_main"_ss>(
        environment.main_context, io::create<"sdl_poll_event"_ss>(std::move(poll_event)));
}
}  // namespace vulkandemo::sdl::detail
