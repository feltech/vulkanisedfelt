// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/window_id.hpp
 *
 * Main-thread SDL window-identifier queries.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL_video.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../../execution/main_thread.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../window_id_types.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl::detail
{
/** Commands and main-thread dispatch needed to identify a window. */
template <class TEnvironment>
concept sdl_get_window_id_environment = main_thread_environment<TEnvironment> &&
    requires(TEnvironment const & environment, SDL_Window * window)
{
    {environment.api.get().sdl_get_window_id(window)}->std::same_as<Uint32>;
};

/**
 * Queries the identifier of an owned window on main.
 *
 * @param api Borrowed SDL implementation.
 * @param window Retained SDL window.
 * @return Nonzero identifier or an owned SDL failure.
 */
template <class TApi>
auto get_window_id_io_action(std::reference_wrapper<TApi> api, window_ptr window)
    -> sdl_result<window_id> pre(execution::is_main_thread()) pre(window != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<window_id>;

    Uint32 identifier = api.get().sdl_get_window_id(window.get());
    if (identifier == 0)
    {
        return result::error<result_type>(last_failure(api, FallibleSdlOperation::get_window_id));
    }
    return result::success<result_type>(window_id{identifier});
}

/**
 * Provides a main-thread window-identifier query.
 *
 * @param window Retained SDL window.
 * @param environment Borrowed commands and serviced main context.
 * @return IO providing the window's identifier.
 */
template <class TEnvironment>
constexpr auto get_window_id_reader_io_action(window_ptr window, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Action providing an identifier while retaining the window and its video parent.
    auto query_identifier =
        std::bind_front(get_window_id_io_action<api_type>, environment.api, std::move(window));

    // IO providing the identifier or an error copied on SDL's main thread.
    return io::on<"sdl_window_id_on_main"_ss>(
        environment.main_context, io::create<"sdl_get_window_id"_ss>(std::move(query_identifier)));
}
}  // namespace vulkandemo::sdl::detail
